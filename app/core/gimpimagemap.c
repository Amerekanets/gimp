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

#include <glib-object.h>

#include "core-types.h"

#include "base/pixel-region.h"
#include "base/tile-manager.h"
#include "base/tile.h"

#include "paint-funcs/paint-funcs.h"

#include "gimpdrawable.h"
#include "gimpimage.h"
#include "gimpimagemap.h"
#include "gimpmarshal.h"


enum
{
  FLUSH,
  LAST_SIGNAL
};


struct _GimpImageMap
{
  GimpObject             parent_instance;

  gboolean               interactive;
  GimpDrawable          *drawable;
  TileManager           *undo_tiles;
  gint		         undo_offset_x;
  gint		         undo_offset_y;
  GimpImageMapApplyFunc  apply_func;
  gpointer               user_data;
  PixelRegion            srcPR;
  PixelRegion            destPR;
  PixelRegionIterator   *PRI;
  guint                  idle_id;
};


static void       gimp_image_map_class_init (GimpImageMapClass *klass);
static void       gimp_image_map_init       (GimpImageMap      *image_map);

static gboolean   gimp_image_map_do         (GimpImageMap      *image_map);


static guint  image_map_signals[LAST_SIGNAL] = { 0 };

static GimpObjectClass *parent_class = NULL;


GType
gimp_image_map_get_type (void)
{
  static GType image_map_type = 0;

  if (! image_map_type)
    {
      static const GTypeInfo image_map_info =
      {
        sizeof (GimpImageMapClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_image_map_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data     */
	sizeof (GimpImageMap),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_image_map_init,
      };

      image_map_type = g_type_register_static (GIMP_TYPE_OBJECT,
                                               "GimpImageMap",
                                               &image_map_info, 0);
    }

  return image_map_type;
}

static void
gimp_image_map_class_init (GimpImageMapClass *klass)
{
  parent_class = g_type_class_peek_parent (klass);

  image_map_signals[FLUSH] =
    g_signal_new ("flush",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GimpImageMapClass, flush),
		  NULL, NULL,
		  gimp_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

static void
gimp_image_map_init (GimpImageMap *image_map)
{
  image_map->drawable      = NULL;
  image_map->undo_tiles    = NULL;
  image_map->undo_offset_x = 0;
  image_map->undo_offset_y = 0;
  image_map->apply_func    = NULL;
  image_map->user_data     = NULL;
  image_map->PRI           = NULL;
  image_map->idle_id       = 0;
}

GimpImageMap *
gimp_image_map_new (gboolean      interactive,
                    GimpDrawable *drawable)
{
  GimpImageMap *image_map;
  GimpImage    *gimage;

  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), NULL);

  gimage = gimp_item_get_image (GIMP_ITEM (drawable));

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  image_map = g_object_new (GIMP_TYPE_IMAGE_MAP, NULL);

  image_map->interactive = interactive ? TRUE : FALSE;
  image_map->drawable    = drawable;

  /* Interactive tools based on image_map disable the undo stack
   * to avert any unintented undo interaction through the UI
   */
  gimp_image_undo_freeze (gimage);

  return image_map;
}

