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

#include <gtk/gtk.h>

#include "libgimpwidgets/gimpwidgets.h"

#include "widgets-types.h"

#include "core/gimp.h"
#include "core/gimp-edit.h"
#include "core/gimpbuffer.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-colormap.h"
#include "core/gimplayer.h"
#include "core/gimplayermask.h"
#include "core/gimptoolinfo.h"

#include "gimpdnd.h"
#include "gimptoolbox.h"
#include "gimptoolbox-dnd.h"


/*  local function prototypes  */

static void   gimp_toolbox_drop_drawable (GtkWidget    *widget,
                                          GimpViewable *viewable,
                                          gpointer      data);
static void   gimp_toolbox_drop_tool     (GtkWidget    *widget,
                                          GimpViewable *viewable,
                                          gpointer      data);
static void   gimp_toolbox_drop_buffer   (GtkWidget    *widget,
                                          GimpViewable *viewable,
                                          gpointer      data);


/*  public functions  */

void
gimp_toolbox_dnd_init (GimpToolbox *toolbox)
{
  GimpDock *dock;

  g_return_if_fail (GIMP_IS_TOOLBOX (toolbox));

  dock = GIMP_DOCK (toolbox);

  gimp_dnd_file_dest_add (GTK_WIDGET (toolbox), gimp_dnd_open_files, NULL);

  gimp_dnd_file_dest_add (toolbox->wbox, gimp_dnd_open_files, NULL);

  gimp_dnd_viewable_dest_add (toolbox->wbox, GIMP_TYPE_LAYER,
			      gimp_toolbox_drop_drawable,
			      dock->context);
  gimp_dnd_viewable_dest_add (toolbox->wbox, GIMP_TYPE_LAYER_MASK,
			      gimp_toolbox_drop_drawable,
			      dock->context);
  gimp_dnd_viewable_dest_add (toolbox->wbox, GIMP_TYPE_CHANNEL,
			      gimp_toolbox_drop_drawable,
			      dock->context);
  gimp_dnd_viewable_dest_add (toolbox->wbox, GIMP_TYPE_TOOL_INFO,
			      gimp_toolbox_drop_tool,
			      dock->context);
  gimp_dnd_viewable_dest_add (toolbox->wbox, GIMP_TYPE_BUFFER,
			      gimp_toolbox_drop_buffer,
			      dock->context);
}


/*  private functions  */

static void
gimp_toolbox_drop_drawable (GtkWidget    *widget,
                            GimpViewable *viewable,
                            gpointer      data)
{
  GimpDrawable      *drawable;
  GimpItem          *item;
  GimpImage         *gimage;
  GimpImage         *new_gimage;
  GimpLayer         *new_layer;
  gint               width, height;
  gint               off_x, off_y;
  gint               bytes;
  GimpImageBaseType  type;

  drawable = GIMP_DRAWABLE (viewable);
  item     = GIMP_ITEM (viewable);
  gimage   = gimp_item_get_image (item);

  width  = gimp_item_width  (item);
  height = gimp_item_height (item);
  bytes  = gimp_drawable_bytes (drawable);

  type = GIMP_IMAGE_TYPE_BASE_TYPE (gimp_drawable_type (drawable));

  new_gimage = gimp_create_image (gimage->gimp,
				  width, height,
				  type,
				  FALSE);
  gimp_image_undo_disable (new_gimage);

  if (type == GIMP_INDEXED) /* copy the colormap */
    gimp_image_set_colormap (new_gimage,
                             gimp_image_get_colormap (gimage),
                             gimp_image_get_colormap_size (gimage),
                             FALSE);

  gimp_image_set_resolution (new_gimage,
			     gimage->xresolution, gimage->yresolution);
  gimp_image_set_unit (new_gimage, gimage->unit);

  if (GIMP_IS_LAYER (drawable))
    {
      new_layer = GIMP_LAYER (gimp_item_duplicate (GIMP_ITEM (drawable),
                                                   G_TYPE_FROM_INSTANCE (drawable),
                                                   FALSE));
    }
  else
    {
      new_layer = GIMP_LAYER (gimp_item_duplicate (GIMP_ITEM (drawable),
                                                   GIMP_TYPE_LAYER,
                                                   TRUE));
    }

  gimp_item_set_image (GIMP_ITEM (new_layer), new_gimage);

  gimp_object_set_name (GIMP_OBJECT (new_layer),
			gimp_object_get_name (GIMP_OBJECT (drawable)));

  gimp_item_offsets (GIMP_ITEM (new_layer), &off_x, &off_y);
  gimp_item_translate (GIMP_ITEM (new_layer), -off_x, -off_y, FALSE);

  gimp_image_add_layer (new_gimage, new_layer, 0);

  gimp_image_undo_enable (new_gimage);

  gimp_create_display (gimage->gimp, new_gimage, 0x0101);

  g_object_unref (new_gimage);
}

static void
gimp_toolbox_drop_tool (GtkWidget    *widget,
                        GimpViewable *viewable,
                        gpointer      data)
{
  GimpContext *context = GIMP_CONTEXT (data);

  gimp_context_set_tool (context, GIMP_TOOL_INFO (viewable));
}

static void
gimp_toolbox_drop_buffer (GtkWidget    *widget,
                          GimpViewable *viewable,
                          gpointer      data)
{
  GimpContext *context = GIMP_CONTEXT (data);

  if (context->gimp->busy)
    return;

  gimp_edit_paste_as_new (context->gimp, NULL, GIMP_BUFFER (viewable));
}
