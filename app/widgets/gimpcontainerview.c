/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpcontainerview.c
 * Copyright (C) 2001 Michael Natterer <mitch@gimp.org>
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

#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimpmarshal.h"
#include "core/gimpviewable.h"

#include "gimpcontainerview.h"
#include "gimpcontainerview-utils.h"
#include "gimpdnd.h"
#include "gimppreviewrenderer.h"


enum
{
  SELECT_ITEM,
  ACTIVATE_ITEM,
  CONTEXT_ITEM,
  LAST_SIGNAL
};


static void   gimp_container_view_class_init  (GimpContainerViewClass *klass);
static void   gimp_container_view_init        (GimpContainerView      *view,
                                               GimpContainerViewClass *klass);

static void   gimp_container_view_destroy     (GtkObject              *object);

static void   gimp_container_view_real_set_container (GimpContainerView *view,
						      GimpContainer     *container);

static void   gimp_container_view_clear_items (GimpContainerView      *view);
static void   gimp_container_view_real_clear_items (GimpContainerView *view);

static void   gimp_container_view_add_foreach (GimpViewable           *viewable,
					       GimpContainerView      *view);
static void   gimp_container_view_add         (GimpContainerView      *view,
					       GimpViewable           *viewable,
					       GimpContainer          *container);
static void   gimp_container_view_remove      (GimpContainerView      *view,
					       GimpViewable           *viewable,
					       GimpContainer          *container);
static void   gimp_container_view_reorder     (GimpContainerView      *view,
					       GimpViewable           *viewable,
					       gint                    new_index,
					       GimpContainer          *container);

static void   gimp_container_view_context_changed  (GimpContext        *context,
						    GimpViewable       *viewable,
						    GimpContainerView  *view);
static void   gimp_container_view_viewable_dropped (GtkWidget          *widget,
						    GimpViewable       *viewable,
						    gpointer            data);
static void  gimp_container_view_button_viewable_dropped (GtkWidget    *widget,
							  GimpViewable *viewable,
							  gpointer      data);


static guint  view_signals[LAST_SIGNAL] = { 0 };

static GimpEditorClass *parent_class = NULL;


GType
gimp_container_view_get_type (void)
{
  static GType view_type = 0;

  if (! view_type)
    {
      static const GTypeInfo view_info =
      {
        sizeof (GimpContainerViewClass),
        NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) gimp_container_view_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GimpContainerView),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gimp_container_view_init,
      };

      view_type = g_type_register_static (GIMP_TYPE_EDITOR,
                                          "GimpContainerView",
                                          &view_info, 0);
    }

  return view_type;
}

static void
gimp_container_view_class_init (GimpContainerViewClass *klass)
{
  GtkObjectClass *object_class;

  object_class = GTK_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  view_signals[SELECT_ITEM] =
    g_signal_new ("select_item",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GimpContainerViewClass, select_item),
		  NULL, NULL,
		  gimp_marshal_VOID__OBJECT_POINTER,
		  G_TYPE_NONE, 2,
		  GIMP_TYPE_OBJECT,
		  G_TYPE_POINTER);

  view_signals[ACTIVATE_ITEM] =
    g_signal_new ("activate_item",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GimpContainerViewClass, activate_item),
		  NULL, NULL,
		  gimp_marshal_VOID__OBJECT_POINTER,
		  G_TYPE_NONE, 2,
		  GIMP_TYPE_OBJECT,
		  G_TYPE_POINTER);

  view_signals[CONTEXT_ITEM] =
    g_signal_new ("context_item",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GimpContainerViewClass, context_item),
		  NULL, NULL,
		  gimp_marshal_VOID__OBJECT_POINTER,
		  G_TYPE_NONE, 2,
		  GIMP_TYPE_OBJECT,
		  G_TYPE_POINTER);

  object_class->destroy   = gimp_container_view_destroy;

  klass->select_item      = NULL;
  klass->activate_item    = NULL;
  klass->context_item     = NULL;

  klass->set_container    = gimp_container_view_real_set_container;
  klass->insert_item      = NULL;
  klass->remove_item      = NULL;
  klass->reorder_item     = NULL;
  klass->clear_items      = gimp_container_view_real_clear_items;
  klass->set_preview_size = NULL;

  klass->insert_data_free = NULL;
}

