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

#include "gimp.h"
#include "gimpimage.h"
#include "gimpimage-mask.h"
#include "gimpimage-projection.h"
#include "gimpimage-flip.h"
#include "gimpimage-guides.h"
#include "gimpimage-undo.h"
#include "gimpimage-undo-push.h"
#include "gimpitem.h"
#include "gimplayer-floating-sel.h"
#include "gimplist.h"


void
gimp_image_flip (GimpImage           *gimage, 
                 GimpOrientationType  flip_type,
                 GimpProgressFunc     progress_func,
                 gpointer             progress_data)
{
  GimpLayer *floating_layer;
  GimpItem  *item;
  GList     *list;
  gdouble    axis;
  gint       progress_max;
  gint       progress_current = 1;

  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  gimp_set_busy (gimage->gimp);

  switch (flip_type)
    {
    case GIMP_ORIENTATION_HORIZONTAL:
      axis = (gdouble) gimage->width / 2.0;
      break;

    case GIMP_ORIENTATION_VERTICAL:
      axis = (gdouble) gimage->height / 2.0;
      break;

    default:
      g_warning ("gimp_image_flip(): unknown flip_type");
      return;
    }

  progress_max = (gimage->channels->num_children +
                  gimage->layers->num_children   +
                  gimage->vectors->num_children  +
                  1 /* selection */);

  /*  Get the floating layer if one exists  */
  floating_layer = gimp_image_floating_sel (gimage);

  gimp_image_undo_group_start (gimage, GIMP_UNDO_GROUP_IMAGE_FLIP, NULL);

  /*  Relax the floating selection  */
  if (floating_layer)
    floating_sel_relax (floating_layer, TRUE);

  /*  Flip all channels  */
  for (list = GIMP_LIST (gimage->channels)->list; 
       list; 
       list = g_list_next (list))
    {
      item = (GimpItem *) list->data;

      gimp_item_flip (item, flip_type, axis, TRUE);

      if (progress_func)
        (* progress_func) (0, progress_max, progress_current++, progress_data);
    }

  /*  Flip all vectors  */
  for (list = GIMP_LIST (gimage->vectors)->list; 
       list; 
       list = g_list_next (list))
    {
      item = (GimpItem *) list->data;

      gimp_item_flip (item, flip_type, axis, FALSE);

      if (progress_func)
        (* progress_func) (0, progress_max, progress_current++, progress_data);
    }

  /*  Don't forget the selection mask!  */
  gimp_item_flip (GIMP_ITEM (gimage->selection_mask), flip_type, axis, TRUE);
  gimp_image_mask_invalidate (gimage);

  if (progress_func)
    (* progress_func) (0, progress_max, progress_current++, progress_data);

  /*  Flip all layers  */
  for (list = GIMP_LIST (gimage->layers)->list; 
       list; 
       list = g_list_next (list))
    {
      item = (GimpItem *) list->data;

      gimp_item_flip (item, flip_type, axis, FALSE);

      if (progress_func)
        (* progress_func) (0, progress_max, progress_current++, progress_data);
    }

  /*  Flip all Guides  */
  for (list = gimage->guides; list; list = g_list_next (list))
    {
      GimpGuide *guide = list->data;

      switch (guide->orientation)
	{
	case GIMP_ORIENTATION_HORIZONTAL:
          if (flip_type == GIMP_ORIENTATION_VERTICAL)
            gimp_image_move_guide (gimage, guide,
                                   gimage->height - guide->position, TRUE);
	  break;

	case GIMP_ORIENTATION_VERTICAL:
          if (flip_type == GIMP_ORIENTATION_HORIZONTAL)
            gimp_image_move_guide (gimage, guide,
                                   gimage->width - guide->position, TRUE);
	  break;

	default:
          break;
	}
    }

  /*  Make sure the projection matches the gimage size  */
  gimp_image_projection_allocate (gimage);

  /*  Rigor the floating selection  */
  if (floating_layer)
    floating_sel_rigor (floating_layer, TRUE);

  gimp_image_undo_group_end (gimage);

  gimp_image_mask_changed (gimage);

  gimp_unset_busy (gimage->gimp);
}
