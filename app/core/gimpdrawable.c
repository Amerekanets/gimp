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
#include <stdio.h>
#include <string.h>

#include <glib-object.h>

#include "libgimpcolor/gimpcolor.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpbase/gimpbase.h"

#include "core-types.h"

#include "base/pixel-region.h"
#include "base/tile.h"
#include "base/tile-manager.h"

#include "paint-funcs/paint-funcs.h"

#include "gimp.h"
#include "gimp-utils.h"
#include "gimpchannel.h"
#include "gimpcontext.h"
#include "gimpdrawable.h"
#include "gimpdrawable-preview.h"
#include "gimpdrawable-transform.h"
#include "gimpimage.h"
#include "gimpimage-mask.h"
#include "gimpimage-undo-push.h"
#include "gimplayer.h"
#include "gimplist.h"
#include "gimpmarshal.h"
#include "gimpparasitelist.h"
#include "gimppreviewcache.h"

#include "gimp-intl.h"


enum
{
  VISIBILITY_CHANGED,
  ALPHA_CHANGED,
  LAST_SIGNAL
};


/*  local function prototypes  */

static void       gimp_drawable_class_init         (GimpDrawableClass *klass);
static void       gimp_drawable_init               (GimpDrawable      *drawable);

static void       gimp_drawable_finalize           (GObject           *object);

static gsize      gimp_drawable_get_memsize        (GimpObject        *object,
                                                    gsize             *gui_size);

static void       gimp_drawable_invalidate_preview (GimpViewable      *viewable);

static GimpItem * gimp_drawable_duplicate          (GimpItem          *item,
                                                    GType              new_type,
                                                    gboolean           add_alpha);
static void       gimp_drawable_scale              (GimpItem          *item,
                                                    gint               new_width,
                                                    gint               new_height,
                                                    gint               new_offset_x,
                                                    gint               new_offset_y,
                                                    GimpInterpolationType interp_type);
static void       gimp_drawable_resize             (GimpItem          *item,
                                                    gint               new_width,
                                                    gint               new_height,
                                                    gint               offset_x,
                                                    gint               offset_y);
static void       gimp_drawable_flip               (GimpItem          *item,
                                                    GimpOrientationType  flip_type,
                                                    gdouble            axis,
                                                    gboolean           clip_result);
static void       gimp_drawable_rotate             (GimpItem          *item,
                                                    GimpRotationType   rotate_type,
                                                    gdouble            center_x,
                                                    gdouble            center_y,
                                                    gboolean           clip_result);
static void       gimp_drawable_transform          (GimpItem          *item,
                                                    const GimpMatrix3 *matrix,
                                                    GimpTransformDirection  direction,
                                                    GimpInterpolationType   interpolation_type,
                                                    gboolean           clip_result,
                                                    GimpProgressFunc   progress_callback,
                                                    gpointer           progress_data);


/*  private variables  */

static guint gimp_drawable_signals[LAST_SIGNAL] = { 0 };

static GimpItemClass *parent_class = NULL;


