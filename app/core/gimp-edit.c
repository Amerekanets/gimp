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

#include "core-types.h"

#include "base/pixel-region.h"
#include "base/temp-buf.h"
#include "base/tile-manager.h"
#include "base/tile-manager-crop.h"

#include "paint-funcs/paint-funcs.h"

#include "gimp.h"
#include "gimp-edit.h"
#include "gimp-utils.h"
#include "gimpbuffer.h"
#include "gimpchannel.h"
#include "gimpcontext.h"
#include "gimpimage.h"
#include "gimpimage-undo.h"
#include "gimplayer.h"
#include "gimplayer-floating-sel.h"
#include "gimplist.h"
#include "gimppattern.h"
#include "gimpselection.h"

#include "gimp-intl.h"


/*  local function protypes  */

static const GimpBuffer * gimp_edit_extract       (GimpImage    *gimage,
                                                   GimpDrawable *drawable,
                                                   gboolean      cut_pixels);
static gboolean           gimp_edit_fill_internal (GimpImage    *gimage,
                                                   GimpDrawable *drawable,
                                                   GimpContext  *context,
                                                   GimpFillType  fill_type,
                                                   const gchar  *undo_desc);


/*  public functions  */

const GimpBuffer *
gimp_edit_cut (GimpImage    *gimage,
	       GimpDrawable *drawable)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);
  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), NULL);

  return gimp_edit_extract (gimage, drawable, TRUE);
}

const GimpBuffer *
gimp_edit_copy (GimpImage    *gimage,
		GimpDrawable *drawable)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);
  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), NULL);

  return gimp_edit_extract (gimage, drawable, FALSE);
}

GimpLayer *
gimp_edit_paste (GimpImage    *gimage,
		 GimpDrawable *drawable,
		 GimpBuffer   *paste,
		 gboolean      paste_into,
                 gint          viewport_x,
                 gint          viewport_y,
                 gint          viewport_width,
                 gint          viewport_height)
{
  GimpLayer     *layer;
  GimpImageType  type;
  gint           center_x;
  gint           center_y;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);
  g_return_val_if_fail (drawable == NULL || GIMP_IS_DRAWABLE (drawable), NULL);
  g_return_val_if_fail (GIMP_IS_BUFFER (paste), NULL);

  /*  Make a new layer: if drawable == NULL,
   *  user is pasting into an empty image.
   */

  if (drawable)
    type = gimp_drawable_type_with_alpha (drawable);
  else
    type = gimp_image_base_type_with_alpha (gimage);

  layer = gimp_layer_new_from_tiles (paste->tiles,
                                     gimage,
                                     type,
                                     _("Pasted Layer"),
                                     GIMP_OPACITY_OPAQUE, GIMP_NORMAL_MODE);

  if (! layer)
    return NULL;

  /*  Start a group undo  */
  gimp_image_undo_group_start (gimage, GIMP_UNDO_GROUP_EDIT_PASTE,
                               _("Paste"));

  if (drawable)
    {
      /*  if pasting to a drawable  */

      gint     off_x, off_y;
      gint     x1, y1, x2, y2;
      gint     paste_x, paste_y;
      gint     paste_width, paste_height;
      gboolean have_mask;

      gimp_item_offsets (GIMP_ITEM (drawable), &off_x, &off_y);
      have_mask = gimp_drawable_mask_bounds (drawable, &x1, &y1, &x2, &y2);

      if (! have_mask         &&
          viewport_width  > 0 &&
          viewport_height > 0 &&
          gimp_rectangle_intersect (viewport_x, viewport_y,
                                    viewport_width, viewport_height,
                                    off_x, off_y,
                                    x2 - x1, y2 - y1,
                                    &paste_x, &paste_y,
                                    &paste_width, &paste_height))
        {
          center_x = paste_x + paste_width  / 2;
          center_y = paste_y + paste_height / 2;
        }
      else
        {
          center_x = off_x + (x1 + x2) / 2;
          center_y = off_y + (y1 + y2) / 2;
        }
    }
  else if (viewport_width > 0 && viewport_height > 0)
    {
      /*  if we got a viewport set the offsets to the center of the viewport  */

      center_x = viewport_x + viewport_width  / 2;
      center_y = viewport_y + viewport_height / 2;
    }
  else
    {
      /*  otherwise the offsets to the center of the image  */

      center_x = gimage->width  / 2;
      center_y = gimage->height / 2;
    }

  GIMP_ITEM (layer)->offset_x = center_x - (GIMP_ITEM (layer)->width  / 2);
  GIMP_ITEM (layer)->offset_y = center_y - (GIMP_ITEM (layer)->height / 2);

  /*  If there is a selection mask clear it--
   *  this might not always be desired, but in general,
   *  it seems like the correct behavior.
   */
  if (! gimp_channel_is_empty (gimp_image_get_mask (gimage)) && ! paste_into)
    gimp_channel_clear (gimp_image_get_mask (gimage), NULL, TRUE);

  /*  if there's a drawable, add a new floating selection  */
  if (drawable)
    floating_sel_attach (layer, drawable);
  else
    gimp_image_add_layer (gimage, layer, 0);

  /*  end the group undo  */
  gimp_image_undo_group_end (gimage);

  return layer;
}

