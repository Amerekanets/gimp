/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <stdlib.h>

#include <glib-object.h>

#include "libgimpcolor/gimpcolor.h"

#include "core-types.h"

#include "base/pixel-region.h"
#include "base/tile.h"
#include "base/tile-manager.h"

#include "gimpchannel.h"
#include "gimpimage.h"
#include "gimpimage-contiguous-region.h"
#include "gimpimage-projection.h"


/*  local function prototypes  */

static gint pixel_difference              (guchar       *col1,
                                           guchar       *col2,
                                           gboolean      antialias, 
                                           gint          threshold, 
                                           gint          bytes,
                                           gboolean      has_alpha,
                                           gboolean      select_transparent);
static void ref_tiles                     (TileManager  *src, 
                                           TileManager  *mask, 
                                           Tile        **s_tile, 
                                           Tile        **m_tile,
                                           gint          x, 
                                           gint          y, 
                                           guchar      **s, 
                                           guchar      **m);
static gint find_contiguous_segment       (guchar       *col, 
                                           PixelRegion  *src,
                                           PixelRegion  *mask, 
                                           gint          width, 
                                           gint          bytes,
                                           gboolean      has_alpha,
                                           gboolean      select_transparent,
                                           gboolean      antialias,
                                           gint          threshold,
                                           gint          initial, 
                                           gint         *start, 
                                           gint         *end);
static void find_contiguous_region_helper (PixelRegion  *mask, 
                                           PixelRegion  *src,
                                           gboolean      has_alpha, 
                                           gboolean      select_transparent,
                                           gboolean      antialias, 
                                           gint          threshold,
                                           gint          x, 
                                           gint          y, 
                                           guchar       *col);


/*  public functions  */

GimpChannel *
gimp_image_contiguous_region_by_seed (GimpImage    *gimage, 
                                      GimpDrawable *drawable,
                                      gboolean      sample_merged,
                                      gboolean      antialias,
                                      gint          threshold,
                                      gboolean      select_transparent,
                                      gint          x,
                                      gint          y)
{
  PixelRegion  srcPR, maskPR;
  GimpChannel *mask;
  guchar      *start;
  gboolean     has_alpha;
  gint         bytes;
  Tile        *tile;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);
  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), NULL);

  if (sample_merged)
    {
      pixel_region_init (&srcPR, gimp_image_projection (gimage), 0, 0,
			 gimage->width, gimage->height, FALSE);

      has_alpha =
        GIMP_IMAGE_TYPE_HAS_ALPHA (gimp_image_projection_type (gimage));

      bytes = gimp_image_projection_bytes (gimage);
    }
  else
    {
      pixel_region_init (&srcPR, gimp_drawable_data (drawable),
			 0, 0,
			 gimp_drawable_width (drawable),
			 gimp_drawable_height (drawable),
			 FALSE);

      has_alpha = gimp_drawable_has_alpha (drawable);

      if (gimp_drawable_is_indexed (drawable))
        bytes = has_alpha ? 4 : 3;
      else
        bytes = gimp_drawable_bytes (drawable);
    }

  mask = gimp_channel_new_mask (gimage, srcPR.w, srcPR.h);
  pixel_region_init (&maskPR, gimp_drawable_data (GIMP_DRAWABLE (mask)),
		     0, 0, 
		     gimp_drawable_width (GIMP_DRAWABLE (mask)), 
		     gimp_drawable_height (GIMP_DRAWABLE (mask)), 
		     TRUE);

  tile = tile_manager_get_tile (srcPR.tiles, x, y, TRUE, FALSE);
  if (tile)
    {
      start = tile_data_pointer (tile, x % TILE_WIDTH, y % TILE_HEIGHT);

      if (has_alpha)
        {
          if (select_transparent)
            {
              /*  don't select transparent regions if the start pixel isn't
               *  fully transparent
               */
              if (start[bytes - 1] > 0)
                select_transparent = FALSE;
            }
        }
      else
        {
          select_transparent = FALSE;
        }

      find_contiguous_region_helper (&maskPR, &srcPR, has_alpha,
                                     select_transparent, antialias, threshold,
                                     x, y, start);

      tile_release (tile, FALSE);
    }

  return mask;
}

