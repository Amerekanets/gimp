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

#include <string.h>

#include <glib-object.h>

#include "libgimpmath/gimpmath.h"

#include "core-types.h"

#include "base/pixel-region.h"
#include "base/temp-buf.h"
#include "base/tile-manager.h"
#include "base/tile.h"

#include "paint-funcs/paint-funcs.h"

#include "gimpchannel.h"
#include "gimpimage.h"
#include "gimpdrawable.h"
#include "gimplayer.h"
#include "gimppreviewcache.h"


/*  local function prototypes  */

static TempBuf * gimp_drawable_preview_private (GimpDrawable      *drawable,
						gint               width,
						gint               height);
static void      gimp_drawable_preview_scale   (GimpImageBaseType  type,
						guchar            *cmap,
						PixelRegion       *srcPR,
						PixelRegion       *destPR,
						gint               subsample);


/*  public functions  */

TempBuf *
gimp_drawable_get_preview (GimpViewable *viewable,
			   gint          width,
			   gint          height)
{
  GimpDrawable *drawable;

  drawable = GIMP_DRAWABLE (viewable);

  /* Ok prime the cache with a large preview if the cache is invalid */
  if (! drawable->preview_valid                            &&
      width  <= PREVIEW_CACHE_PRIME_WIDTH                  &&
      height <= PREVIEW_CACHE_PRIME_HEIGHT                 &&
      drawable->gimage                                     &&
      drawable->gimage->width  > PREVIEW_CACHE_PRIME_WIDTH &&
      drawable->gimage->height > PREVIEW_CACHE_PRIME_HEIGHT)
    {
      TempBuf *tb = gimp_drawable_preview_private (drawable,
						   PREVIEW_CACHE_PRIME_WIDTH,
						   PREVIEW_CACHE_PRIME_HEIGHT);

      /* Save the 2nd call */
      if (width  == PREVIEW_CACHE_PRIME_WIDTH &&
	  height == PREVIEW_CACHE_PRIME_HEIGHT)
	return tb;
    }

  /* Second call - should NOT visit the tile cache...*/
  return gimp_drawable_preview_private (drawable, width, height);
}


/*  private functions  */

static TempBuf *
gimp_drawable_preview_private (GimpDrawable *drawable,
			       gint          width,
			       gint          height)
{
  TempBuf           *preview_buf;
  PixelRegion        srcPR;
  PixelRegion        destPR;
  GimpImageBaseType  type;
  gint               bytes;
  gint               subsample;
  TempBuf           *ret_buf;

  type  = RGB;
  bytes = 0;

  /*  The easy way  */
  if (drawable->preview_valid &&
      (ret_buf = gimp_preview_cache_get (&drawable->preview_cache, 
					 width, height)))
    {
      return ret_buf;
    }
  /*  The hard way  */
  else
    {
      switch (gimp_drawable_type (drawable))
	{
	case RGB_GIMAGE: case RGBA_GIMAGE:
	  type  = RGB;
	  bytes = gimp_drawable_bytes (drawable);
	  break;
	case GRAY_GIMAGE: case GRAYA_GIMAGE:
	  type  = GRAY;
	  bytes = gimp_drawable_bytes (drawable);
	  break;
	case INDEXED_GIMAGE: case INDEXEDA_GIMAGE:
	  type  = INDEXED;
	  bytes = (gimp_drawable_type (drawable) == INDEXED_GIMAGE) ? 3 : 4;
	  break;
	}

      /*  calculate 'acceptable' subsample  */
      subsample = 1;

      /* handle some truncation errors */
      if (width < 1) width = 1;
      if (height < 1) height = 1;

      while ((width  * (subsample + 1) * 2 < gimp_drawable_width  (drawable)) &&
	     (height * (subsample + 1) * 2 < gimp_drawable_height (drawable))) 
	subsample += 1;

      pixel_region_init (&srcPR, gimp_drawable_data (drawable),
			 0, 0,
			 gimp_drawable_width (drawable),
			 gimp_drawable_height (drawable),
			 FALSE);

      preview_buf = temp_buf_new (width, height, bytes, 0, 0, NULL);

      destPR.bytes     = preview_buf->bytes;
      destPR.x         = 0;
      destPR.y         = 0;
      destPR.w         = width;
      destPR.h         = height;
      destPR.rowstride = width * destPR.bytes;
      destPR.data      = temp_buf_data (preview_buf);

      if (GIMP_IS_LAYER (drawable))
	{
	  GimpImage *gimage;

	  gimage = drawable->gimage;

	  gimp_drawable_preview_scale (type, gimage->cmap,
				       &srcPR, &destPR,
				       subsample);
	}
      else if (GIMP_IS_CHANNEL (drawable))
	{
	  subsample_region (&srcPR, &destPR, subsample);
	}

      if (! drawable->preview_valid)
	gimp_preview_cache_invalidate (&drawable->preview_cache);

      drawable->preview_valid = TRUE;

      gimp_preview_cache_add (&drawable->preview_cache, preview_buf);

      return preview_buf;
    }
}