void
gimp_image_map_apply (GimpImageMap          *image_map,
                      GimpImageMapApplyFunc  apply_func,
                      gpointer               apply_data)
{
  gint x1, y1;
  gint x2, y2;
  gint offset_x, offset_y;
  gint width, height;

  g_return_if_fail (GIMP_IS_IMAGE_MAP (image_map));
  g_return_if_fail (apply_func != NULL);

  image_map->apply_func = apply_func;
  image_map->user_data  = apply_data;

  /*  If we're still working, remove the timer  */
  if (image_map->idle_id)
    {
      g_source_remove (image_map->idle_id);
      image_map->idle_id = 0;

      pixel_regions_process_stop (image_map->PRI);
      image_map->PRI = NULL;
    }

  /*  Make sure the drawable is still valid  */
  if (! gimp_item_get_image (GIMP_ITEM (image_map->drawable)))
    return;

  /*  The application should occur only within selection bounds  */
  gimp_drawable_mask_bounds (image_map->drawable, &x1, &y1, &x2, &y2);

  /*  If undo tiles don't exist, or change size, (re)allocate  */
  if (image_map->undo_tiles)
    {
      offset_x = image_map->undo_offset_x;
      offset_y = image_map->undo_offset_y;
      width  = tile_manager_width (image_map->undo_tiles);
      height = tile_manager_height (image_map->undo_tiles);
    }
  else
    {
      offset_x = offset_y = width = height = 0;
    }

  if (! image_map->undo_tiles ||
      offset_x != x1 || offset_y != y1 ||
      width != (x2 - x1) || height != (y2 - y1))
    {
      /* If either the extents changed or the tiles don't exist,
       * allocate new
       */
      if (! image_map->undo_tiles ||
	  width != (x2 - x1) || height != (y2 - y1))
	{
	  /*  Destroy old tiles--If they exist  */
	  if (image_map->undo_tiles)
	    tile_manager_destroy (image_map->undo_tiles);

	  /*  Allocate new tiles  */
	  image_map->undo_tiles =
	    tile_manager_new ((x2 - x1), (y2 - y1),
			      gimp_drawable_bytes (image_map->drawable));
	}

      /*  Copy from the image to the new tiles  */
      pixel_region_init (&image_map->srcPR,
			 gimp_drawable_data (image_map->drawable),
			 x1, y1, (x2 - x1), (y2 - y1), FALSE);
      pixel_region_init (&image_map->destPR, image_map->undo_tiles,
			 0, 0, (x2 - x1), (y2 - y1), TRUE);

      copy_region (&image_map->srcPR, &image_map->destPR);

      /*  Set the offsets  */
      image_map->undo_offset_x = x1;
      image_map->undo_offset_y = y1;
    }
  else /* image_map->undo_tiles exist AND drawable dimensions have not changed... */
    {
      /* Reset to initial drawable conditions.            */
      /* Copy from the backup undo tiles to the drawable  */
      pixel_region_init (&image_map->srcPR, image_map->undo_tiles, 
			 0, 0, width, height, FALSE);
      pixel_region_init (&image_map->destPR,
			 gimp_drawable_data (image_map->drawable), 
			 offset_x, offset_y, width, height, TRUE);

      copy_region (&image_map->srcPR, &image_map->destPR);
    }

  /*  Configure the src from the drawable data  */
  pixel_region_init (&image_map->srcPR, image_map->undo_tiles,
		     0, 0, (x2 - x1), (y2 - y1), FALSE);

  /*  Configure the dest as the shadow buffer  */
  pixel_region_init (&image_map->destPR,
		     gimp_drawable_shadow (image_map->drawable),
		     x1, y1, (x2 - x1), (y2 - y1), TRUE);

  /*  Apply the image transformation to the pixels  */
  image_map->PRI = pixel_regions_register (2,
					   &image_map->srcPR,
					   &image_map->destPR);

  /*  Start the intermittant work procedure  */
  image_map->idle_id = g_idle_add ((GSourceFunc) gimp_image_map_do,
                                   image_map);
}

void
gimp_image_map_commit (GimpImageMap *image_map)
{
  GimpImage *gimage;
  gint       x1, y1, x2, y2;

  g_return_if_fail (GIMP_IS_IMAGE_MAP (image_map));

  if (image_map->idle_id)
    {
      g_source_remove (image_map->idle_id);
      image_map->idle_id = 0;

      /*  Finish the changes  */
      while (gimp_image_map_do (image_map));
    }

  /*  Make sure the drawable is still valid  */
  gimage = gimp_item_get_image (GIMP_ITEM (image_map->drawable));

  if (! gimage)
    return;

  /* Interactive phase ends: we can commit an undo frame now */
  gimp_image_undo_thaw (gimage);

  /*  Register an undo step  */
  if (image_map->undo_tiles)
    {
      x1 = image_map->undo_offset_x;
      y1 = image_map->undo_offset_y;
      x2 = x1 + tile_manager_width (image_map->undo_tiles);
      y2 = y1 + tile_manager_height (image_map->undo_tiles);

      gimp_drawable_push_undo (image_map->drawable,
                               x1, y1, x2, y2,
                               image_map->undo_tiles, FALSE);
    }

  g_object_unref (G_OBJECT (image_map));
}

void
gimp_image_map_clear (GimpImageMap *image_map)
{
  GimpImage   *gimage;
  PixelRegion  srcPR, destPR;

  g_return_if_fail (GIMP_IS_IMAGE_MAP (image_map));

  if (image_map->idle_id)
    {
      g_source_remove (image_map->idle_id);
      image_map->idle_id = 0;

      pixel_regions_process_stop (image_map->PRI);
      image_map->PRI = NULL;
    }

  /*  Make sure the drawable is still valid  */
  gimage =  gimp_item_get_image (GIMP_ITEM (image_map->drawable));

  if (! gimage)
    return;

  /*  restore the original image  */
  if (image_map->undo_tiles)
    {
      gint offset_x;
      gint offset_y;
      gint width;
      gint height;

      offset_x = image_map->undo_offset_x;
      offset_y = image_map->undo_offset_y;
      width    = tile_manager_width (image_map->undo_tiles);
      height   = tile_manager_height (image_map->undo_tiles),

      /*  Copy from the drawable to the tiles  */
      pixel_region_init (&srcPR, image_map->undo_tiles, 
			 0, 0, width, height, FALSE);
      pixel_region_init (&destPR, gimp_drawable_data (image_map->drawable),
			 offset_x, offset_y, width, height, TRUE);

      /* if the user has changed the image depth get out quickly */
      if (destPR.bytes != srcPR.bytes) 
	{
	  g_message ("image depth change, unable to restore original image");

	  tile_manager_destroy (image_map->undo_tiles);
          gimp_image_undo_thaw (gimage);

#if 0
          gimp_display_shell_set_menu_sensitivity (GIMP_DISPLAY_SHELL (image_map->gdisp->shell),
                                                   image_map->gdisp->gimage->gimp);
#endif

	  g_object_unref (G_OBJECT (image_map));
	  return;
	}

      copy_region (&srcPR, &destPR);

      /*  Update the area  */
      gimp_drawable_update (image_map->drawable,
			    offset_x, offset_y,
			    width, height);

      /*  Free the undo_tiles tile manager  */
      tile_manager_destroy (image_map->undo_tiles);
      image_map->undo_tiles = NULL;
    }
}

