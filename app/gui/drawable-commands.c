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

#include <gtk/gtk.h>

#include "libgimpwidgets/gimpwidgets.h"

#include "gui-types.h"

#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpdrawable.h"
#include "core/gimpdrawable-desaturate.h"
#include "core/gimpdrawable-equalize.h"
#include "core/gimpdrawable-invert.h"
#include "core/gimpimage.h"

#include "widgets/gimpitemtreeview.h"

#include "display/gimpdisplay.h"

#include "drawable-commands.h"
#include "offset-dialog.h"

#include "gimp-intl.h"


#define return_if_no_image(gimage,data) \
  if (GIMP_IS_DISPLAY (data)) \
    gimage = ((GimpDisplay *) data)->gimage; \
  else if (GIMP_IS_GIMP (data)) \
    gimage = gimp_context_get_image (gimp_get_user_context (GIMP (data))); \
  else if (GIMP_IS_ITEM_TREE_VIEW (data)) \
    gimage = ((GimpItemTreeView *) data)->gimage; \
  else \
    gimage = NULL; \
  \
  if (! gimage) \
    return

#define return_if_no_drawable(gimage,drawable,data) \
  return_if_no_image (gimage,data); \
  drawable = gimp_image_active_drawable (gimage); \
  if (! drawable) \
    return


/*  public functions  */

void
drawable_desaturate_cmd_callback (GtkWidget *widget,
                                  gpointer   data)
{
  GimpImage    *gimage;
  GimpDrawable *active_drawable;
  return_if_no_drawable (gimage, active_drawable, data);

  if (! gimp_drawable_is_rgb (active_drawable))
    {
      g_message (_("Desaturate operates only on RGB color drawables."));
      return;
    }

  gimp_drawable_desaturate (active_drawable);
  gimp_image_flush (gimage);
}

void
drawable_invert_cmd_callback (GtkWidget *widget,
                              gpointer   data)
{
  GimpImage    *gimage;
  GimpDrawable *active_drawable;
  return_if_no_drawable (gimage, active_drawable, data);

  if (gimp_drawable_is_indexed (active_drawable))
    {
      g_message (_("Invert does not operate on indexed drawables."));
      return;
    }

  gimp_drawable_invert (active_drawable);
  gimp_image_flush (gimage);
}

void
drawable_equalize_cmd_callback (GtkWidget *widget,
                                gpointer   data)
{
  GimpImage    *gimage;
  GimpDrawable *active_drawable;
  return_if_no_drawable (gimage, active_drawable, data);

  if (gimp_drawable_is_indexed (active_drawable))
    {
      g_message (_("Equalize does not operate on indexed drawables."));
      return;
    }

  gimp_drawable_equalize (active_drawable, TRUE);
  gimp_image_flush (gimage);
}

void
drawable_flip_cmd_callback (GtkWidget *widget,
                            gpointer   data,
                            guint      action)
{
  GimpImage    *gimage;
  GimpDrawable *active_drawable;
  GimpItem     *item;
  gint          off_x, off_y;
  gdouble       axis = 0.0;
  return_if_no_drawable (gimage, active_drawable, data);

  item = GIMP_ITEM (active_drawable);

  gimp_item_offsets (item, &off_x, &off_y);

  switch ((GimpOrientationType) action)
    {
    case GIMP_ORIENTATION_HORIZONTAL:
      axis = ((gdouble) off_x + (gdouble) gimp_item_width (item) / 2.0);
      break;

    case GIMP_ORIENTATION_VERTICAL:
      axis = ((gdouble) off_y + (gdouble) gimp_item_height (item) / 2.0);
      break;

    default:
      break;
    }

  gimp_item_flip (item, (GimpOrientationType) action, axis, TRUE);
  gimp_image_flush (gimage);
}

void
drawable_rotate_cmd_callback (GtkWidget *widget,
                              gpointer   data,
                              guint      action)
{
  GimpImage    *gimage;
  GimpDrawable *active_drawable;
  GimpItem     *item;
  gint          off_x, off_y;
  gdouble       center_x, center_y;
  return_if_no_drawable (gimage, active_drawable, data);

  item = GIMP_ITEM (active_drawable);

  gimp_item_offsets (item, &off_x, &off_y);

  center_x = ((gdouble) off_x + (gdouble) gimp_item_width  (item) / 2.0);
  center_y = ((gdouble) off_y + (gdouble) gimp_item_height (item) / 2.0);

  gimp_item_rotate (item, (GimpRotationType) action, center_x, center_y, TRUE);
  gimp_image_flush (gimage);
}

void
drawable_offset_cmd_callback (GtkWidget *widget,
                              gpointer   data)
{
  GimpImage    *gimage;
  GimpDrawable *active_drawable;
  return_if_no_drawable (gimage, active_drawable, data);

  offset_dialog_create (active_drawable);
}