GType
gimp_drawable_get_type (void)
{
  static GType drawable_type = 0;

  if (! drawable_type)
    {
      static const GTypeInfo drawable_info =
      {
        sizeof (GimpDrawableClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_drawable_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data     */
	sizeof (GimpDrawable),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_drawable_init,
      };

      drawable_type = g_type_register_static (GIMP_TYPE_ITEM,
					      "GimpDrawable", 
					      &drawable_info, 0);
    }

  return drawable_type;
}

static void
gimp_drawable_class_init (GimpDrawableClass *klass)
{
  GObjectClass      *object_class;
  GimpObjectClass   *gimp_object_class;
  GimpViewableClass *viewable_class;
  GimpItemClass     *item_class;

  object_class      = G_OBJECT_CLASS (klass);
  gimp_object_class = GIMP_OBJECT_CLASS (klass);
  viewable_class    = GIMP_VIEWABLE_CLASS (klass);
  item_class        = GIMP_ITEM_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  gimp_drawable_signals[VISIBILITY_CHANGED] =
    g_signal_new ("visibility_changed",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GimpDrawableClass, visibility_changed),
		  NULL, NULL,
		  gimp_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  gimp_drawable_signals[ALPHA_CHANGED] =
    g_signal_new ("alpha_changed",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GimpDrawableClass, alpha_changed),
		  NULL, NULL,
		  gimp_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  object_class->finalize             = gimp_drawable_finalize;

  gimp_object_class->get_memsize     = gimp_drawable_get_memsize;

  viewable_class->invalidate_preview = gimp_drawable_invalidate_preview;
  viewable_class->get_preview_size   = gimp_drawable_get_preview_size;
  viewable_class->get_popup_size     = gimp_drawable_get_popup_size;
  viewable_class->get_preview        = gimp_drawable_get_preview;

  item_class->duplicate              = gimp_drawable_duplicate;
  item_class->scale                  = gimp_drawable_scale;
  item_class->resize                 = gimp_drawable_resize;
  item_class->flip                   = gimp_drawable_flip;
  item_class->rotate                 = gimp_drawable_rotate;
  item_class->transform              = gimp_drawable_transform;

  klass->visibility_changed          = NULL;
  klass->alpha_changed               = NULL;
}

static void
gimp_drawable_init (GimpDrawable *drawable)
{
  drawable->tiles         = NULL;
  drawable->visible       = FALSE;
  drawable->bytes         = 0;
  drawable->type          = -1;
  drawable->has_alpha     = FALSE;
  drawable->preview_cache = NULL;
  drawable->preview_valid = FALSE;
}

static void
gimp_drawable_finalize (GObject *object)
{
  GimpDrawable *drawable;

  g_return_if_fail (GIMP_IS_DRAWABLE (object));

  drawable = GIMP_DRAWABLE (object);

  if (drawable->tiles)
    {
      tile_manager_unref (drawable->tiles);
      drawable->tiles = NULL;
    }

  if (drawable->preview_cache)
    gimp_preview_cache_invalidate (&drawable->preview_cache);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gsize
gimp_drawable_get_memsize (GimpObject *object,
                           gsize      *gui_size)
{
  GimpDrawable *drawable;
  gsize         memsize = 0;

  drawable = GIMP_DRAWABLE (object);

  if (drawable->tiles)
    memsize += tile_manager_get_memsize (drawable->tiles);

  if (drawable->preview_cache)
    *gui_size += gimp_preview_cache_get_memsize (drawable->preview_cache);

  return memsize + GIMP_OBJECT_CLASS (parent_class)->get_memsize (object,
                                                                  gui_size);
}

static void
gimp_drawable_invalidate_preview (GimpViewable *viewable)
{
  GimpDrawable *drawable;
  GimpImage    *gimage;

  if (GIMP_VIEWABLE_CLASS (parent_class)->invalidate_preview)
    GIMP_VIEWABLE_CLASS (parent_class)->invalidate_preview (viewable);

  drawable = GIMP_DRAWABLE (viewable);

  drawable->preview_valid = FALSE;

  if (drawable->preview_cache)
    gimp_preview_cache_invalidate (&drawable->preview_cache);

  gimage = gimp_item_get_image (GIMP_ITEM (drawable));

  if (gimage)
    gimp_viewable_invalidate_preview (GIMP_VIEWABLE (gimage));
}

static GimpItem *
gimp_drawable_duplicate (GimpItem *item,
                         GType     new_type,
                         gboolean  add_alpha)
{
  GimpDrawable  *drawable;
  GimpItem      *new_item;
  GimpDrawable  *new_drawable;
  GimpImageType  new_image_type;
  PixelRegion    srcPR;
  PixelRegion    destPR;

  g_return_val_if_fail (g_type_is_a (new_type, GIMP_TYPE_DRAWABLE), NULL);

  new_item = GIMP_ITEM_CLASS (parent_class)->duplicate (item, new_type,
                                                        add_alpha);

  if (! GIMP_IS_DRAWABLE (new_item))
    return new_item;

  drawable     = GIMP_DRAWABLE (item);
  new_drawable = GIMP_DRAWABLE (new_item);

  if (add_alpha)
    new_image_type = gimp_drawable_type_with_alpha (drawable);
  else
    new_image_type = gimp_drawable_type (drawable);

  gimp_drawable_configure (new_drawable,
                           gimp_item_get_image (GIMP_ITEM (drawable)),
                           item->offset_x,
                           item->offset_y,
                           item->width,
                           item->height,
                           new_image_type,
                           GIMP_OBJECT (new_drawable)->name);

  new_drawable->visible = drawable->visible;

  pixel_region_init (&srcPR, drawable->tiles, 
                     0, 0, 
                     item->width,
                     item->height,
                     FALSE);
  pixel_region_init (&destPR, new_drawable->tiles,
                     0, 0, 
                     new_item->width,
                     new_item->height,
                     TRUE);

  if (new_image_type == drawable->type)
    copy_region (&srcPR, &destPR);
  else
    add_alpha_region (&srcPR, &destPR);

  return new_item;
}

static void
gimp_drawable_scale (GimpItem              *item,
                     gint                   new_width,
                     gint                   new_height,
                     gint                   new_offset_x,
                     gint                   new_offset_y,
                     GimpInterpolationType  interpolation_type)
{
  GimpDrawable *drawable;
  PixelRegion   srcPR, destPR;
  TileManager  *new_tiles;

  drawable = GIMP_DRAWABLE (item);

  /*  Update the old position  */
  gimp_drawable_update (drawable, 0, 0, item->width, item->height);

  /*  Allocate the new channel  */
  new_tiles = tile_manager_new (new_width, new_height, drawable->bytes);

  /*  Configure the pixel regions  */
  pixel_region_init (&srcPR, drawable->tiles,
		     0, 0,
		     item->width,
		     item->height,
                     FALSE);

  pixel_region_init (&destPR, new_tiles,
                     0, 0,
                     new_width, new_height,
                     TRUE);

  /*  Scale the drawable -
   *   If the drawable is indexed, then we don't use pixel-value
   *   resampling because that doesn't necessarily make sense for indexed
   *   images.
   */
  scale_region (&srcPR, &destPR,
                gimp_drawable_is_indexed (drawable) ?
                GIMP_INTERPOLATION_NONE : interpolation_type);

  tile_manager_unref (drawable->tiles);
  drawable->tiles = new_tiles;

  GIMP_ITEM_CLASS (parent_class)->scale (item, new_width, new_height,
                                         new_offset_x, new_offset_y,
                                         interpolation_type);

  /*  Update the new position  */
  gimp_drawable_update (drawable, 0, 0, item->width, item->height);

  gimp_viewable_size_changed (GIMP_VIEWABLE (drawable));
}

static void
gimp_drawable_resize (GimpItem *item,
                      gint      new_width,
                      gint      new_height,
                      gint      offset_x,
                      gint      offset_y)
{
  GimpDrawable *drawable;
  PixelRegion   srcPR, destPR;
  TileManager  *new_tiles;
  gint          new_offset_x;
  gint          new_offset_y;
  gint          copy_x, copy_y;
  gint          copy_width, copy_height;

  drawable = GIMP_DRAWABLE (item);

  new_offset_x = item->offset_x - offset_x;
  new_offset_y = item->offset_y - offset_y;

  gimp_rectangle_intersect (item->offset_x, item->offset_y,
                            item->width, item->height,
                            new_offset_x, new_offset_y,
                            new_width, new_height,
                            &copy_x, &copy_y,
                            &copy_width, &copy_height);

  /*  Update the old position  */
  gimp_drawable_update (drawable, 0, 0, item->width, item->height);

  /*  Allocate the new tile manager, configure dest region  */
  new_tiles = tile_manager_new (new_width, new_height,
				drawable->bytes);

  /*  Determine whether the new tiles need to be initially cleared  */
  if (copy_width  != new_width ||
      copy_height != new_height)
    {
      guchar bg[MAX_CHANNELS] = { 0, };

      pixel_region_init (&destPR, new_tiles,
                         0, 0,
                         new_width, new_height,
                         TRUE);

      if (! gimp_drawable_has_alpha (drawable) && ! GIMP_IS_CHANNEL (drawable))
        gimp_image_get_background (gimp_item_get_image (item), drawable, bg);

      color_region (&destPR, bg);
    }

  /*  Determine whether anything needs to be copied  */
  if (copy_width && copy_height)
    {
      pixel_region_init (&srcPR, drawable->tiles,
                         copy_x - item->offset_x, copy_y - item->offset_y,
                         copy_width, copy_height,
                         FALSE);

      pixel_region_init (&destPR, new_tiles,
                         copy_x - new_offset_x, copy_y - new_offset_y,
                         copy_width, copy_height,
                         TRUE);

      copy_region (&srcPR, &destPR);
    }

  tile_manager_unref (drawable->tiles);
  drawable->tiles = new_tiles;

  GIMP_ITEM_CLASS (parent_class)->resize (item, new_width, new_height,
                                          offset_x, offset_y);

  /*  update the new area  */
  gimp_drawable_update (drawable, 0, 0, item->width, item->height);

  gimp_viewable_size_changed (GIMP_VIEWABLE (drawable));
}

static void
gimp_drawable_flip (GimpItem            *item,
                    GimpOrientationType  flip_type,
                    gdouble              axis,
                    gboolean             clip_result)
{
  GimpDrawable *drawable;
  TileManager  *tiles;
  gint          off_x, off_y;
  gint          old_off_x, old_off_y;

  drawable = GIMP_DRAWABLE (item);

  gimp_item_offsets (item, &off_x, &off_y);

  tile_manager_get_offsets (drawable->tiles, &old_off_x, &old_off_y);
  tile_manager_set_offsets (drawable->tiles, off_x, off_y);

  tiles = gimp_drawable_transform_tiles_flip (drawable,
                                              drawable->tiles,
                                              flip_type, axis,
                                              clip_result);

  tile_manager_set_offsets (drawable->tiles, old_off_x, old_off_y);

  if (tiles)
    {
      gimp_drawable_transform_paste (drawable, tiles, FALSE);
      tile_manager_unref (tiles);
    }
}

static void
gimp_drawable_rotate (GimpItem         *item,
                      GimpRotationType  rotate_type,
                      gdouble           center_x,
                      gdouble           center_y,
                      gboolean          clip_result)
{
  GimpDrawable *drawable;
  TileManager  *tiles;
  gint          off_x, off_y;
  gint          old_off_x, old_off_y;

  drawable = GIMP_DRAWABLE (item);

  gimp_item_offsets (item, &off_x, &off_y);

  tile_manager_get_offsets (drawable->tiles, &old_off_x, &old_off_y);
  tile_manager_set_offsets (drawable->tiles, off_x, off_y);

  tiles = gimp_drawable_transform_tiles_rotate (drawable,
                                                drawable->tiles,
                                                rotate_type, center_x, center_y,
                                                clip_result);

  tile_manager_set_offsets (drawable->tiles, old_off_x, old_off_y);

  if (tiles)
    {
      gimp_drawable_transform_paste (drawable, tiles, FALSE);
      tile_manager_unref (tiles);
    }
}

static void
gimp_drawable_transform (GimpItem               *item,
                         const GimpMatrix3      *matrix,
                         GimpTransformDirection  direction,
                         GimpInterpolationType   interpolation_type,
                         gboolean                clip_result,
                         GimpProgressFunc        progress_callback,
                         gpointer                progress_data)
{
  GimpDrawable *drawable;
  TileManager  *tiles;
  gint          off_x, off_y;
  gint          old_off_x, old_off_y;

  drawable = GIMP_DRAWABLE (item);

  gimp_item_offsets (item, &off_x, &off_y);

  tile_manager_get_offsets (drawable->tiles, &old_off_x, &old_off_y);
  tile_manager_set_offsets (drawable->tiles, off_x, off_y);

  tiles = gimp_drawable_transform_tiles_affine (drawable,
                                                drawable->tiles,
                                                matrix, direction,
                                                interpolation_type,
                                                clip_result,
                                                progress_callback,
                                                progress_data);

  tile_manager_set_offsets (drawable->tiles, old_off_x, old_off_y);

  if (tiles)
    {
      gimp_drawable_transform_paste (drawable, tiles, FALSE);
      tile_manager_unref (tiles);
    }
}

void
gimp_drawable_configure (GimpDrawable  *drawable,
			 GimpImage     *gimage,
                         gint           offset_x,
                         gint           offset_y,
			 gint           width,
			 gint           height, 
			 GimpImageType  type,
			 const gchar   *name)
{
  g_return_if_fail (GIMP_IS_DRAWABLE (drawable));
  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  gimp_item_configure (GIMP_ITEM (drawable), gimage,
                       offset_x, offset_y, width, height, name);

  drawable->type      = type;
  drawable->bytes     = GIMP_IMAGE_TYPE_BYTES (type);
  drawable->has_alpha = GIMP_IMAGE_TYPE_HAS_ALPHA (type);

  if (drawable->tiles)
    tile_manager_unref (drawable->tiles);

  drawable->tiles = tile_manager_new (width, height, drawable->bytes);

  drawable->visible = TRUE;

  /*  preview variables  */
  drawable->preview_cache = NULL;
  drawable->preview_valid = FALSE;
}

void
gimp_drawable_update (GimpDrawable *drawable,
		      gint          x,
		      gint          y,
		      gint          w,
		      gint          h)
{
  GimpItem  *item;
  GimpImage *gimage;
  gint       offset_x;
  gint       offset_y;

  g_return_if_fail (GIMP_IS_DRAWABLE (drawable));

  item   = GIMP_ITEM (drawable);
  gimage = gimp_item_get_image (item);

  g_return_if_fail (gimage != NULL);

  gimp_item_offsets (item, &offset_x, &offset_y);
  x += offset_x;
  y += offset_y;

  gimp_image_update (gimage, x, y, w, h);

  gimp_viewable_invalidate_preview (GIMP_VIEWABLE (drawable));
}

void
gimp_drawable_push_undo (GimpDrawable *drawable,
                         const gchar  *undo_desc,
                         gint          x1,
                         gint          y1,
                         gint          x2,
                         gint          y2, 
                         TileManager  *tiles,
                         gboolean      sparse)
{
  g_return_if_fail (GIMP_IS_DRAWABLE (drawable));

  if (! tiles)
    gimp_image_undo_push_image (gimp_item_get_image (GIMP_ITEM (drawable)),
                                undo_desc,
                                drawable, 
                                x1, y1, x2, y2);
  else
    gimp_image_undo_push_image_mod (gimp_item_get_image (GIMP_ITEM (drawable)),
                                    undo_desc,
                                    drawable, 
                                    x1, y1, x2, y2,
                                    tiles, sparse);
}

void
gimp_drawable_merge_shadow (GimpDrawable *drawable,
			    gboolean      push_undo,
                            const gchar  *undo_desc)
{
  GimpImage   *gimage;
  PixelRegion  shadowPR;
  gint         x1, y1, x2, y2;

  g_return_if_fail (GIMP_IS_DRAWABLE (drawable));

  gimage = gimp_item_get_image (GIMP_ITEM (drawable));

  g_return_if_fail (GIMP_IS_IMAGE (gimage));
  g_return_if_fail (gimage->shadow != NULL);

  /*  A useful optimization here is to limit the update to the
   *  extents of the selection mask, as it cannot extend beyond
   *  them.
   */
  gimp_drawable_mask_bounds (drawable, &x1, &y1, &x2, &y2);
  pixel_region_init (&shadowPR, gimage->shadow, x1, y1,
		     (x2 - x1), (y2 - y1), FALSE);
  gimp_image_apply_image (gimage, drawable, &shadowPR,
                          push_undo, undo_desc,
                          GIMP_OPACITY_OPAQUE, GIMP_REPLACE_MODE,
                          NULL, x1, y1);
}

void
gimp_drawable_fill (GimpDrawable  *drawable,
		    const GimpRGB *color)
{
  GimpItem    *item;
  GimpImage   *gimage;
  PixelRegion  destPR;
  guchar       c[MAX_CHANNELS];
  guchar       i;

  g_return_if_fail (GIMP_IS_DRAWABLE (drawable));

  item   = GIMP_ITEM (drawable);
  gimage = gimp_item_get_image (item);

  g_return_if_fail (gimage != NULL);

  switch (GIMP_IMAGE_TYPE_BASE_TYPE (gimp_drawable_type (drawable)))
    {
    case GIMP_RGB:
      gimp_rgba_get_uchar (color,
			   &c[RED_PIX],
			   &c[GREEN_PIX],
			   &c[BLUE_PIX],
			   &c[ALPHA_PIX]);
      if (gimp_drawable_type (drawable) != GIMP_RGBA_IMAGE)
	c[ALPHA_PIX] = 255;
      break;

    case GIMP_GRAY:
      gimp_rgba_get_uchar (color,
			   &c[GRAY_PIX],
			   NULL,
			   NULL,
			   &c[ALPHA_G_PIX]);
      if (gimp_drawable_type (drawable) != GIMP_GRAYA_IMAGE)
	c[ALPHA_G_PIX] = 255;
      break;

    case GIMP_INDEXED:
      gimp_rgb_get_uchar (color,
			  &c[RED_PIX],
			  &c[GREEN_PIX],
			  &c[BLUE_PIX]);
      gimp_image_transform_color (gimage, drawable, c, &i, GIMP_RGB);
      c[INDEXED_PIX] = i;
      if (gimp_drawable_type (drawable) == GIMP_INDEXEDA_IMAGE)
	gimp_rgba_get_uchar (color,
			     NULL,
			     NULL,
			     NULL,
			     &c[ALPHA_I_PIX]);
      break;

    default:
      g_warning ("%s: Cannot fill unknown image type.", 
                 G_GNUC_PRETTY_FUNCTION);
      break;
    }

  pixel_region_init (&destPR,
		     gimp_drawable_data (drawable),
		     0, 0,
		     gimp_item_width  (item),
		     gimp_item_height (item),
		     TRUE);
  color_region (&destPR, c);

  gimp_drawable_update (drawable,
			0, 0,
			gimp_item_width  (item),
			gimp_item_height (item));
}

void
gimp_drawable_fill_by_type (GimpDrawable *drawable,
			    GimpContext  *context,
			    GimpFillType  fill_type)
{
  GimpRGB color;

  g_return_if_fail (GIMP_IS_DRAWABLE (drawable));

  color.a = GIMP_OPACITY_OPAQUE;

  switch (fill_type)
    {
    case GIMP_FOREGROUND_FILL:
      gimp_context_get_foreground (context, &color);
      break;

    case  GIMP_BACKGROUND_FILL:
      gimp_context_get_background (context, &color);
      break;

    case GIMP_WHITE_FILL:
      gimp_rgb_set (&color, 1.0, 1.0, 1.0);
      break;

    case GIMP_TRANSPARENT_FILL:
      gimp_rgba_set (&color, 0.0, 0.0, 0.0, GIMP_OPACITY_TRANSPARENT);
      break;

    case  GIMP_NO_FILL:
      return;

    default:
      g_warning ("%s: unknown fill type %d", G_GNUC_PRETTY_FUNCTION, fill_type);
      return;
    }

  gimp_drawable_fill (drawable, &color);
}

gboolean
gimp_drawable_mask_bounds (GimpDrawable *drawable, 
			   gint         *x1,
			   gint         *y1,
			   gint         *x2,
			   gint         *y2)
{
  GimpItem  *item;
  GimpImage *gimage;

  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), FALSE);
  g_return_val_if_fail (x1 != NULL, FALSE);
  g_return_val_if_fail (y1 != NULL, FALSE);
  g_return_val_if_fail (x2 != NULL, FALSE);
  g_return_val_if_fail (y2 != NULL, FALSE);

  item   = GIMP_ITEM (drawable);
  gimage = gimp_item_get_image (item);

  g_return_val_if_fail (gimage != NULL, FALSE);

  if (gimp_image_mask_bounds (gimage, x1, y1, x2, y2))
    {
      gint off_x, off_y;

      gimp_item_offsets (item, &off_x, &off_y);

      *x1 = CLAMP (*x1 - off_x, 0, gimp_item_width  (item));
      *y1 = CLAMP (*y1 - off_y, 0, gimp_item_height (item));
      *x2 = CLAMP (*x2 - off_x, 0, gimp_item_width  (item));
      *y2 = CLAMP (*y2 - off_y, 0, gimp_item_height (item));
      return TRUE;
    }
  else
    {
      *x2 = gimp_item_width  (item);
      *y2 = gimp_item_height (item);
      return FALSE;
    }
}