GimpImage *
gimp_edit_paste_as_new (Gimp       *gimp,
			GimpImage  *invoke,
			GimpBuffer *paste)
{
  GimpImage    *gimage;
  GimpLayer    *layer;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);
  g_return_val_if_fail (invoke == NULL || GIMP_IS_IMAGE (invoke), NULL);
  g_return_val_if_fail (GIMP_IS_BUFFER (paste), NULL);

  /*  create a new image  (always of type GIMP_RGB)  */
  gimage = gimp_create_image (gimp,
			      gimp_buffer_get_width (paste),
                              gimp_buffer_get_height (paste),
			      GIMP_RGB,
			      TRUE);
  gimp_image_undo_disable (gimage);

  if (invoke)
    {
      gimp_image_set_resolution (gimage,
				 invoke->xresolution, invoke->yresolution);
      gimp_image_set_unit (gimage, invoke->unit);
    }

  layer = gimp_layer_new_from_tiles (paste->tiles,
                                     gimage,
                                     gimp_image_base_type_with_alpha (gimage),
				     _("Pasted Layer"),
				     GIMP_OPACITY_OPAQUE, GIMP_NORMAL_MODE);

  if (! layer)
    {
      g_object_unref (gimage);
      return NULL;
    }

  gimp_image_add_layer (gimage, layer, 0);

  gimp_image_undo_enable (gimage);

  gimp_create_display (gimp, gimage, 0x0101);
  g_object_unref (gimage);

  return gimage;
}

gboolean
gimp_edit_clear (GimpImage    *gimage,
		 GimpDrawable *drawable)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), FALSE);

  return gimp_edit_fill_internal (gimage, drawable,
                                  gimp_get_current_context (gimage->gimp),
                                  GIMP_TRANSPARENT_FILL,
                                  _("Clear"));
}

gboolean
gimp_edit_fill (GimpImage    *gimage,
		GimpDrawable *drawable,
		GimpFillType  fill_type)
{
  const gchar *undo_desc;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), FALSE);

  switch (fill_type)
    {
    case GIMP_FOREGROUND_FILL:
      undo_desc = _("Fill with FG Color");
      break;

    case GIMP_BACKGROUND_FILL:
      undo_desc = _("Fill with BG Color");
      break;

    case GIMP_WHITE_FILL:
      undo_desc = _("Fill with White");
      break;

    case GIMP_TRANSPARENT_FILL:
      undo_desc = _("Fill with Transparency");
      break;

    case GIMP_PATTERN_FILL:
      undo_desc = _("Fill with Pattern");
      break;

    case GIMP_NO_FILL:
      return TRUE;  /*  nothing to do, but the fill succeded  */

    default:
      g_warning ("%s: unknown fill type", G_GNUC_PRETTY_FUNCTION);
      fill_type = GIMP_BACKGROUND_FILL;
      undo_desc = _("Fill with BG Color");
      break;
    }

  return gimp_edit_fill_internal (gimage, drawable,
                                  gimp_get_current_context (gimage->gimp),
                                  fill_type, undo_desc);
}


/*  private functions  */