static void
gimp_container_view_init (GimpContainerView      *view,
                          GimpContainerViewClass *klass)
{
  view->container            = NULL;
  view->context              = NULL;
  view->hash_table           = g_hash_table_new_full (g_direct_hash,
                                                      g_direct_equal,
                                                      NULL,
                                                      klass->insert_data_free);
  view->preview_size         = 0;
  view->preview_border_width = 1;
  view->reorderable          = FALSE;
  view->get_name_func        = NULL;

  view->scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (view), view->scrolled_win, TRUE, TRUE, 0);
  gtk_widget_show (view->scrolled_win);

  GTK_WIDGET_UNSET_FLAGS (GTK_SCROLLED_WINDOW (view->scrolled_win)->vscrollbar,
                          GTK_CAN_FOCUS);

  view->dnd_widget = view->scrolled_win;
}

static void
gimp_container_view_destroy (GtkObject *object)
{
  GimpContainerView *view;

  view = GIMP_CONTAINER_VIEW (object);

  if (view->container)
    gimp_container_view_set_container (view, NULL);

  if (view->context)
    gimp_container_view_set_context (view, NULL);

  if (view->hash_table)
    {
      g_hash_table_destroy (view->hash_table);
      view->hash_table = NULL;
    }

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

void
gimp_container_view_set_container (GimpContainerView *view,
				   GimpContainer     *container)
{
  g_return_if_fail (GIMP_IS_CONTAINER_VIEW (view));
  g_return_if_fail (container == NULL || GIMP_IS_CONTAINER (container));

  if (container != view->container)
    GIMP_CONTAINER_VIEW_GET_CLASS (view)->set_container (view, container);
}

static void
gimp_container_view_real_set_container (GimpContainerView *view,
					GimpContainer     *container)
{
  if (view->container)
    {
      GimpContainerViewClass *view_class;

      gimp_container_view_select_item (view, NULL);
      gimp_container_view_clear_items (view);

      g_signal_handlers_disconnect_by_func (view->container,
					    gimp_container_view_add,
					    view);
      g_signal_handlers_disconnect_by_func (view->container,
					    gimp_container_view_remove,
					    view);
      g_signal_handlers_disconnect_by_func (view->container,
					    gimp_container_view_reorder,
					    view);

      g_hash_table_destroy (view->hash_table);

      view_class = GIMP_CONTAINER_VIEW_GET_CLASS (view);

      view->hash_table = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                NULL,
                                                view_class->insert_data_free);

      if (view->context)
	{
	  g_signal_handlers_disconnect_by_func (view->context,
						gimp_container_view_context_changed,
						view);

	  if (view->dnd_widget)
	    {
	      gtk_drag_dest_unset (GTK_WIDGET (view->dnd_widget));
	      gimp_dnd_viewable_dest_remove (GTK_WIDGET (view->dnd_widget),
                                             view->container->children_type);
	    }
	}

      if (view->get_name_func &&
	  gimp_container_view_is_built_in_name_func (view->get_name_func))
	{
	  gimp_container_view_set_name_func (view, NULL);
	}
    }

  view->container = container;

  if (view->container)
    {
      if (! view->get_name_func)
	{
	  GimpItemGetNameFunc get_name_func;

	  get_name_func =
            gimp_container_view_get_built_in_name_func (view->container->children_type);

	  gimp_container_view_set_name_func (view, get_name_func);
	}

      gimp_container_foreach (view->container,
			      (GFunc) gimp_container_view_add_foreach,
			      view);

      g_signal_connect_object (view->container, "add",
			       G_CALLBACK (gimp_container_view_add),
			       view,
			       G_CONNECT_SWAPPED);
      g_signal_connect_object (view->container, "remove",
			       G_CALLBACK (gimp_container_view_remove),
			       view,
			       G_CONNECT_SWAPPED);
      g_signal_connect_object (view->container, "reorder",
			       G_CALLBACK (gimp_container_view_reorder),
			       view,
			       G_CONNECT_SWAPPED);

      if (view->context)
	{
	  GimpObject  *object;
	  const gchar *signal_name;

	  signal_name =
	    gimp_context_type_to_signal_name (view->container->children_type);

	  g_signal_connect_object (view->context, signal_name,
				   G_CALLBACK (gimp_container_view_context_changed),
				   view,
				   0);

	  object = gimp_context_get_by_type (view->context,
					     view->container->children_type);

	  gimp_container_view_select_item (view, (GimpViewable *) object);

	  if (view->dnd_widget)
            gimp_dnd_viewable_dest_add (GTK_WIDGET (view->dnd_widget),
                                        view->container->children_type,
                                        gimp_container_view_viewable_dropped,
                                        view);
	}
    }
}