gboolean
gimp_drawable_has_alpha (const GimpDrawable *drawable)
{
  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), FALSE);

  return drawable->has_alpha;
}

GimpImageType
gimp_drawable_type (const GimpDrawable *drawable)
{
  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), -1);

  return drawable->type;
}

GimpImageType
gimp_drawable_type_with_alpha (const GimpDrawable *drawable)
{
  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), -1);

  return GIMP_IMAGE_TYPE_WITH_ALPHA (gimp_drawable_type (drawable));
}

gboolean
gimp_drawable_is_rgb (const GimpDrawable *drawable)
{
  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), FALSE);

  return GIMP_IMAGE_TYPE_IS_RGB (gimp_drawable_type (drawable));
}

gboolean
gimp_drawable_is_gray (const GimpDrawable *drawable)
{
  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), FALSE);

  return GIMP_IMAGE_TYPE_IS_GRAY (gimp_drawable_type (drawable));
}

gboolean
gimp_drawable_is_indexed (const GimpDrawable *drawable)
{
  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), FALSE);

  return GIMP_IMAGE_TYPE_IS_INDEXED (gimp_drawable_type (drawable));
}

TileManager *
gimp_drawable_data (const GimpDrawable *drawable)
{
  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), NULL);

  return drawable->tiles;
}

