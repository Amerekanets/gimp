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

#include "gimpimage.h"
#include "gimpitem.h"
#include "gimpitem-linked.h"
#include "gimplist.h"


typedef enum
{
  GIMP_ITEM_LINKED_LAYERS   = 1 << 0,
  GIMP_ITEM_LINKED_CHANNELS = 1 << 1,
  GIMP_ITEM_LINKED_VECTORS  = 1 << 2,

  GIMP_ITEM_LINKED_ALL      = (GIMP_ITEM_LINKED_LAYERS   |
                               GIMP_ITEM_LINKED_CHANNELS |
                               GIMP_ITEM_LINKED_VECTORS)
} GimpItemLinkedMask;


/*  local function prototypes  */

static GList * gimp_item_linked_get_list (GimpImage          *gimage,
                                          GimpItem           *item,
                                          GimpItemLinkedMask  which);


/*  public functions  */

void
gimp_item_linked_translate (GimpItem *item,
                            gint      offset_x,
                            gint      offset_y,
                            gboolean  push_undo)
{
  GimpImage *gimage;
  GList     *linked_list;
  GList     *list;

  g_return_if_fail (GIMP_IS_ITEM (item));
  g_return_if_fail (gimp_item_get_linked (item) == TRUE);

  gimage = gimp_item_get_image (item);

  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  linked_list = gimp_item_linked_get_list (gimage, item, GIMP_ITEM_LINKED_ALL);

  for (list = linked_list; list; list = g_list_next (list))
    gimp_item_translate (GIMP_ITEM (list->data),
                         offset_x, offset_y, push_undo);

  g_list_free (linked_list);
}

void
gimp_item_linked_flip (GimpItem            *item,
                       GimpOrientationType  flip_type,
                       gdouble              axis,
                       gboolean             clip_result)
{
  GimpImage *gimage;
  GList     *linked_list;
  GList     *list;

  g_return_if_fail (GIMP_IS_ITEM (item));
  g_return_if_fail (gimp_item_get_linked (item) == TRUE);

  gimage = gimp_item_get_image (item);

  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  linked_list = gimp_item_linked_get_list (gimage, item, GIMP_ITEM_LINKED_ALL);

  for (list = linked_list; list; list = g_list_next (list))
    gimp_item_flip (GIMP_ITEM (list->data),
                    flip_type, axis, clip_result);

  g_list_free (linked_list);
}

void
gimp_item_linked_rotate (GimpItem         *item,
                         GimpRotationType  rotate_type,
                         gdouble           center_x,
                         gdouble           center_y,
                         gboolean          clip_result)
{
  GimpImage *gimage;
  GList     *linked_list;
  GList     *list;

  g_return_if_fail (GIMP_IS_ITEM (item));
  g_return_if_fail (gimp_item_get_linked (item) == TRUE);

  gimage = gimp_item_get_image (item);

  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  linked_list = gimp_item_linked_get_list (gimage, item,
                                           GIMP_ITEM_LINKED_LAYERS |
                                           GIMP_ITEM_LINKED_VECTORS);

  for (list = linked_list; list; list = g_list_next (list))
    gimp_item_rotate (GIMP_ITEM (list->data),
                      rotate_type, center_x, center_y, clip_result);

  g_list_free (linked_list);

  linked_list = gimp_item_linked_get_list (gimage, item,
                                           GIMP_ITEM_LINKED_CHANNELS);

  for (list = linked_list; list; list = g_list_next (list))
    gimp_item_rotate (GIMP_ITEM (list->data),
                      rotate_type, center_x, center_y, TRUE);

  g_list_free (linked_list);
}

void
gimp_item_linked_transform (GimpItem               *item,
                            GimpMatrix3             matrix,
                            GimpTransformDirection  direction,
                            GimpInterpolationType   interpolation_type,
                            gboolean                clip_result,
                            GimpProgressFunc        progress_callback,
                            gpointer                progress_data)
{
  GimpImage *gimage;
  GList     *linked_list;
  GList     *list;

  g_return_if_fail (GIMP_IS_ITEM (item));
  g_return_if_fail (gimp_item_get_linked (item) == TRUE);

  gimage = gimp_item_get_image (item);

  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  linked_list = gimp_item_linked_get_list (gimage, item, GIMP_ITEM_LINKED_ALL);

  for (list = linked_list; list; list = g_list_next (list))
    gimp_item_transform (GIMP_ITEM (list->data),
                         matrix, direction,
                         interpolation_type, clip_result,
                         progress_callback, progress_data);

  g_list_free (linked_list);
}


/*  private functions  */

static GList *
gimp_item_linked_get_list (GimpImage          *gimage,
                           GimpItem           *item,
                           GimpItemLinkedMask  which)
{
  GimpItem *linked_item;
  GList    *list;
  GList    *linked_list = NULL;

  if (which & GIMP_ITEM_LINKED_LAYERS)
    {
      for (list = GIMP_LIST (gimage->layers)->list;
           list;
           list = g_list_next (list))
        {
          linked_item = (GimpItem *) list->data;

          if (linked_item != item && gimp_item_get_linked (linked_item))
            linked_list = g_list_prepend (linked_list, linked_item);
        }
    }

  if (which & GIMP_ITEM_LINKED_CHANNELS)
    {
      for (list = GIMP_LIST (gimage->channels)->list;
           list;
           list = g_list_next (list))
        {
          linked_item = (GimpItem *) list->data;

          if (linked_item != item && gimp_item_get_linked (linked_item))
            linked_list = g_list_prepend (linked_list, linked_item);
        }
    }

  if (which & GIMP_ITEM_LINKED_VECTORS)
    {
      for (list = GIMP_LIST (gimage->vectors)->list;
           list;
           list = g_list_next (list))
        {
          linked_item = (GimpItem *) list->data;

          if (linked_item != item && gimp_item_get_linked (linked_item))
            linked_list = g_list_prepend (linked_list, linked_item);
        }
    }

  return g_list_reverse (linked_list);
}