void
gimp_container_view_construct (GimpContainerView *view,
                               GimpContainer     *container,
                               GimpContext       *context,
                               gint               preview_size,
                               gint               preview_border_width,
                               gboolean           reorderable)
{
  g_return_if_fail (GIMP_IS_CONTAINER_VIEW (view));
  g_return_if_fail (container == NULL || GIMP_IS_CONTAINER (container));
  g_return_if_fail (context == NULL || GIMP_IS_CONTEXT (context));
  g_return_if_fail (preview_size >  0 &&
                    preview_size <= GIMP_VIEWABLE_MAX_PREVIEW_SIZE);
  g_return_if_fail (preview_border_width >= 0 &&
                    preview_border_width <= GIMP_PREVIEW_MAX_BORDER_WIDTH);

  view->reorderable = reorderable ? TRUE : FALSE;

  gimp_container_view_set_preview_size (view, preview_size,
                                        preview_border_width);

  if (container)
    gimp_container_view_set_container (view, container);

  if (context)
    gimp_container_view_set_context (view, context);
}

void
gimp_container_view_set_context (GimpContainerView *view,
				 GimpContext       *context)
{
  g_return_if_fail (GIMP_IS_CONTAINER_VIEW (view));
  g_return_if_fail (! context || GIMP_IS_CONTEXT (context));

  if (context == view->context)
    return;

  if (view->context)
    {
      if (view->container)
        {
          g_signal_handlers_disconnect_by_func (view->context,
                                                gimp_container_view_context_changed,
                                                view);

          if (view->dnd_widget)
            {
              gtk_drag_dest_unset (GTK_WIDGET (view->dnd_widget));
              gimp_dnd_viewable_dest_remove (GTK_WIDGET (view->dnd_widget),
                                             view->container->children_type);
            }
        }

      g_object_unref (view->context);
    }

  view->context = context;

  if (view->context)
    {
      g_object_ref (view->context);

      if (view->container)
        {
          GimpObject  *object;
          const gchar *signal_name;

          signal_name =
            gimp_context_type_to_signal_name (view->container->children_type);

          g_signal_connect_object (view->context, signal_name,
                                   G_CALLBACK (gimp_container_view_context_changed),
                                   view,
                                   0);

          object = gimp_context_get_by_type (view->context,
                                             view->container->children_type);

          gimp_container_view_select_item (view, (GimpViewable *) object);

          if (view->dnd_widget)
            gimp_dnd_viewable_dest_add (GTK_WIDGET (view->dnd_widget),
                                        view->container->children_type,
                                        gimp_container_view_viewable_dropped,
                                        view);
        }
    }
}

void
gimp_container_view_set_preview_size (GimpContainerView *view,
				      gint               preview_size,
                                      gint               preview_border_width)
{
  g_return_if_fail (GIMP_IS_CONTAINER_VIEW (view));
  g_return_if_fail (preview_size >  0 &&
                    preview_size <= GIMP_VIEWABLE_MAX_PREVIEW_SIZE);
  g_return_if_fail (preview_border_width >= 0 &&
                    preview_border_width <= GIMP_PREVIEW_MAX_BORDER_WIDTH);

  if (view->preview_size         != preview_size ||
      view->preview_border_width != preview_border_width)
    {
      view->preview_size         = preview_size;
      view->preview_border_width = preview_border_width;

      GIMP_CONTAINER_VIEW_GET_CLASS (view)->set_preview_size (view);
    }
}

void
gimp_container_view_set_name_func (GimpContainerView   *view,
				   GimpItemGetNameFunc  get_name_func)
{
  g_return_if_fail (GIMP_IS_CONTAINER_VIEW (view));

  view->get_name_func = get_name_func;
}

