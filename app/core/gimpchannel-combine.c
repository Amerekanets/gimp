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
#include <string.h>

#include <gtk/gtk.h>

#include "libgimpcolor/gimpcolor.h"
#include "libgimpmath/gimpmath.h"

#include "core-types.h"

#include "paint-funcs/paint-funcs.h"

#include "boundary.h"
#include "drawable.h"
#include "gimpimage.h"
#include "gimpimage-mask.h"
#include "gimpchannel.h"
#include "gimplayer.h"
#include "gimplut.h"
#include "parasitelist.h"
#include "pixel_processor.h"
#include "pixel_region.h"
#include "lut_funcs.h"
#include "temp_buf.h"
#include "tile.h"
#include "tile_manager.h"
#include "undo.h"

#include "libgimp/gimpintl.h"


static void   gimp_channel_class_init (GimpChannelClass *klass);
static void   gimp_channel_init       (GimpChannel      *channel);
static void   gimp_channel_destroy    (GtkObject        *object);


static GimpDrawableClass * parent_class = NULL;


GtkType
gimp_channel_get_type (void)
{
  static GtkType channel_type = 0;

  if (!channel_type)
    {
      GtkTypeInfo channel_info =
      {
	"GimpChannel",
	sizeof (GimpChannel),
	sizeof (GimpChannelClass),
	(GtkClassInitFunc) gimp_channel_class_init,
	(GtkObjectInitFunc) gimp_channel_init,
        /* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      channel_type = gtk_type_unique (GIMP_TYPE_DRAWABLE, &channel_info);
    }

  return channel_type;
}

static void
gimp_channel_class_init (GimpChannelClass *klass)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass *) klass;

  parent_class = gtk_type_class (GIMP_TYPE_DRAWABLE);

  object_class->destroy = gimp_channel_destroy;
}

static void
gimp_channel_init (GimpChannel *channel)
{
  gimp_rgba_set (&channel->color, 0.0, 0.0, 0.0, 1.0);

  channel->show_masked = FALSE;

  /*  Selection mask variables  */
  channel->boundary_known = TRUE;
  channel->segs_in        = NULL;
  channel->segs_out       = NULL;
  channel->num_segs_in    = 0;
  channel->num_segs_out   = 0;
  channel->empty          = TRUE;
  channel->bounds_known   = TRUE;
  channel->x1             = 0;
  channel->y1             = 0;
  channel->x2             = 0;
  channel->y2             = 0;
  
}

static void
gimp_channel_destroy (GtkObject *object)
{
  GimpChannel *channel;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GIMP_IS_CHANNEL (object));

  channel = GIMP_CHANNEL (object);

  /* free the segments?  */
  if (channel->segs_in)
    g_free (channel->segs_in);
  if (channel->segs_out)
    g_free (channel->segs_out);

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gimp_channel_validate (TileManager *tm,
		       Tile        *tile)
{
  /*  Set the contents of the tile to empty  */
  memset (tile_data_pointer (tile, 0, 0), 
	  TRANSPARENT_OPACITY, tile_size (tile));
}

GimpChannel *
gimp_channel_new (GimpImage     *gimage,
		  gint           width,
		  gint           height,
		  const gchar   *name,
		  const GimpRGB *color)
{
  GimpChannel *channel;

  g_return_val_if_fail (color != NULL, NULL);

  channel = gtk_type_new (GIMP_TYPE_CHANNEL);

  gimp_drawable_configure (GIMP_DRAWABLE (channel),
			   gimage, width, height, GRAY_GIMAGE, name);

  /*  set the channel color and opacity  */
  channel->color = *color;

  channel->show_masked = TRUE;

  /*  selection mask variables  */
  channel->x2 = width;
  channel->y2 = height;

  return channel;
}

GimpChannel *
gimp_channel_copy (const GimpChannel *channel,
                   gboolean           dummy) /*  the dummy is for symmetry with
                                              *  gimp_layer_copy() because
                                              *  both functions are used as
                                              *  function pointers in
                                              *  GimpDrawableListView --Mitch
                                              */
{
  gchar       *channel_name;
  GimpChannel *new_channel;
  PixelRegion  srcPR, destPR;
  gchar       *ext;
  gint         number;
  const gchar *name;
  gint         len;

  /*  formulate the new channel name  */
  name = gimp_object_get_name (GIMP_OBJECT (channel));

  ext = strrchr (name, '#');
  len = strlen (_("copy"));
  if ((strlen (name) >= len &&
       strcmp (&name[strlen (name) - len], _("copy")) == 0) ||
      (ext && (number = atoi (ext + 1)) > 0 && 
       ((int)(log10 (number) + 1)) == strlen (ext + 1)))
    /* don't have redundant "copy"s */
    channel_name = g_strdup (name);
  else
    channel_name = g_strdup_printf (_("%s copy"), name);

  /*  allocate a new channel object  */
  new_channel = gimp_channel_new (GIMP_DRAWABLE (channel)->gimage, 
				  GIMP_DRAWABLE (channel)->width, 
				  GIMP_DRAWABLE (channel)->height, 
				  channel_name,
				  &channel->color);

  GIMP_DRAWABLE (new_channel)->visible = GIMP_DRAWABLE (channel)->visible;

  new_channel->show_masked = channel->show_masked;

  /*  copy the contents across channels  */
  pixel_region_init (&srcPR, GIMP_DRAWABLE (channel)->tiles, 0, 0, 
		     GIMP_DRAWABLE (channel)->width, 
		     GIMP_DRAWABLE (channel)->height, FALSE);
  pixel_region_init (&destPR, GIMP_DRAWABLE (new_channel)->tiles,
		     0, 0,
		     GIMP_DRAWABLE (channel)->width,
		     GIMP_DRAWABLE (channel)->height, TRUE);
  copy_region (&srcPR, &destPR);

  /* copy the parasites */
  GIMP_DRAWABLE (new_channel)->parasites =
    parasite_list_copy (GIMP_DRAWABLE (channel)->parasites);

  /*  free up the channel_name memory  */
  g_free (channel_name);

  return new_channel;
}

void 
gimp_channel_set_color (GimpChannel   *channel,
			const GimpRGB *color)
{
  g_return_if_fail (channel != NULL);
  g_return_if_fail (GIMP_IS_CHANNEL (channel));
  g_return_if_fail (color != NULL);

  channel->color = *color;
}

const GimpRGB *
gimp_channel_get_color (const GimpChannel *channel)
{
  g_return_val_if_fail (channel != NULL, NULL);
  g_return_val_if_fail (GIMP_IS_CHANNEL (channel), NULL);

  return &channel->color;
}

gint
gimp_channel_get_opacity (const GimpChannel *channel)
{
  g_return_val_if_fail (channel != NULL, 0);
  g_return_val_if_fail (GIMP_IS_CHANNEL (channel), 0);

  return (gint) (channel->color.a * 100.999);
}

void 
gimp_channel_set_opacity (GimpChannel *channel,
			  gint         opacity)
{
  g_return_if_fail (channel != NULL);
  g_return_if_fail (GIMP_IS_CHANNEL (channel));

  opacity = CLAMP (opacity, 0, 100);

  channel->color.a = opacity / 100.0;
}

void
gimp_channel_scale (GimpChannel *channel,
		    gint         new_width,
		    gint         new_height)
{
  PixelRegion  srcPR, destPR;
  TileManager *new_tiles;

  if (new_width == 0 || new_height == 0)
    return;

  /*  Update the old channel position  */
  drawable_update (GIMP_DRAWABLE (channel),
		   0, 0,
		   GIMP_DRAWABLE (channel)->width,
		   GIMP_DRAWABLE (channel)->height);

  /*  Configure the pixel regions  */
  pixel_region_init (&srcPR, GIMP_DRAWABLE (channel)->tiles,
		     0, 0,
		     GIMP_DRAWABLE (channel)->width,
		     GIMP_DRAWABLE (channel)->height, FALSE);

  /*  Allocate the new channel, configure dest region  */
  new_tiles = tile_manager_new (new_width, new_height, 1);
  pixel_region_init (&destPR, new_tiles, 0, 0, new_width, new_height, TRUE);

  /*  Add an alpha channel  */
  scale_region (&srcPR, &destPR);

  /*  Push the channel on the undo stack  */
  undo_push_channel_mod (GIMP_DRAWABLE (channel)->gimage, channel);

  /*  Configure the new channel  */
  GIMP_DRAWABLE (channel)->tiles = new_tiles;
  GIMP_DRAWABLE (channel)->width = new_width;
  GIMP_DRAWABLE (channel)->height = new_height;

  /*  bounds are now unknown  */
  channel->bounds_known = FALSE;

  /*  Update the new channel position  */
  drawable_update (GIMP_DRAWABLE (channel),
		   0, 0,
		   GIMP_DRAWABLE (channel)->width,
		   GIMP_DRAWABLE (channel)->height);
}

void
gimp_channel_resize (GimpChannel *channel,
		     gint         new_width,
		     gint         new_height,
		     gint         offx,
		     gint         offy)
{
  PixelRegion  srcPR, destPR;
  TileManager *new_tiles;
  guchar       bg = 0;
  gint         clear;
  gint         w, h;
  gint         x1, y1, x2, y2;

  if (!new_width || !new_height)
    return;

  x1 = CLAMP (offx, 0, new_width);
  y1 = CLAMP (offy, 0, new_height);
  x2 = CLAMP ((offx + GIMP_DRAWABLE (channel)->width), 0, new_width);
  y2 = CLAMP ((offy + GIMP_DRAWABLE (channel)->height), 0, new_height);
  w = x2 - x1;
  h = y2 - y1;

  if (offx > 0)
    {
      x1 = 0;
      x2 = offx;
    }
  else
    {
      x1 = -offx;
      x2 = 0;
    }

  if (offy > 0)
    {
      y1 = 0;
      y2 = offy;
    }
  else
    {
      y1 = -offy;
      y2 = 0;
    }

  /*  Update the old channel position  */
  drawable_update (GIMP_DRAWABLE (channel),
		   0, 0,
		   GIMP_DRAWABLE (channel)->width,
		   GIMP_DRAWABLE (channel)->height);

  /*  Configure the pixel regions  */
  pixel_region_init (&srcPR, GIMP_DRAWABLE (channel)->tiles,
		     x1, y1, w, h, FALSE);

  /*  Determine whether the new channel needs to be initially cleared  */
  if ((new_width  > GIMP_DRAWABLE (channel)->width) ||
      (new_height > GIMP_DRAWABLE (channel)->height) ||
      (x2 || y2))
    clear = TRUE;
  else
    clear = FALSE;

  /*  Allocate the new channel, configure dest region  */
  new_tiles = tile_manager_new (new_width, new_height, 1);

  /*  Set to black (empty--for selections)  */
  if (clear)
    {
      pixel_region_init (&destPR, new_tiles, 0, 0, new_width, new_height, TRUE);
      color_region (&destPR, &bg);
    }

  /*  copy from the old to the new  */
  pixel_region_init (&destPR, new_tiles, x2, y2, w, h, TRUE);
  if (w && h)
    copy_region (&srcPR, &destPR);

  /*  Push the channel on the undo stack  */
  undo_push_channel_mod (GIMP_DRAWABLE (channel)->gimage, channel);

  /*  Configure the new channel  */
  GIMP_DRAWABLE (channel)->tiles  = new_tiles;
  GIMP_DRAWABLE (channel)->width  = new_width;
  GIMP_DRAWABLE (channel)->height = new_height;

  /*  bounds are now unknown  */
  channel->bounds_known = FALSE;

  /*  update the new channel area  */
  drawable_update (GIMP_DRAWABLE (channel),
		   0, 0,
		   GIMP_DRAWABLE (channel)->width,
		   GIMP_DRAWABLE (channel)->height);
}


/******************************/
/*  selection mask functions  */
/******************************/

GimpChannel *
gimp_channel_new_mask (GimpImage *gimage,
		       gint       width,
		       gint       height)
{
  GimpRGB      black = { 0.0, 0.0, 0.0, 0.5 };
  GimpChannel *new_channel;

  /*  Create the new channel  */
  new_channel = gimp_channel_new (gimage, width, height,
				  _("Selection Mask"), &black);

  /*  Set the validate procedure  */
  tile_manager_set_validate_proc (GIMP_DRAWABLE (new_channel)->tiles,
				  gimp_channel_validate);

  return new_channel;
}

gboolean
gimp_channel_boundary (GimpChannel  *mask,
		       BoundSeg    **segs_in,
		       BoundSeg    **segs_out,
		       gint         *num_segs_in,
		       gint         *num_segs_out,
		       gint          x1,
		       gint          y1,
		       gint          x2,
		       gint          y2)
{
  gint        x3, y3, x4, y4;
  PixelRegion bPR;

  if (! mask->boundary_known)
    {
      /* free the out of date boundary segments */
      if (mask->segs_in)
	g_free (mask->segs_in);
      if (mask->segs_out)
	g_free (mask->segs_out);

      if (gimp_channel_bounds (mask, &x3, &y3, &x4, &y4))
	{
	  pixel_region_init (&bPR, GIMP_DRAWABLE (mask)->tiles,
			     x3, y3, (x4 - x3), (y4 - y3), FALSE);
	  mask->segs_out = find_mask_boundary (&bPR, &mask->num_segs_out,
					       IgnoreBounds,
					       x1, y1,
					       x2, y2);
	  x1 = MAX (x1, x3);
	  y1 = MAX (y1, y3);
	  x2 = MIN (x2, x4);
	  y2 = MIN (y2, y4);

	  if (x2 > x1 && y2 > y1)
	    {
	      pixel_region_init (&bPR, GIMP_DRAWABLE (mask)->tiles,
				 0, 0,
				 GIMP_DRAWABLE (mask)->width,
				 GIMP_DRAWABLE (mask)->height, FALSE);
	      mask->segs_in =  find_mask_boundary (&bPR, &mask->num_segs_in,
						   WithinBounds,
						   x1, y1,
						   x2, y2);
	    }
	  else
	    {
	      mask->segs_in     = NULL;
	      mask->num_segs_in = 0;
	    }
	}
      else
	{
	  mask->segs_in      = NULL;
	  mask->segs_out     = NULL;
	  mask->num_segs_in  = 0;
	  mask->num_segs_out = 0;
	}
      mask->boundary_known = TRUE;
    }

  *segs_in      = mask->segs_in;
  *segs_out     = mask->segs_out;
  *num_segs_in  = mask->num_segs_in;
  *num_segs_out = mask->num_segs_out;

  return TRUE;
}

gint
gimp_channel_value (GimpChannel *mask,
		    gint         x,
		    gint         y)
{
  Tile *tile;
  gint  val;

  /*  Some checks to cut back on unnecessary work  */
  if (mask->bounds_known)
    {
      if (mask->empty)
	return 0;
      else if (x < mask->x1 || x >= mask->x2 || y < mask->y1 || y >= mask->y2)
	return 0;
    }
  else
    {
      if (x < 0 || x >= GIMP_DRAWABLE (mask)->width ||
	  y < 0 || y >= GIMP_DRAWABLE (mask)->height)
	return 0;
    }

  tile = tile_manager_get_tile (GIMP_DRAWABLE (mask)->tiles, x, y, TRUE, FALSE);
  val = *(guchar *) (tile_data_pointer (tile, x % TILE_WIDTH, y % TILE_HEIGHT));
  tile_release (tile, FALSE);

  return val;
}

gboolean
gimp_channel_bounds (GimpChannel *mask,
		     gint        *x1,
		     gint        *y1,
		     gint        *x2,
		     gint        *y2)
{
  PixelRegion  maskPR;
  guchar      *data, *data1;
  gint         x, y;
  gint         ex, ey;
  gint         tx1, tx2, ty1, ty2;
  gint         minx, maxx;
  gpointer     pr;

  /*  if the mask's bounds have already been reliably calculated...  */
  if (mask->bounds_known)
    {
      *x1 = mask->x1;
      *y1 = mask->y1;
      *x2 = mask->x2;
      *y2 = mask->y2;

      return !mask->empty;
    }

  /*  go through and calculate the bounds  */
  tx1 = GIMP_DRAWABLE (mask)->width;
  ty1 = GIMP_DRAWABLE (mask)->height;
  tx2 = 0;
  ty2 = 0;

  pixel_region_init (&maskPR, GIMP_DRAWABLE (mask)->tiles,
		     0, 0,
		     GIMP_DRAWABLE (mask)->width,
		     GIMP_DRAWABLE (mask)->height, FALSE);
  for (pr = pixel_regions_register (1, &maskPR);
       pr != NULL;
       pr = pixel_regions_process (pr))
    {
      data1 = data = maskPR.data;
      ex = maskPR.x + maskPR.w;
      ey = maskPR.y + maskPR.h;
      /* only check the pixels if this tile is not fully within the currently
	 computed bounds */
      if (maskPR.x < tx1 || ex > tx2 ||
	  maskPR.y < ty1 || ey > ty2)
        {
	  /* Check upper left and lower right corners to see if we can
	     avoid checking the rest of the pixels in this tile */
	  if (data[0] && data[maskPR.rowstride*(maskPR.h - 1) + maskPR.w - 1])
	  {
	    if (maskPR.x < tx1)
	      tx1 = maskPR.x;
	    if (ex > tx2)
	      tx2 = ex;
	    if (maskPR.y < ty1)
	      ty1 = maskPR.y;
	    if (ey > ty2)
	      ty2 = ey;
	  }
	  else
	    for (y = maskPR.y; y < ey; y++, data1 += maskPR.rowstride)
	    {
	      for (x = maskPR.x, data = data1; x < ex; x++, data++)
		if (*data)
		{
		  minx = x;
		  maxx = x;
		  for (; x < ex; x++, data++)
		    if (*data)
		      maxx = x;
		  if (minx < tx1)
		    tx1 = minx;
		  if (maxx > tx2)
		    tx2 = maxx;
		  if (y < ty1)
		    ty1 = y;
		  if (y > ty2)
		    ty2 = y;
	      }
	    }
	}
    }

  tx2 = CLAMP (tx2 + 1, 0, GIMP_DRAWABLE (mask)->width);
  ty2 = CLAMP (ty2 + 1, 0, GIMP_DRAWABLE (mask)->height);

  if (tx1 == GIMP_DRAWABLE (mask)->width && ty1 == GIMP_DRAWABLE (mask)->height)
    {
      mask->empty = TRUE;
      mask->x1    = 0;
      mask->y1    = 0;
      mask->x2    = GIMP_DRAWABLE (mask)->width;
      mask->y2    = GIMP_DRAWABLE (mask)->height;
    }
  else
    {
      mask->empty = FALSE;
      mask->x1    = tx1;
      mask->y1    = ty1;
      mask->x2    = tx2;
      mask->y2    = ty2;
    }
  mask->bounds_known = TRUE;

  *x1 = tx1;
  *x2 = tx2;
  *y1 = ty1;
  *y2 = ty2;

  return !mask->empty;
}

gboolean
gimp_channel_is_empty (GimpChannel *mask)
{
  PixelRegion  maskPR;
  guchar      *data;
  gint         x, y;
  gpointer     pr;

  if (mask->bounds_known)
    return mask->empty;

  pixel_region_init (&maskPR, GIMP_DRAWABLE (mask)->tiles,
		     0, 0,
		     GIMP_DRAWABLE (mask)->width,
		     GIMP_DRAWABLE (mask)->height, FALSE);
  for (pr = pixel_regions_register (1, &maskPR);
       pr != NULL;
       pr = pixel_regions_process (pr))
    {
      /*  check if any pixel in the mask is non-zero  */
      data = maskPR.data;
      for (y = 0; y < maskPR.h; y++)
	for (x = 0; x < maskPR.w; x++)
	  if (*data++)
	    {
	      pixel_regions_process_stop (pr);
	      return FALSE;
	    }
    }

  /*  The mask is empty, meaning we can set the bounds as known  */
  if (mask->segs_in)
    g_free (mask->segs_in);
  if (mask->segs_out)
    g_free (mask->segs_out);

  mask->empty          = TRUE;
  mask->segs_in        = NULL;
  mask->segs_out       = NULL;
  mask->num_segs_in    = 0;
  mask->num_segs_out   = 0;
  mask->bounds_known   = TRUE;
  mask->boundary_known = TRUE;
  mask->x1             = 0;
  mask->y1             = 0;
  mask->x2             = GIMP_DRAWABLE (mask)->width;
  mask->y2             = GIMP_DRAWABLE (mask)->height;

  return TRUE;
}

void
gimp_channel_add_segment (GimpChannel *mask,
			  gint         x,
			  gint         y,
			  gint         width,
			  gint         value)
{
  PixelRegion  maskPR;
  guchar      *data;
  gint         val;
  gint         x2;
  gpointer     pr;

  /*  check horizontal extents...  */
  x2 = x + width;
  if (x2 < 0) x2 = 0;
  if (x2 > GIMP_DRAWABLE (mask)->width) x2 = GIMP_DRAWABLE (mask)->width;
  if (x < 0) x = 0;
  if (x > GIMP_DRAWABLE (mask)->width) x = GIMP_DRAWABLE (mask)->width;
  width = x2 - x;
  if (!width) return;

  if (y < 0 || y > GIMP_DRAWABLE (mask)->height)
    return;

  pixel_region_init (&maskPR, GIMP_DRAWABLE (mask)->tiles,
		     x, y, width, 1, TRUE);
  for (pr = pixel_regions_register (1, &maskPR);
       pr != NULL;
       pr = pixel_regions_process (pr))
    {
      data = maskPR.data;
      width = maskPR.w;
      while (width--)
	{
	  val = *data + value;
	  if (val > 255)
	    val = 255;
	  *data++ = val;
	}
    }
}

void
gimp_channel_sub_segment (GimpChannel *mask,
			  gint         x,
			  gint         y,
			  gint         width,
			  gint         value)
{
  PixelRegion  maskPR;
  guchar      *data;
  gint         val;
  gint         x2;
  gpointer     pr;

  /*  check horizontal extents...  */
  x2 = x + width;
  if (x2 < 0) x2 = 0;
  if (x2 > GIMP_DRAWABLE (mask)->width) x2 = GIMP_DRAWABLE (mask)->width;
  if (x < 0) x = 0;
  if (x > GIMP_DRAWABLE (mask)->width) x = GIMP_DRAWABLE (mask)->width;
  width = x2 - x;
  if (!width) return;

  if (y < 0 || y > GIMP_DRAWABLE (mask)->height)
    return;

  pixel_region_init (&maskPR, GIMP_DRAWABLE (mask)->tiles, x, y, width, 1, TRUE);
  for (pr = pixel_regions_register (1, &maskPR);
       pr != NULL;
       pr = pixel_regions_process (pr))
    {
      data = maskPR.data;
      width = maskPR.w;
      while (width--)
	{
	  val = *data - value;
	  if (val < 0)
	    val = 0;
	  *data++ = val;
	}
    }
}

void
gimp_channel_combine_rect (GimpChannel *mask,
			   ChannelOps   op,
			   gint         x,
			   gint         y,
			   gint         w,
			   gint         h)
{
  gint        x2, y2;
  PixelRegion maskPR;
  guchar      color;

  y2 = y + h;
  x2 = x + w;

  x  = CLAMP (x,  0, GIMP_DRAWABLE (mask)->width);
  y  = CLAMP (y,  0, GIMP_DRAWABLE (mask)->height);
  x2 = CLAMP (x2, 0, GIMP_DRAWABLE (mask)->width);
  y2 = CLAMP (y2, 0, GIMP_DRAWABLE (mask)->height);

  if (x2 - x <= 0 || y2 - y <= 0)
    return;

  pixel_region_init (&maskPR, GIMP_DRAWABLE (mask)->tiles,
		     x, y, x2 - x, y2 - y, TRUE);
  if (op == CHANNEL_OP_ADD  || op == CHANNEL_OP_REPLACE)
    color = 255;
  else
    color = 0;
  color_region (&maskPR, &color);

  /*  Determine new boundary  */
  if (mask->bounds_known && (op == CHANNEL_OP_ADD) && !mask->empty)
    {
      if (x < mask->x1)
	mask->x1 = x;
      if (y < mask->y1)
	mask->y1 = y;
      if ((x + w) > mask->x2)
	mask->x2 = (x + w);
      if ((y + h) > mask->y2)
	mask->y2 = (y + h);
    }
  else if (op == CHANNEL_OP_REPLACE || mask->empty)
    {
      mask->empty = FALSE;
      mask->x1 = x;
      mask->y1 = y;
      mask->x2 = x + w;
      mask->y2 = y + h;
    }
  else
    mask->bounds_known = FALSE;

  mask->x1 = CLAMP (mask->x1, 0, GIMP_DRAWABLE (mask)->width);
  mask->y1 = CLAMP (mask->y1, 0, GIMP_DRAWABLE (mask)->height);
  mask->x2 = CLAMP (mask->x2, 0, GIMP_DRAWABLE (mask)->width);
  mask->y2 = CLAMP (mask->y2, 0, GIMP_DRAWABLE (mask)->height);
}

void
gimp_channel_combine_ellipse (GimpChannel *mask,
			      ChannelOps   op,
			      gint         x,
			      gint         y,
			      gint         w,
			      gint         h,
			      gboolean     antialias)
{
  gint   i, j;
  gint   x0, x1, x2;
  gint   val, last;
  gfloat a_sqr, b_sqr, aob_sqr;
  gfloat w_sqr, h_sqr;
  gfloat y_sqr;
  gfloat t0, t1;
  gfloat r;
  gfloat cx, cy;
  gfloat rad;
  gfloat dist;

  if (!w || !h)
    return;

  a_sqr = (w * w / 4.0);
  b_sqr = (h * h / 4.0);
  aob_sqr = a_sqr / b_sqr;

  cx = x + w / 2.0;
  cy = y + h / 2.0;

  for (i = y; i < (y + h); i++)
    {
      if (i >= 0 && i < GIMP_DRAWABLE (mask)->height)
	{
	  /*  Non-antialiased code  */
	  if (!antialias)
	    {
	      y_sqr = (i + 0.5 - cy) * (i + 0.5 - cy);
	      rad = sqrt (a_sqr - a_sqr * y_sqr / (double) b_sqr);
	      x1 = ROUND (cx - rad);
	      x2 = ROUND (cx + rad);

	      switch (op)
		{
		case CHANNEL_OP_ADD:
		case CHANNEL_OP_REPLACE:
		  gimp_channel_add_segment (mask, x1, i, (x2 - x1), 255);
		  break;
		case CHANNEL_OP_SUB:
		  gimp_channel_sub_segment (mask, x1, i, (x2 - x1), 255);
		  break;
		default:
		  g_warning ("Only ADD, REPLACE and SUB are valid for channel_combine!");
		  break;
		}
	    }
	  /*  antialiasing  */
	  else
	    {
	      x0 = x;
	      last = 0;
	      h_sqr = (i + 0.5 - cy) * (i + 0.5 - cy);
	      for (j = x; j < (x + w); j++)
		{
		  w_sqr = (j + 0.5 - cx) * (j + 0.5 - cx);

		  if (h_sqr != 0)
		    {
		      t0 = w_sqr / h_sqr;
		      t1 = a_sqr / (t0 + aob_sqr);
		      r = sqrt (t1 + t0 * t1);
		      rad = sqrt (w_sqr + h_sqr);
		      dist = rad - r;
		    }
		  else
		    dist = -1.0;

		  if (dist < -0.5)
		    val = 255;
		  else if (dist < 0.5)
		    val = (int) (255 * (1 - (dist + 0.5)));
		  else
		    val = 0;
		  
		  if (last != val && last)
		    {
		      switch (op)
			{
			case CHANNEL_OP_ADD:
			case CHANNEL_OP_REPLACE:
			  gimp_channel_add_segment (mask, x0, i, j - x0, last);
			  break;
			case CHANNEL_OP_SUB:
			  gimp_channel_sub_segment (mask, x0, i, j - x0, last);
			  break;
			default:
			  g_warning ("Only ADD, REPLACE and SUB are valid for channel_combine!");
			  break;
			}
		    }

		  if (last != val)
		    {
		      x0 = j;
		      last = val;
		      /* because we are symetric accross the y axis we can
			 skip ahead a bit if we are inside the ellipse*/
		      if (val == 255 && j < cx)
			j = cx + (cx - j) - 1;
		    }
		}

	      if (last)
		{
		  if (op == CHANNEL_OP_ADD || op == CHANNEL_OP_REPLACE)
		    gimp_channel_add_segment (mask, x0, i, j - x0, last);
		  else if (op == CHANNEL_OP_SUB)
		    gimp_channel_sub_segment (mask, x0, i, j - x0, last);
		  else
		    g_warning ("Only ADD, REPLACE and SUB are valid for channel_combine!");
		}
	    }

	}
    }

  /*  Determine new boundary  */
  if (mask->bounds_known && (op == CHANNEL_OP_ADD) && !mask->empty)
    {
      if (x < mask->x1)
	mask->x1 = x;
      if (y < mask->y1)
	mask->y1 = y;
      if ((x + w) > mask->x2)
	mask->x2 = (x + w);
      if ((y + h) > mask->y2)
	mask->y2 = (y + h);
    }
  else if (op == CHANNEL_OP_REPLACE || mask->empty)
    {
      mask->empty = FALSE;
      mask->x1 = x;
      mask->y1 = y;
      mask->x2 = x + w;
      mask->y2 = y + h;
    }
  else
    mask->bounds_known = FALSE;

  mask->x1 = CLAMP (mask->x1, 0, GIMP_DRAWABLE (mask)->width);
  mask->y1 = CLAMP (mask->y1, 0, GIMP_DRAWABLE (mask)->height);
  mask->x2 = CLAMP (mask->x2, 0, GIMP_DRAWABLE (mask)->width);
  mask->y2 = CLAMP (mask->y2, 0, GIMP_DRAWABLE (mask)->height);
}

static void
gimp_channel_combine_sub_region_add (void        *unused,
				     PixelRegion *srcPR,
				     PixelRegion *destPR)
{
  guchar *src, *dest;
  gint    x, y, val;

  src  = srcPR->data;
  dest = destPR->data;

  for (y = 0; y < srcPR->h; y++)
    {
      for (x = 0; x < srcPR->w; x++)
	{
	  val = dest[x] + src[x];
	  if (val > 255)
	    dest[x] = 255;
	  else
	    dest[x] = val;
	}
      src  += srcPR->rowstride;
      dest += destPR->rowstride;
    }
}

static void
gimp_channel_combine_sub_region_sub (void        *unused,
				     PixelRegion *srcPR,
				     PixelRegion *destPR)
{
  guchar *src, *dest;
  gint    x, y;

  src  = srcPR->data;
  dest = destPR->data;

  for (y = 0; y < srcPR->h; y++)
    {
      for (x = 0; x < srcPR->w; x++)
	{
	  if (src[x] > dest[x])
	    dest[x] = 0;
	  else
	    dest[x]-= src[x];
	}
      src  += srcPR->rowstride;
      dest += destPR->rowstride;
    }
}

static void
gimp_channel_combine_sub_region_intersect (void        *unused,
					   PixelRegion *srcPR,
					   PixelRegion *destPR)
{
  guchar *src, *dest;
  gint    x, y;

  src  = srcPR->data;
  dest = destPR->data;

  for (y = 0; y < srcPR->h; y++)
    {
      for (x = 0; x < srcPR->w; x++)
	{
	  dest[x] = MIN (dest[x], src[x]);
	}
      src  += srcPR->rowstride;
      dest += destPR->rowstride;
  }
}

void
gimp_channel_combine_mask (GimpChannel *mask,
			   GimpChannel *add_on,
			   ChannelOps   op,
			   gint         off_x,
			   gint         off_y)
{
  PixelRegion srcPR, destPR;
  gint        x1, y1, x2, y2;
  gint        w, h;

  x1 = CLAMP (off_x, 0, GIMP_DRAWABLE (mask)->width);
  y1 = CLAMP (off_y, 0, GIMP_DRAWABLE (mask)->height);
  x2 = CLAMP (off_x + GIMP_DRAWABLE (add_on)->width, 0,
	      GIMP_DRAWABLE (mask)->width);
  y2 = CLAMP (off_y + GIMP_DRAWABLE (add_on)->height, 0,
	      GIMP_DRAWABLE (mask)->height);

  w = (x2 - x1);
  h = (y2 - y1);

  pixel_region_init (&srcPR, GIMP_DRAWABLE (add_on)->tiles,
		     (x1 - off_x), (y1 - off_y), w, h, FALSE);
  pixel_region_init (&destPR, GIMP_DRAWABLE (mask)->tiles, x1, y1, w, h, TRUE);

  switch (op)
    {
    case CHANNEL_OP_ADD:
    case CHANNEL_OP_REPLACE:
      pixel_regions_process_parallel ((p_func)
				      gimp_channel_combine_sub_region_add,
				      NULL, 2, &srcPR, &destPR);
      break;
    case CHANNEL_OP_SUB:
      pixel_regions_process_parallel ((p_func)
				      gimp_channel_combine_sub_region_sub,
				      NULL, 2, &srcPR, &destPR);
      break;
    case CHANNEL_OP_INTERSECT:
      pixel_regions_process_parallel ((p_func)
				      gimp_channel_combine_sub_region_intersect,
				      NULL, 2, &srcPR, &destPR);
      break;
    default:
      g_message ("Error: unknown opperation type in channel_combine_mask\n");
      break;
    }

  mask->bounds_known = FALSE;
}

void
gimp_channel_feather (GimpChannel *input,
		      GimpChannel *output,
		      gdouble      radius_x,
		      gdouble      radius_y,
		      ChannelOps   op,
		      gint         off_x,
		      gint         off_y)
{
  gint        x1, y1, x2, y2;
  PixelRegion srcPR;

  x1 = CLAMP (off_x, 0, GIMP_DRAWABLE (output)->width);
  y1 = CLAMP (off_y, 0, GIMP_DRAWABLE (output)->height);
  x2 = CLAMP (off_x + GIMP_DRAWABLE (input)->width, 0,
	      GIMP_DRAWABLE (output)->width);
  y2 = CLAMP (off_y + GIMP_DRAWABLE (input)->height, 0,
	      GIMP_DRAWABLE (output)->height);

  pixel_region_init (&srcPR, GIMP_DRAWABLE (input)->tiles,
		     (x1 - off_x), (y1 - off_y), (x2 - x1), (y2 - y1), FALSE);
  gaussian_blur_region (&srcPR, radius_x, radius_y);

  if (input != output) 
    gimp_channel_combine_mask (output, input, op, 0, 0);

  output->bounds_known = FALSE;
}

void
gimp_channel_push_undo (GimpChannel *mask)
{
  gint         x1, y1, x2, y2;
  MaskUndo    *mask_undo;
  TileManager *undo_tiles;
  PixelRegion  srcPR, destPR;
  GimpImage   *gimage;

  mask_undo = g_new (MaskUndo, 1);
  if (gimp_channel_bounds (mask, &x1, &y1, &x2, &y2))
    {
      undo_tiles = tile_manager_new ((x2 - x1), (y2 - y1), 1);
      pixel_region_init (&srcPR, GIMP_DRAWABLE (mask)->tiles,
			 x1, y1, (x2 - x1), (y2 - y1), FALSE);
      pixel_region_init (&destPR, undo_tiles, 0, 0, (x2 - x1), (y2 - y1), TRUE);
      copy_region (&srcPR, &destPR);
    }
  else
    undo_tiles = NULL;

  mask_undo->tiles = undo_tiles;
  mask_undo->x     = x1;
  mask_undo->y     = y1;

  /* push the undo buffer onto the undo stack */
  gimage = GIMP_DRAWABLE (mask)->gimage;
  undo_push_mask (gimage, mask_undo);
  gimage_mask_invalidate (gimage);

  /*  invalidate the preview  */
  GIMP_DRAWABLE (mask)->preview_valid = FALSE;
}

void
gimp_channel_clear (GimpChannel *mask)
{
  PixelRegion maskPR;
  guchar      bg = 0;

  /*  push the current channel onto the undo stack  */
  gimp_channel_push_undo (mask);

  if (mask->bounds_known && !mask->empty)
    {
      pixel_region_init (&maskPR, GIMP_DRAWABLE (mask)->tiles,
			 mask->x1, mask->y1,
			 (mask->x2 - mask->x1), (mask->y2 - mask->y1), TRUE);
      color_region (&maskPR, &bg);
    }
  else
    {
      /*  clear the mask  */
      pixel_region_init (&maskPR, GIMP_DRAWABLE (mask)->tiles,
			 0, 0,
			 GIMP_DRAWABLE (mask)->width,
			 GIMP_DRAWABLE (mask)->height, TRUE);
      color_region (&maskPR, &bg);
    }

  /*  we know the bounds  */
  mask->bounds_known = TRUE;
  mask->empty        = TRUE;
  mask->x1           = 0;
  mask->y1           = 0;
  mask->x2           = GIMP_DRAWABLE (mask)->width;
  mask->y2           = GIMP_DRAWABLE (mask)->height;
}

void
gimp_channel_invert (GimpChannel *mask)
{
  PixelRegion  maskPR;
  GimpLut     *lut;

  /*  push the current channel onto the undo stack  */
  gimp_channel_push_undo (mask);

  pixel_region_init (&maskPR, GIMP_DRAWABLE (mask)->tiles,
		     0, 0,
		     GIMP_DRAWABLE (mask)->width,
		     GIMP_DRAWABLE (mask)->height, TRUE);
  
  lut = invert_lut_new (1);

  pixel_regions_process_parallel ((p_func) gimp_lut_process_inline,
				  lut, 1, &maskPR);
  gimp_lut_free (lut);
  mask->bounds_known = FALSE;
}

void
gimp_channel_sharpen (GimpChannel *mask)
{
  PixelRegion  maskPR;
  GimpLut     *lut;

  /*  push the current channel onto the undo stack  */
  gimp_channel_push_undo (mask);

  pixel_region_init (&maskPR, GIMP_DRAWABLE (mask)->tiles,
		     0, 0,
		     GIMP_DRAWABLE (mask)->width,
		     GIMP_DRAWABLE (mask)->height, TRUE);
  lut = threshold_lut_new (0.5, 1);

  pixel_regions_process_parallel ((p_func) gimp_lut_process_inline,
				  lut, 1, &maskPR);
  gimp_lut_free (lut);
}

void
gimp_channel_all (GimpChannel *mask)
{
  PixelRegion maskPR;
  guchar      bg = 255;

  /*  push the current channel onto the undo stack  */
  gimp_channel_push_undo (mask);

  /*  clear the mask  */
  pixel_region_init (&maskPR, GIMP_DRAWABLE (mask)->tiles,
		     0, 0,
		     GIMP_DRAWABLE (mask)->width,
		     GIMP_DRAWABLE (mask)->height, TRUE);
  color_region (&maskPR, &bg);

  /*  we know the bounds  */
  mask->bounds_known = TRUE;
  mask->empty        = FALSE;
  mask->x1           = 0;
  mask->y1           = 0;
  mask->x2           = GIMP_DRAWABLE (mask)->width;
  mask->y2           = GIMP_DRAWABLE (mask)->height;
}

void
gimp_channel_border (GimpChannel *mask,
		     gint         radius_x,
		     gint         radius_y)
{
  PixelRegion bPR;
  gint        x1, y1, x2, y2;

  if (radius_x < 0 || radius_y < 0)
    return;

  if (! gimp_channel_bounds (mask, &x1, &y1, &x2, &y2))
    return;
  if (gimp_channel_is_empty (mask))
    return;

  if (x1 - radius_x < 0)
    x1 = 0;
  else
    x1 -= radius_x;
  if (x2 + radius_x > GIMP_DRAWABLE (mask)->width)
    x2 = GIMP_DRAWABLE (mask)->width;
  else
    x2 += radius_x;

  if (y1 - radius_y < 0)
    y1 = 0;
  else
    y1 -= radius_y;
  if (y2 + radius_y > GIMP_DRAWABLE (mask)->height)
    y2 = GIMP_DRAWABLE (mask)->height;
  else
    y2 += radius_y;

  /*  push the current channel onto the undo stack  */
  gimp_channel_push_undo (mask);

  pixel_region_init (&bPR, GIMP_DRAWABLE (mask)->tiles, x1, y1,
		     (x2-x1), (y2-y1), TRUE);

  border_region (&bPR, radius_x, radius_y);

  mask->bounds_known = FALSE;
}

void
gimp_channel_grow (GimpChannel *mask,
		   gint         radius_x,
		   gint         radius_y)
{
  PixelRegion bPR;
  gint        x1, y1, x2, y2;

  if (radius_x == 0 && radius_y == 0)
    return;

  if (radius_x <= 0 && radius_y <= 0)
    {
      gimp_channel_shrink (mask, -radius_x, -radius_y, FALSE);
      return;
    }

  if (radius_x < 0 || radius_y < 0)
    return;
  
  if (! gimp_channel_bounds (mask, &x1, &y1, &x2, &y2))
    return;
  if (gimp_channel_is_empty (mask))
    return;

  if (x1 - radius_x > 0)
    x1 = x1 - radius_x;
  else
    x1 = 0;
  if (y1 - radius_y > 0)
    y1 = y1 - radius_y;
  else
    y1 = 0;
  if (x2 + radius_x < GIMP_DRAWABLE (mask)->width)
    x2 = x2 + radius_x;
  else
    x2 = GIMP_DRAWABLE (mask)->width;
  if (y2 + radius_y < GIMP_DRAWABLE (mask)->height)
    y2 = y2 + radius_y;
  else
    y2 = GIMP_DRAWABLE (mask)->height;

  /*  push the current channel onto the undo stack  */
  gimp_channel_push_undo (mask);

  /*  need full extents for grow, not! */
  pixel_region_init (&bPR, GIMP_DRAWABLE (mask)->tiles, x1, y1, (x2 - x1),
		     (y2 - y1), TRUE);

  fatten_region (&bPR, radius_x, radius_y);

  mask->bounds_known = FALSE;
}

void
gimp_channel_shrink (GimpChannel  *mask,
		     gint          radius_x,
		     gint          radius_y,
		     gboolean      edge_lock)
{
  PixelRegion bPR;
  gint        x1, y1, x2, y2;

  if (radius_x == 0 && radius_y == 0)
    return;

  if (radius_x <= 0 && radius_y <= 0)
    {
      gimp_channel_grow (mask, -radius_x, -radius_y);
      return;
    }

  if (radius_x < 0 || radius_y < 0)
    return;
  
  if (! gimp_channel_bounds (mask, &x1, &y1, &x2, &y2))
    return;
  if (gimp_channel_is_empty (mask))
    return;

  if (x1 > 0)
    x1--;
  if (y1 > 0)
    y1--;
  if (x2 < GIMP_DRAWABLE (mask)->width)
    x2++;
  if (y2 < GIMP_DRAWABLE (mask)->height)
    y2++;

  /*  push the current channel onto the undo stack  */
  gimp_channel_push_undo (mask);

  pixel_region_init (&bPR, GIMP_DRAWABLE (mask)->tiles, x1, y1, (x2 - x1),
		     (y2 - y1), TRUE);

  thin_region (&bPR, radius_x, radius_y, edge_lock);

  mask->bounds_known = FALSE;
}

void
gimp_channel_translate (GimpChannel *mask,
			gint         off_x,
			gint         off_y)
{
  gint         width, height;
  GimpChannel *tmp_mask;
  PixelRegion  srcPR, destPR;
  guchar       empty = 0;
  gint         x1, y1, x2, y2;

  tmp_mask = NULL;

  /*  push the current channel onto the undo stack  */
  gimp_channel_push_undo (mask);

  gimp_channel_bounds (mask, &x1, &y1, &x2, &y2);
  x1 = CLAMP ((x1 + off_x), 0, GIMP_DRAWABLE (mask)->width);
  y1 = CLAMP ((y1 + off_y), 0, GIMP_DRAWABLE (mask)->height);
  x2 = CLAMP ((x2 + off_x), 0, GIMP_DRAWABLE (mask)->width);
  y2 = CLAMP ((y2 + off_y), 0, GIMP_DRAWABLE (mask)->height);

  width = x2 - x1;
  height = y2 - y1;

  /*  make sure width and height are non-zero  */
  if (width != 0 && height != 0)
    {
      /*  copy the portion of the mask we will keep to a
       *  temporary buffer
       */
      tmp_mask = gimp_channel_new_mask (GIMP_DRAWABLE (mask)->gimage,
					width, height);

      pixel_region_init (&srcPR, GIMP_DRAWABLE (mask)->tiles,
			 x1 - off_x, y1 - off_y, width, height, FALSE);
      pixel_region_init (&destPR, GIMP_DRAWABLE (tmp_mask)->tiles,
			 0, 0, width, height, TRUE);
      copy_region (&srcPR, &destPR);
    }

  /*  clear the mask  */
  pixel_region_init (&srcPR, GIMP_DRAWABLE (mask)->tiles,
		     0, 0,
		     GIMP_DRAWABLE (mask)->width,
		     GIMP_DRAWABLE (mask)->height, TRUE);
  color_region (&srcPR, &empty);

  if (width != 0 && height != 0)
    {
      /*  copy the temp mask back to the mask  */
      pixel_region_init (&srcPR, GIMP_DRAWABLE (tmp_mask)->tiles,
			 0, 0, width, height, FALSE);
      pixel_region_init (&destPR, GIMP_DRAWABLE (mask)->tiles,
			 x1, y1, width, height, TRUE);
      copy_region (&srcPR, &destPR);

      /*  free the temporary mask  */
      gtk_object_unref (GTK_OBJECT (tmp_mask));
    }

  /*  calculate new bounds  */
  if (width == 0 || height == 0)
    {
      mask->empty = TRUE;
      mask->x1 = 0; mask->y1 = 0;
      mask->x2 = GIMP_DRAWABLE (mask)->width;
      mask->y2 = GIMP_DRAWABLE (mask)->height;
    }
  else
    {
      mask->x1 = x1;
      mask->y1 = y1;
      mask->x2 = x2;
      mask->y2 = y2;
    }
}

void
gimp_channel_load (GimpChannel *mask,
		   GimpChannel *channel)
{
  PixelRegion srcPR, destPR;

  /*  push the current mask onto the undo stack  */
  gimp_channel_push_undo (mask);

  /*  copy the channel to the mask  */
  pixel_region_init (&srcPR, GIMP_DRAWABLE (channel)->tiles,
		     0, 0,
		     GIMP_DRAWABLE (channel)->width,
		     GIMP_DRAWABLE (channel)->height, FALSE);
  pixel_region_init (&destPR, GIMP_DRAWABLE (mask)->tiles,
		     0, 0,
		     GIMP_DRAWABLE (channel)->width,
		     GIMP_DRAWABLE (channel)->height, TRUE);
  copy_region (&srcPR, &destPR);

  mask->bounds_known = FALSE;
}

void
gimp_channel_layer_alpha (GimpChannel *mask,
			  GimpLayer   *layer)
{
  PixelRegion srcPR, destPR;
  guchar      empty = 0;
  gint        x1, y1, x2, y2;

  /*  push the current mask onto the undo stack  */
  gimp_channel_push_undo (mask);

  /*  clear the mask  */
  pixel_region_init (&destPR, GIMP_DRAWABLE (mask)->tiles,
		     0, 0,
		     GIMP_DRAWABLE (mask)->width,
		     GIMP_DRAWABLE (mask)->height, TRUE);
  color_region (&destPR, &empty);

  x1 = CLAMP (GIMP_DRAWABLE (layer)->offset_x, 0, GIMP_DRAWABLE (mask)->width);
  y1 = CLAMP (GIMP_DRAWABLE (layer)->offset_y, 0, GIMP_DRAWABLE (mask)->height);
  x2 = CLAMP (GIMP_DRAWABLE (layer)->offset_x + GIMP_DRAWABLE (layer)->width,
	      0, GIMP_DRAWABLE (mask)->width);
  y2 = CLAMP (GIMP_DRAWABLE( layer)->offset_y + GIMP_DRAWABLE (layer)->height,
	      0, GIMP_DRAWABLE (mask)->height);

  pixel_region_init (&srcPR, GIMP_DRAWABLE (layer)->tiles,
		     (x1 - GIMP_DRAWABLE (layer)->offset_x),
		     (y1 - GIMP_DRAWABLE (layer)->offset_y),
		     (x2 - x1), (y2 - y1), FALSE);
  pixel_region_init (&destPR, GIMP_DRAWABLE (mask)->tiles,
		     x1, y1, (x2 - x1), (y2 - y1), TRUE);
  extract_alpha_region (&srcPR, NULL, &destPR);

  mask->bounds_known = FALSE;
}

void
gimp_channel_layer_mask (GimpChannel *mask,
			 GimpLayer   *layer)
{
  PixelRegion srcPR, destPR;
  guchar      empty = 0;
  gint        x1, y1, x2, y2;

  /*  push the current mask onto the undo stack  */
  gimp_channel_push_undo (mask);

  /*  clear the mask  */
  pixel_region_init (&destPR, GIMP_DRAWABLE (mask)->tiles, 
		     0, 0, 
		     GIMP_DRAWABLE (mask)->width, GIMP_DRAWABLE (mask)->height, 
		     TRUE);
  color_region (&destPR, &empty);

  x1 = CLAMP (GIMP_DRAWABLE (layer)->offset_x, 0, GIMP_DRAWABLE (mask)->width);
  y1 = CLAMP (GIMP_DRAWABLE (layer)->offset_y, 0, GIMP_DRAWABLE (mask)->height);
  x2 = CLAMP (GIMP_DRAWABLE (layer)->offset_x + GIMP_DRAWABLE (layer)->width, 
	      0, GIMP_DRAWABLE (mask)->width);
  y2 = CLAMP (GIMP_DRAWABLE (layer)->offset_y + GIMP_DRAWABLE (layer)->height, 
	      0, GIMP_DRAWABLE (mask)->height);

  pixel_region_init (&srcPR, GIMP_DRAWABLE (layer->mask)->tiles,
		     (x1 - GIMP_DRAWABLE (layer)->offset_x), 
		     (y1 - GIMP_DRAWABLE (layer)->offset_y),
		     (x2 - x1), (y2 - y1), 
		     FALSE);
  pixel_region_init (&destPR, GIMP_DRAWABLE (mask)->tiles, 
		     x1, y1, 
		     (x2 - x1), (y2 - y1), 
		     TRUE);
  copy_region (&srcPR, &destPR);

  mask->bounds_known = FALSE;
}

void
gimp_channel_invalidate_bounds (GimpChannel *channel)
{
  channel->bounds_known = FALSE;
}