TileManager *
gimp_drawable_shadow (GimpDrawable *drawable)
{
  GimpItem  *item;
  GimpImage *gimage;

  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), NULL);

  item = GIMP_ITEM (drawable);

  if (! (gimage = gimp_item_get_image (item)))
    return NULL;

  return gimp_image_shadow (gimage, item->width, item->height,
			    drawable->bytes);
}

gint
gimp_drawable_bytes (const GimpDrawable *drawable)
{
  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), -1);

  return drawable->bytes;
}

gint
gimp_drawable_bytes_with_alpha (const GimpDrawable *drawable)
{
  GimpImageType type;

  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), -1);

  type = GIMP_IMAGE_TYPE_WITH_ALPHA (gimp_drawable_type (drawable));

  return GIMP_IMAGE_TYPE_BYTES (type);
}

gboolean
gimp_drawable_get_visible (const GimpDrawable *drawable)
{
  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), FALSE);

  return drawable->visible;
}

void
gimp_drawable_set_visible (GimpDrawable *drawable,
                           gboolean      visible,
                           gboolean      push_undo)
{
  g_return_if_fail (GIMP_IS_DRAWABLE (drawable));

  if (drawable->visible != visible)
    {
      GimpItem *item = GIMP_ITEM (drawable);

      if (push_undo)
        {
          GimpImage *gimage = gimp_item_get_image (item);

          if (gimage)
            gimp_image_undo_push_drawable_visibility (gimage, NULL, drawable);
        }

      drawable->visible = visible ? TRUE : FALSE;

      g_signal_emit (drawable, gimp_drawable_signals[VISIBILITY_CHANGED], 0);

      gimp_drawable_update (drawable, 0, 0, item->width, item->height);
    }
}