void
gimp_container_view_enable_dnd (GimpContainerView *view,
				GtkButton         *button,
				GType              children_type)
{
  g_return_if_fail (GIMP_IS_CONTAINER_VIEW (view));
  g_return_if_fail (GTK_IS_BUTTON (button));

  gimp_dnd_viewable_dest_add (GTK_WIDGET (button),
			      children_type,
			      gimp_container_view_button_viewable_dropped,
			      view);
}

void
gimp_container_view_set_size_request (GimpContainerView *view,
                                      gint               width,
                                      gint               height)
{
  GtkScrolledWindowClass *sw_class;
  GtkRequisition          req;
  gint                    scrollbar_width;
  gint                    border_x;
  gint                    border_y;

  g_return_if_fail (GIMP_IS_CONTAINER_VIEW (view));
  g_return_if_fail (width  <= 0 || width  >= view->preview_size);
  g_return_if_fail (height <= 0 || height >= view->preview_size);

  sw_class = GTK_SCROLLED_WINDOW_GET_CLASS (view->scrolled_win);

  if (sw_class->scrollbar_spacing >= 0)
    width = sw_class->scrollbar_spacing;
  else
    gtk_widget_style_get (GTK_WIDGET (view->scrolled_win),
                          "scrollbar_spacing", &scrollbar_width,
                          NULL);

  gtk_widget_size_request (GTK_SCROLLED_WINDOW (view->scrolled_win)->vscrollbar,
                           &req);
  scrollbar_width += req.width;

  border_x = view->scrolled_win->style->xthickness * 2 + scrollbar_width;
  border_y = view->scrolled_win->style->ythickness * 2;

  gtk_widget_set_size_request (view->scrolled_win,
                               width  > 0 ? width  + border_x : -1,
                               height > 0 ? height + border_y : -1);
}

void
gimp_container_view_select_item (GimpContainerView *view,
				 GimpViewable      *viewable)
{
  g_return_if_fail (GIMP_IS_CONTAINER_VIEW (view));
  g_return_if_fail (! viewable || GIMP_IS_VIEWABLE (viewable));

  if (view->hash_table)
    {
      gpointer insert_data;

      insert_data = g_hash_table_lookup (view->hash_table, viewable);

      g_signal_emit (view, view_signals[SELECT_ITEM], 0,
                     viewable, insert_data);
    }
}

void
gimp_container_view_activate_item (GimpContainerView *view,
				   GimpViewable      *viewable)
{
  g_return_if_fail (GIMP_IS_CONTAINER_VIEW (view));
  g_return_if_fail (GIMP_IS_VIEWABLE (viewable));

  if (view->hash_table)
    {
      gpointer insert_data;

      insert_data = g_hash_table_lookup (view->hash_table, viewable);

      g_signal_emit (view, view_signals[ACTIVATE_ITEM], 0,
                     viewable, insert_data);
    }
}

void
gimp_container_view_context_item (GimpContainerView *view,
				  GimpViewable      *viewable)
{
  g_return_if_fail (GIMP_IS_CONTAINER_VIEW (view));
  g_return_if_fail (GIMP_IS_VIEWABLE (viewable));

  if (view->hash_table)
    {
      gpointer insert_data;

      insert_data = g_hash_table_lookup (view->hash_table, viewable);

      g_signal_emit (view, view_signals[CONTEXT_ITEM], 0,
                     viewable, insert_data);
    }
}

void
gimp_container_view_item_selected (GimpContainerView *view,
				   GimpViewable      *viewable)
{
  g_return_if_fail (GIMP_IS_CONTAINER_VIEW (view));
  g_return_if_fail (GIMP_IS_VIEWABLE (viewable));

  gimp_container_view_select_item (view, viewable);

  if (view->container && view->context)
    {
      GimpContext *context;

      /*  ref and remember the context because view->context may
       *  become NULL by calling gimp_context_set_by_type()
       */
      context = g_object_ref (view->context);

      g_signal_handlers_block_by_func (context,
                                       gimp_container_view_context_changed,
                                       view);

      gimp_context_set_by_type (context,
                                view->container->children_type,
                                GIMP_OBJECT (viewable));

      g_signal_handlers_unblock_by_func (context,
                                         gimp_container_view_context_changed,
                                         view);

      g_object_unref (context);
    }
}