static void
gimp_drawable_preview_scale (GimpImageBaseType  type,
			     guchar            *cmap,
			     PixelRegion       *srcPR,
			     PixelRegion       *destPR,
			     gint               subsample)
{
#define EPSILON 0.000001
  guchar  *src, *s;
  guchar  *dest, *d;
  gdouble *row, *r;
  gint     destwidth;
  gint     src_row, src_col;
  gint     bytes, b;
  gint     width, height;
  gint     orig_width, orig_height;
  gdouble  x_rat, y_rat;
  gdouble  x_cum, y_cum;
  gdouble  x_last, y_last;
  gdouble *x_frac, y_frac, tot_frac;
  gint     i, j;
  gint     frac;
  gboolean advance_dest;
  guchar   rgb[MAX_CHANNELS];

  orig_width  = srcPR->w / subsample;
  orig_height = srcPR->h / subsample;
  width       = destPR->w;
  height      = destPR->h;

  /*  Some calculations...  */
  bytes     = destPR->bytes;
  destwidth = destPR->rowstride;

  /*  the data pointers...  */
  src  = g_new (guchar, orig_width * bytes);
  dest = destPR->data;

  /*  find the ratios of old x to new x and old y to new y  */
  x_rat = (gdouble) orig_width  / (gdouble) width;
  y_rat = (gdouble) orig_height / (gdouble) height;

  /*  allocate an array to help with the calculations  */
  row    = g_new (gdouble, width * bytes);
  x_frac = g_new (gdouble, width + orig_width);

  /*  initialize the pre-calculated pixel fraction array  */
  src_col = 0;
  x_cum = (gdouble) src_col;
  x_last = x_cum;

  for (i = 0; i < width + orig_width; i++)
    {
      if (x_cum + x_rat <= (src_col + 1 + EPSILON))
	{
	  x_cum += x_rat;
	  x_frac[i] = x_cum - x_last;
	}
      else
	{
	  src_col ++;
	  x_frac[i] = src_col - x_last;
	}
      x_last += x_frac[i];
    }

  /*  clear the "row" array  */
  memset (row, 0, sizeof (gdouble) * width * bytes);

  /*  counters...  */
  src_row = 0;
  y_cum = (double) src_row;
  y_last = y_cum;

  pixel_region_get_row (srcPR, 
			0, 
			src_row * subsample, 
			orig_width * subsample, 
			src, 
			subsample);

  /*  Scale the selected region  */
  for (i = 0; i < height; )
    {
      src_col = 0;
      x_cum = (gdouble) src_col;

      /* determine the fraction of the src pixel we are using for y */
      if (y_cum + y_rat <= (src_row + 1 + EPSILON))
	{
	  y_cum += y_rat;
	  y_frac = y_cum - y_last;
	  advance_dest = TRUE;
	}
      else
	{
	  src_row ++;
	  y_frac = src_row - y_last;
	  advance_dest = FALSE;
	}

      y_last += y_frac;

      s = src;
      r = row;

      frac = 0;
      j = width;

      while (j)
	{
	  tot_frac = x_frac[frac++] * y_frac;

	  /*  If indexed, transform the color to RGB  */
	  if (type == INDEXED)
	    {
	      map_to_color (2, cmap, s, rgb);

	      r[RED_PIX]   += rgb[RED_PIX] * tot_frac;
	      r[GREEN_PIX] += rgb[GREEN_PIX] * tot_frac;
	      r[BLUE_PIX]  += rgb[BLUE_PIX] * tot_frac;

	      if (bytes == 4)
		r[ALPHA_PIX] += s[ALPHA_I_PIX] * tot_frac;
	    }
	  else
	    {
	      for (b = 0; b < bytes; b++)
		r[b] += s[b] * tot_frac;
	    }

	  /*  increment the destination  */
	  if (x_cum + x_rat <= (src_col + 1 + EPSILON))
	    {
	      r += bytes;
	      x_cum += x_rat;
	      j--;
	    }
	  /* increment the source */
	  else
	    {
	      s += srcPR->bytes;
	      src_col++;
	    }
	}

      if (advance_dest)
	{
	  tot_frac = 1.0 / (x_rat * y_rat);

	  /*  copy "row" to "dest"  */
	  d = dest;
	  r = row;

	  j = width;
	  while (j--)
	    {
	      b = bytes;
	      while (b--)
		*d++ = (guchar) ((*r++ * tot_frac) + 0.5);
	    }

	  dest += destwidth;

	  /*  clear the "row" array  */
	  memset (row, 0, sizeof (gdouble) * destwidth);

	  i++;
	}
      else
	{
	  pixel_region_get_row (srcPR, 
				0, 
				src_row * subsample, 
				orig_width * subsample, 
				src, 
				subsample);
	}
    }

  /*  free up temporary arrays  */
  g_free (row);
  g_free (x_frac);
  g_free (src);
}
