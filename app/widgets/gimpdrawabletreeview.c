/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpdrawabletreeview.c
 * Copyright (C) 2001-2003 Michael Natterer <mitch@gimp.org>
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

#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "widgets-types.h"

#include "core/gimp.h"
#include "core/gimpchannel.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimpdrawable-bucket-fill.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimplayer.h"
#include "core/gimpmarshal.h"
#include "core/gimppattern.h"
#include "core/gimptoolinfo.h"

#include "gimpcellrenderertoggle.h"
#include "gimpchanneltreeview.h"
#include "gimpdnd.h"
#include "gimpitemfactory.h"
#include "gimplayertreeview.h"
#include "gimppreviewrenderer.h"

#include "gimp-intl.h"


static void   gimp_drawable_tree_view_class_init (GimpDrawableTreeViewClass *klass);
static void   gimp_drawable_tree_view_init       (GimpDrawableTreeView      *view);

static gboolean gimp_drawable_tree_view_select_item (GimpContainerView *view,
						     GimpViewable      *item,
						     gpointer           insert_data);

static void   gimp_drawable_tree_view_set_image  (GimpItemTreeView     *view,
                                                  GimpImage            *gimage);

static void   gimp_drawable_tree_view_floating_selection_changed
                                                 (GimpImage            *gimage,
                                                  GimpDrawableTreeView *view);

static void   gimp_drawable_tree_view_new_pattern_dropped
                                                 (GtkWidget            *widget,
                                                  GimpViewable         *viewable,
                                                  gpointer              data);
static void   gimp_drawable_tree_view_new_color_dropped
                                                 (GtkWidget            *widget,
                                                  const GimpRGB        *color,
                                                  gpointer              data);


static GimpItemTreeViewClass *parent_class = NULL;


GType
gimp_drawable_tree_view_get_type (void)
{
  static GType view_type = 0;

  if (! view_type)
    {
      static const GTypeInfo view_info =
      {
        sizeof (GimpDrawableTreeViewClass),
        NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) gimp_drawable_tree_view_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GimpDrawableTreeView),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gimp_drawable_tree_view_init,
      };

      view_type = g_type_register_static (GIMP_TYPE_ITEM_TREE_VIEW,
                                          "GimpDrawableTreeView",
                                          &view_info, 0);
    }

  return view_type;
}

static void
gimp_drawable_tree_view_class_init (GimpDrawableTreeViewClass *klass)
{
  GimpContainerViewClass *container_view_class;
  GimpItemTreeViewClass  *item_view_class;

  container_view_class = GIMP_CONTAINER_VIEW_CLASS (klass);
  item_view_class      = GIMP_ITEM_TREE_VIEW_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  container_view_class->select_item   = gimp_drawable_tree_view_select_item;

  item_view_class->set_image          = gimp_drawable_tree_view_set_image;
}

static void
gimp_drawable_tree_view_init (GimpDrawableTreeView *view)
{
  GimpContainerTreeView *tree_view;
  GimpItemTreeView      *item_view;

  tree_view = GIMP_CONTAINER_TREE_VIEW (view);
  item_view = GIMP_ITEM_TREE_VIEW (view);

  gimp_dnd_viewable_dest_add (item_view->new_button, GIMP_TYPE_PATTERN,
			      gimp_drawable_tree_view_new_pattern_dropped,
                              view);
  gimp_dnd_color_dest_add (item_view->new_button,
                           gimp_drawable_tree_view_new_color_dropped,
                           view);
}


/*  GimpContainerView methods  */

static gboolean
gimp_drawable_tree_view_select_item (GimpContainerView *view,
                                     GimpViewable      *item,
                                     gpointer           insert_data)
{
  GimpItemTreeView *item_view;
  gboolean          success = TRUE;

  item_view = GIMP_ITEM_TREE_VIEW (view);

  if (item_view->gimage)
    {
      GimpViewable *floating_sel;

      floating_sel = (GimpViewable *)
        gimp_image_floating_sel (item_view->gimage);

      success = (item == NULL || floating_sel == NULL || item == floating_sel);
    }

  if (success)
    return GIMP_CONTAINER_VIEW_CLASS (parent_class)->select_item (view, item,
                                                                  insert_data);

  return success;
}


/*  GimpItemTreeView methods  */

static void
gimp_drawable_tree_view_set_image (GimpItemTreeView *view,
                                   GimpImage        *gimage)
{
  if (view->gimage)
    g_signal_handlers_disconnect_by_func (view->gimage,
                                          gimp_drawable_tree_view_floating_selection_changed,
                                          view);

  GIMP_ITEM_TREE_VIEW_CLASS (parent_class)->set_image (view, gimage);

  if (view->gimage)
    g_signal_connect (view->gimage,
                      "floating_selection_changed",
                      G_CALLBACK (gimp_drawable_tree_view_floating_selection_changed),
                      view);
}


/*  callbacks  */

static void
gimp_drawable_tree_view_floating_selection_changed (GimpImage            *gimage,
						    GimpDrawableTreeView *view)
{
  GimpItem *item;

  item = GIMP_ITEM_TREE_VIEW_GET_CLASS (view)->get_active_item (gimage);

  /*  update button states  */
  gimp_container_view_select_item (GIMP_CONTAINER_VIEW (view),
                                   (GimpViewable *) item);
}

static void
gimp_drawable_tree_view_new_dropped (GimpItemTreeView   *view,
                                     GimpBucketFillMode  fill_mode,
                                     const GimpRGB      *color,
                                     GimpPattern        *pattern)
{
  GimpDrawable *drawable;
  GimpToolInfo *tool_info;
  GimpContext  *context;

  gimp_image_undo_group_start (view->gimage, GIMP_UNDO_GROUP_EDIT_PASTE,
                               _("New Layer"));

  view->new_item_func (view->gimage, NULL, FALSE);

  drawable = gimp_image_active_drawable (view->gimage);

  /*  Get the bucket fill context  */
  tool_info = (GimpToolInfo *)
    gimp_container_get_child_by_name (view->gimage->gimp->tool_info_list,
                                      "gimp-bucket-fill-tool");

  if (tool_info && tool_info->tool_options)
    {
      context = GIMP_CONTEXT (tool_info->tool_options);
    }
  else
    {
      context = gimp_get_user_context (view->gimage->gimp);
    }

  gimp_drawable_bucket_fill_full (drawable,
                                  fill_mode,
                                  gimp_context_get_paint_mode (context),
                                  gimp_context_get_opacity (context),
                                  FALSE /* no seed fill */,
                                  FALSE, 0.0, FALSE, 0.0, 0.0 /* fill params */,
                                  color, pattern);

  gimp_image_undo_group_end (view->gimage);

  gimp_image_flush (view->gimage);
}

static void
gimp_drawable_tree_view_new_pattern_dropped (GtkWidget    *widget,
                                             GimpViewable *viewable,
                                             gpointer      data)
{
  gimp_drawable_tree_view_new_dropped (GIMP_ITEM_TREE_VIEW (data),
                                       GIMP_PATTERN_BUCKET_FILL,
                                       NULL,
                                       GIMP_PATTERN (viewable));
}

static void
gimp_drawable_tree_view_new_color_dropped (GtkWidget     *widget,
                                           const GimpRGB *color,
                                           gpointer       data)
{
  gimp_drawable_tree_view_new_dropped (GIMP_ITEM_TREE_VIEW (data),
                                       GIMP_FG_BUCKET_FILL,
                                       color,
                                       NULL);
}