void
gimp_image_map_abort (GimpImageMap *image_map)
{
  GimpImage *gimage;

  g_return_if_fail (GIMP_IS_IMAGE_MAP (image_map));

  gimage = gimp_item_get_image (GIMP_ITEM (image_map->drawable));

  if (! gimage)
    return;

  gimp_image_map_clear (image_map);

  gimp_image_undo_thaw (gimage);

  g_object_unref (G_OBJECT (image_map));
}

guchar *
gimp_image_map_get_color_at (GimpImageMap *image_map, 
                             gint          x, 
                             gint          y)
{
  guchar  src[5];
  guchar *dest;

  g_return_val_if_fail (GIMP_IS_IMAGE_MAP (image_map), NULL);

  if (x >= 0 && x < gimp_drawable_width (image_map->drawable) && 
      y >= 0 && y < gimp_drawable_height (image_map->drawable))
    {
      /* Check if done damage to original image */
      if (! image_map->undo_tiles)
	return gimp_drawable_get_color_at (image_map->drawable, x, y);

      if (! image_map ||
	  (! gimp_item_get_image (GIMP_ITEM (image_map->drawable)) && 
	   gimp_drawable_is_indexed (image_map->drawable)) ||
	  x < 0 || y < 0                                   ||
	  x >= tile_manager_width (image_map->undo_tiles)  ||
	  y >= tile_manager_height (image_map->undo_tiles))
	{
	  return NULL;
	}

      dest = g_new (guchar, 5);
      
      read_pixel_data_1 (image_map->undo_tiles, x, y, src);

      gimp_image_get_color (gimp_item_get_image  (GIMP_ITEM (image_map->drawable)),
			    gimp_drawable_type (image_map->drawable),
			    dest, src);

      if (GIMP_IMAGE_TYPE_HAS_ALPHA (gimp_drawable_type (image_map->drawable)))
	dest[3] = src[gimp_drawable_bytes (image_map->drawable) - 1];
      else
	dest[3] = 255;

      if (gimp_drawable_is_indexed (image_map->drawable))
	dest[4] = src[0];
      else
	dest[4] = 0;

      return dest;
    }
  else /* out of bounds error */
    {
      return NULL;
    }
}


/*  private functions  */

static gboolean
gimp_image_map_do (GimpImageMap *image_map)
{
  GimpImage   *gimage;
  PixelRegion  shadowPR;
  gint         x, y, w, h;

  gimage = gimp_item_get_image (GIMP_ITEM (image_map->drawable));

  if (! gimage)
    {
      image_map->idle_id = 0;

      return FALSE;
    }

  /*  Process the pixel regions and apply the image mapping  */
  (* image_map->apply_func) (&image_map->srcPR,
			     &image_map->destPR,
			     image_map->user_data);

  x = image_map->destPR.x;
  y = image_map->destPR.y;
  w = image_map->destPR.w;
  h = image_map->destPR.h;

  /*  apply the results  */
  pixel_region_init (&shadowPR, gimage->shadow, x, y, w, h, FALSE);
  gimp_image_apply_image (gimage, image_map->drawable, &shadowPR, FALSE,
                          GIMP_OPACITY_OPAQUE, GIMP_REPLACE_MODE,
                          NULL,
                          x, y);

  /*  display the results  */
  gimp_drawable_update (image_map->drawable, x, y, w, h);

  if (image_map->interactive)
    g_signal_emit (G_OBJECT (image_map), image_map_signals[FLUSH], 0);

  image_map->PRI = pixel_regions_process (image_map->PRI);

  if (image_map->PRI == NULL)
    {
      image_map->idle_id = 0;

      if (image_map->interactive)
        gimp_image_flush (gimage);

      return FALSE;
    }

  return TRUE;
}