GimpChannel *
gimp_image_contiguous_region_by_color (GimpImage     *gimage,
                                       GimpDrawable  *drawable,
                                       gboolean       sample_merged,
                                       gboolean       antialias,
                                       gint           threshold,
                                       gboolean       select_transparent,
                                       const GimpRGB *color)
{
  /*  Scan over the gimage's active layer, finding pixels within the specified
   *  threshold from the given R, G, & B values.  If antialiasing is on,
   *  use the same antialiasing scheme as in fuzzy_select.  Modify the gimage's
   *  mask to reflect the additional selection
   */
  GimpChannel   *mask;
  PixelRegion    imagePR, maskPR;
  guchar        *image_data;
  guchar        *mask_data;
  guchar        *idata, *mdata;
  guchar         rgb[MAX_CHANNELS];
  gint           has_alpha, indexed;
  gint           width, height;
  gint           bytes, color_bytes, alpha;
  gint           i, j;
  gpointer       pr;
  GimpImageType  d_type;
  guchar         col[MAX_CHANNELS];

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);
  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), NULL);
  g_return_val_if_fail (color != NULL, NULL);

  gimp_rgba_get_uchar (color, &col[0], &col[1], &col[2], &col[3]);

  /*  Get the image information  */
  if (sample_merged)
    {
      bytes     = gimp_image_projection_bytes (gimage);
      d_type    = gimp_image_projection_type (gimage);
      has_alpha = GIMP_IMAGE_TYPE_HAS_ALPHA (d_type);
      indexed   = GIMP_IMAGE_TYPE_IS_INDEXED (d_type);
      width     = gimage->width;
      height    = gimage->height;

      pixel_region_init (&imagePR, gimp_image_projection (gimage),
			 0, 0,
                         width, height,
                         FALSE);
    }
  else
    {
      bytes     = gimp_drawable_bytes (drawable);
      d_type    = gimp_drawable_type (drawable);
      has_alpha = gimp_drawable_has_alpha (drawable);
      indexed   = gimp_drawable_is_indexed (drawable);
      width     = gimp_drawable_width (drawable);
      height    = gimp_drawable_height (drawable);

      pixel_region_init (&imagePR, gimp_drawable_data (drawable),
			 0, 0,
                         width, height,
                         FALSE);
    }

  if (has_alpha)
    {
      if (select_transparent)
        {
          /*  don't select transparancy if "color" isn't fully transparent
           */
          if (col[3] > 0)
            select_transparent = FALSE;
        }
    }
  else
    {
      select_transparent = FALSE;
    }

  if (indexed)
    {
      /* indexed colors are always RGB or RGBA */
      color_bytes = has_alpha ? 4 : 3;
    }
  else
    {
      /* RGB, RGBA, GRAY and GRAYA colors are shaped just like the image */
      color_bytes = bytes;
    }

  alpha = bytes - 1;
  mask = gimp_channel_new_mask (gimage, width, height);

  pixel_region_init (&maskPR, gimp_drawable_data (GIMP_DRAWABLE (mask)), 
		     0, 0,
                     width, height,
                     TRUE);

  /*  iterate over the entire image  */
  for (pr = pixel_regions_register (2, &imagePR, &maskPR);
       pr != NULL;
       pr = pixel_regions_process (pr))
    {
      image_data = imagePR.data;
      mask_data = maskPR.data;

      for (i = 0; i < imagePR.h; i++)
	{
	  idata = image_data;
	  mdata = mask_data;

	  for (j = 0; j < imagePR.w; j++)
	    {
	      /*  Get the rgb values for the color  */
	      gimp_image_get_color (gimage, d_type, rgb, idata);

	      /*  Plug the alpha channel in there  */
	      if (has_alpha)
		rgb[color_bytes - 1] = idata[alpha];

	      /*  Find how closely the colors match  */
	      *mdata++ = pixel_difference (col, rgb, 
                                           antialias, threshold,
                                           color_bytes, 
                                           has_alpha, select_transparent);

	      idata += bytes;
	    }

	  image_data += imagePR.rowstride;
	  mask_data += maskPR.rowstride;
	}
    }

  return mask;
}


/*  private functions  */

static gint
pixel_difference (guchar   *col1,
                  guchar   *col2,
                  gboolean  antialias,
                  gint      threshold,
                  gint      bytes,
                  gboolean  has_alpha,
                  gboolean  select_transparent)
{
  gint max = 0;

  /*  if there is an alpha channel, never select transparent regions  */
  if (! select_transparent && has_alpha && col2[bytes - 1] == 0)
    return 0;

  if (select_transparent && has_alpha)
    {
      max = abs (col1[bytes - 1] - col2[bytes - 1]);
    }
  else
    {
      gint diff;
      gint b;

      if (has_alpha)
        bytes--;

      for (b = 0; b < bytes; b++)
        {
          diff = abs (col1[b] - col2[b]);
          if (diff > max)
            max = diff;
        }
    }

  if (antialias && threshold > 0)
    {
      gfloat aa = 1.5 - ((gfloat) max / threshold);

      if (aa <= 0.0)
	return 0;
      else if (aa < 0.5)
	return (guchar) (aa * 512);
      else
	return 255;
    }
  else
    {
      if (max > threshold)
	return 0;
      else
	return 255;
    }
}