void
gimp_container_view_item_activated (GimpContainerView *view,
				    GimpViewable      *viewable)
{
  g_return_if_fail (GIMP_IS_CONTAINER_VIEW (view));
  g_return_if_fail (GIMP_IS_VIEWABLE (viewable));

  gimp_container_view_activate_item (view, viewable);
}

void
gimp_container_view_item_context (GimpContainerView *view,
				  GimpViewable      *viewable)
{
  g_return_if_fail (GIMP_IS_CONTAINER_VIEW (view));
  g_return_if_fail (GIMP_IS_VIEWABLE (viewable));

  gimp_container_view_context_item (view, viewable);
}

static void
gimp_container_view_clear_items (GimpContainerView *view)
{
  GIMP_CONTAINER_VIEW_GET_CLASS (view)->clear_items (view);
}

static void
gimp_container_view_real_clear_items (GimpContainerView *view)
{
  GimpContainerViewClass *view_class;

  view_class = GIMP_CONTAINER_VIEW_GET_CLASS (view);

  g_hash_table_destroy (view->hash_table);

  view->hash_table = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                            NULL,
                                            view_class->insert_data_free);
}

static void
gimp_container_view_add_foreach (GimpViewable      *viewable,
				 GimpContainerView *view)
{
  gpointer insert_data;

  insert_data = GIMP_CONTAINER_VIEW_GET_CLASS (view)->insert_item (view,
								   viewable,
								   -1);

  g_hash_table_insert (view->hash_table, viewable, insert_data);
}

static void
gimp_container_view_add (GimpContainerView *view,
			 GimpViewable      *viewable,
			 GimpContainer     *container)
{
  gpointer insert_data = NULL;
  gint     index;

  index = gimp_container_get_child_index (container,
                                          GIMP_OBJECT (viewable));

  insert_data = GIMP_CONTAINER_VIEW_GET_CLASS (view)->insert_item (view,
								   viewable,
								   index);

  g_hash_table_insert (view->hash_table, viewable, insert_data);
}

static void
gimp_container_view_remove (GimpContainerView *view,
			    GimpViewable      *viewable,
			    GimpContainer     *container)
{
  gpointer insert_data;

  insert_data = g_hash_table_lookup (view->hash_table, viewable);

  if (insert_data)
    {
      GIMP_CONTAINER_VIEW_GET_CLASS (view)->remove_item (view,
							 viewable,
							 insert_data);

      g_hash_table_remove (view->hash_table, viewable);
    }
}

static void
gimp_container_view_reorder (GimpContainerView *view,
			     GimpViewable      *viewable,
			     gint               new_index,
			     GimpContainer     *container)
{
  gpointer insert_data;

  insert_data = g_hash_table_lookup (view->hash_table, viewable);

  if (insert_data)
    {
      GIMP_CONTAINER_VIEW_GET_CLASS (view)->reorder_item (view,
							  viewable,
							  new_index,
							  insert_data);
    }
}

static void
gimp_container_view_context_changed (GimpContext       *context,
				     GimpViewable      *viewable,
				     GimpContainerView *view)
{
  gpointer insert_data;

  insert_data = g_hash_table_lookup (view->hash_table, viewable);

  g_signal_emit (view, view_signals[SELECT_ITEM], 0, viewable, insert_data);
}

static void
gimp_container_view_viewable_dropped (GtkWidget    *widget,
				      GimpViewable *viewable,
				      gpointer      data)
{
  GimpContainerView *view;

  view = GIMP_CONTAINER_VIEW (data);

  gimp_context_set_by_type (view->context,
                            view->container->children_type,
                            GIMP_OBJECT (viewable));
}

static void
gimp_container_view_button_viewable_dropped (GtkWidget    *widget,
					     GimpViewable *viewable,
					     gpointer      data)
{
  GimpContainerView *view;

  view = (GimpContainerView *) data;

  if (viewable && gimp_container_have (view->container,
				       GIMP_OBJECT (viewable)))
    {
      gimp_container_view_item_selected (view, viewable);

      gtk_button_clicked (GTK_BUTTON (widget));
    }
}