const GimpBuffer *
gimp_edit_extract (GimpImage    *gimage,
                   GimpDrawable *drawable,
                   gboolean      cut_pixels)
{
  TileManager *tiles;
  gboolean     empty;

  /*  See if the gimage mask is empty  */
  empty = gimp_channel_is_empty (gimp_image_get_mask (gimage));

  if (cut_pixels)
    gimp_image_undo_group_start (gimage, GIMP_UNDO_GROUP_EDIT_CUT, _("Cut"));

  /*  Cut/copy the mask portion from the gimage  */
  tiles = gimp_selection_extract (gimp_image_get_mask (gimage),
                                  drawable, cut_pixels, FALSE, TRUE);

  if (cut_pixels)
    gimp_image_undo_group_end (gimage);

  if (tiles)
    {
      /*  Only crop if the gimage mask wasn't empty  */
      if (! empty)
        {
          TileManager *crop;

          crop = tile_manager_crop (tiles, 0);

          if (crop != tiles)
            {
              tile_manager_unref (tiles);
              tiles = crop;
            }
        }

      /*  Set the global edit buffer  */
      if (gimage->gimp->global_buffer)
	g_object_unref (gimage->gimp->global_buffer);

      gimage->gimp->global_buffer = gimp_buffer_new (tiles, "Global Buffer",
                                                     FALSE);

      gimage->gimp->have_current_cut_buffer = TRUE;

      return gimage->gimp->global_buffer;
    }

  return NULL;
}

static gboolean
gimp_edit_fill_internal (GimpImage    *gimage,
                         GimpDrawable *drawable,
                         GimpContext  *context,
                         GimpFillType  fill_type,
                         const gchar  *undo_desc)
{
  TileManager *buf_tiles;
  PixelRegion  bufPR;
  gint         x1, y1, x2, y2;
  gint         tiles_bytes;
  guchar       col[MAX_CHANNELS];
  TempBuf     *pat_buf = NULL;
  gboolean     new_buf;

  gimp_drawable_mask_bounds (drawable, &x1, &y1, &x2, &y2);

  if (x1 == x2 || y1 == y2)
    return TRUE;  /*  nothing to do, but the fill succeded  */

  tiles_bytes = gimp_drawable_bytes (drawable);

  switch (fill_type)
    {
    case GIMP_FOREGROUND_FILL:
      gimp_image_get_foreground (gimage, drawable, col);
      break;

    case GIMP_BACKGROUND_FILL:
    case GIMP_TRANSPARENT_FILL:
      gimp_image_get_background (gimage, drawable, col);
      break;

    case GIMP_WHITE_FILL:
      {
        guchar tmp_col[MAX_CHANNELS];

        tmp_col[RED_PIX]   = 255;
        tmp_col[GREEN_PIX] = 255;
        tmp_col[BLUE_PIX]  = 255;
        gimp_image_transform_color (gimage, drawable, col, GIMP_RGB, tmp_col);
      }
      break;

    case GIMP_PATTERN_FILL:
      {
        GimpPattern *pattern = gimp_context_get_pattern (context);

        pat_buf = gimp_image_transform_temp_buf (gimage, drawable,
                                                 pattern->mask, &new_buf);

        if (! gimp_drawable_has_alpha (drawable) &&
            (pat_buf->bytes == 2 || pat_buf->bytes == 4))
          tiles_bytes++;
      }
      break;

    case GIMP_NO_FILL:
      return TRUE;  /*  nothing to do, but the fill succeded  */
    }

  buf_tiles = tile_manager_new (x2 - x1, y2 - y1, tiles_bytes);

  pixel_region_init (&bufPR, buf_tiles, 0, 0, x2 - x1, y2 - y1, TRUE);

  if (pat_buf)
    {
      pattern_region (&bufPR, NULL, pat_buf, 0, 0);

      if (new_buf)
        temp_buf_free (pat_buf);
    }
  else
    {
      if (gimp_drawable_has_alpha (drawable))
        col[gimp_drawable_bytes (drawable) - 1] = OPAQUE_OPACITY;

      color_region (&bufPR, col);
    }

  pixel_region_init (&bufPR, buf_tiles, 0, 0, x2 - x1, y2 - y1, FALSE);
  gimp_drawable_apply_region (drawable, &bufPR,
                              TRUE, undo_desc,
                              GIMP_OPACITY_OPAQUE,
                              (fill_type == GIMP_TRANSPARENT_FILL) ?
                              GIMP_ERASE_MODE : GIMP_NORMAL_MODE,
                              NULL, x1, y1);

  tile_manager_unref (buf_tiles);

  gimp_drawable_update (drawable, x1, y1, x2 - x1, y2 - y1);

  return TRUE;
}