static void
ref_tiles (TileManager  *src, 
	   TileManager  *mask, 
	   Tile        **s_tile, 
	   Tile        **m_tile,
	   gint          x, 
	   gint          y, 
	   guchar      **s, 
	   guchar      **m)
{
  if (*s_tile != NULL)
    tile_release (*s_tile, FALSE);
  if (*m_tile != NULL)
    tile_release (*m_tile, TRUE);

  *s_tile = tile_manager_get_tile (src, x, y, TRUE, FALSE);
  *m_tile = tile_manager_get_tile (mask, x, y, TRUE, TRUE);

  *s = tile_data_pointer (*s_tile, x % TILE_WIDTH, y % TILE_HEIGHT);
  *m = tile_data_pointer (*m_tile, x % TILE_WIDTH, y % TILE_HEIGHT);
}

static int
find_contiguous_segment (guchar      *col, 
			 PixelRegion *src,
			 PixelRegion *mask, 
			 gint         width, 
			 gint         bytes,
			 gboolean     has_alpha,
                         gboolean     select_transparent,
			 gboolean     antialias,
			 gint         threshold,
			 gint         initial, 
			 gint        *start, 
			 gint        *end)
{
  guchar *s;
  guchar *m;
  guchar  diff;
  Tile   *s_tile = NULL;
  Tile   *m_tile = NULL;

  ref_tiles (src->tiles, mask->tiles, 
             &s_tile, &m_tile, src->x, src->y, &s, &m);

  /* check the starting pixel */
  if (! (diff = pixel_difference (col, s, antialias, threshold, 
                                  bytes, has_alpha, select_transparent)))
    {
      tile_release (s_tile, FALSE);
      tile_release (m_tile, TRUE);
      return FALSE;
    }

  *m-- = diff;
  s -= bytes;
  *start = initial - 1;

  while (*start >= 0 && diff)
    {
      if (! ((*start + 1) % TILE_WIDTH))
	ref_tiles (src->tiles, mask->tiles, 
                   &s_tile, &m_tile, *start, src->y, &s, &m);

      diff = pixel_difference (col, s, antialias, threshold, 
                               bytes, has_alpha, select_transparent);

      if ((*m-- = diff))
	{
	  s -= bytes;
	  (*start)--;
	}
    }

  diff = 1;
  *end = initial + 1;

  if (*end % TILE_WIDTH && *end < width)
    ref_tiles (src->tiles, mask->tiles, 
               &s_tile, &m_tile, *end, src->y, &s, &m);

  while (*end < width && diff)
    {
      if (! (*end % TILE_WIDTH))
	ref_tiles (src->tiles, mask->tiles, 
                   &s_tile, &m_tile, *end, src->y, &s, &m);

      diff = pixel_difference (col, s, antialias, threshold, 
                               bytes, has_alpha, select_transparent);

      if ((*m++ = diff))
	{
	  s += bytes;
	  (*end)++;
	}
    }

  tile_release (s_tile, FALSE);
  tile_release (m_tile, TRUE);

  return TRUE;
}

static void
find_contiguous_region_helper (PixelRegion *mask, 
			       PixelRegion *src,
			       gboolean     has_alpha, 
                               gboolean     select_transparent,
			       gboolean     antialias, 
			       gint         threshold, 
			       gint         x, 
			       gint         y, 
			       guchar      *col)
{
  gint start, end, i;
  gint val;

  Tile *tile;

  if (x < 0 || x >= src->w) return;
  if (y < 0 || y >= src->h) return;

  tile = tile_manager_get_tile (mask->tiles, x, y, TRUE, FALSE);
  val = *(guchar *)(tile_data_pointer (tile, 
				       x % TILE_WIDTH, y % TILE_HEIGHT));
  tile_release (tile, FALSE);
  if (val != 0)
    return;

  src->x = x;
  src->y = y;

  if (! find_contiguous_segment (col, src, mask, src->w, src->bytes,
                                 has_alpha, select_transparent,
                                 antialias, threshold, 
                                 x, &start, &end))
    return;

  for (i = start + 1; i < end; i++)
    {
      find_contiguous_region_helper (mask, src, has_alpha, select_transparent,
                                     antialias, threshold,
                                     i, y - 1, col);
      find_contiguous_region_helper (mask, src, has_alpha, select_transparent,
                                     antialias, threshold,
                                     i, y + 1, col);
    }
}