void
gimp_drawable_alpha_changed (GimpDrawable *drawable)
{
  g_return_if_fail (GIMP_IS_DRAWABLE (drawable));

  g_signal_emit (drawable, gimp_drawable_signals[ALPHA_CHANGED], 0);
}

guchar *
gimp_drawable_cmap (const GimpDrawable *drawable)
{
  GimpImage *gimage;

  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), NULL);

  if (! (gimage = gimp_item_get_image (GIMP_ITEM (drawable))))
    return NULL;

  return gimage->cmap;
}

guchar *
gimp_drawable_get_color_at (GimpDrawable *drawable,
			    gint          x,
			    gint          y)
{
  Tile   *tile;
  guchar *src;
  guchar *dest;

  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), NULL);
  g_return_val_if_fail (gimp_item_get_image (GIMP_ITEM (drawable)) ||
			! gimp_drawable_is_indexed (drawable), NULL);

  /* do not make this a g_return_if_fail() */
  if (! (x >= 0 && x < GIMP_ITEM (drawable)->width &&
         y >= 0 && y < GIMP_ITEM (drawable)->height))
    return NULL;

  dest = g_new (guchar, 5);

  tile = tile_manager_get_tile (gimp_drawable_data (drawable), x, y,
				TRUE, FALSE);
  src = tile_data_pointer (tile, x % TILE_WIDTH, y % TILE_HEIGHT);

  gimp_image_get_color (gimp_item_get_image (GIMP_ITEM (drawable)),
			gimp_drawable_type (drawable), dest, src);

  if (GIMP_IMAGE_TYPE_HAS_ALPHA (gimp_drawable_type (drawable)))
    dest[3] = src[gimp_drawable_bytes (drawable) - 1];
  else
    dest[3] = OPAQUE_OPACITY;

  if (gimp_drawable_is_indexed (drawable))
    dest[4] = src[0];
  else
    dest[4] = 0;

  tile_release (tile, FALSE);

  return dest;
}
