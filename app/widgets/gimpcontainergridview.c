/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpcontainergridview.c
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

#include <stdio.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "libgimpcolor/gimpcolor.h"

#include "widgets-types.h"

#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimpmarshal.h"
#include "core/gimpviewable.h"

#include "gimpcontainergridview.h"
#include "gimppreview.h"
#include "gimppreviewrenderer.h"
#include "gtkhwrapbox.h"

#include "gimp-intl.h"


enum
{
  MOVE_CURSOR,
  LAST_SIGNAL
};


static void     gimp_container_grid_view_class_init   (GimpContainerGridViewClass *klass);
static void     gimp_container_grid_view_init         (GimpContainerGridView      *view);

static gboolean gimp_container_grid_view_move_cursor  (GimpContainerGridView  *view,
                                                       GtkMovementStep         step,
                                                       gint                    count);
static gboolean gimp_container_grid_view_focus        (GtkWidget              *widget,
                                                       GtkDirectionType        direction);

static gpointer gimp_container_grid_view_insert_item  (GimpContainerView      *view,
						       GimpViewable           *viewable,
						       gint                    index);
static void     gimp_container_grid_view_remove_item  (GimpContainerView      *view,
						       GimpViewable           *viewable,
						       gpointer                insert_data);
static void     gimp_container_grid_view_reorder_item (GimpContainerView      *view,
						       GimpViewable           *viewable,
						       gint                    new_index,
						       gpointer                insert_data);
static void     gimp_container_grid_view_select_item  (GimpContainerView      *view,
                                                       GimpViewable           *viewable,
                                                       gpointer                insert_data);
static void     gimp_container_grid_view_clear_items  (GimpContainerView      *view);
static void gimp_container_grid_view_set_preview_size (GimpContainerView      *view);
static void    gimp_container_grid_view_item_selected (GtkWidget              *widget,
                                                       GdkModifierType         state,
						       gpointer                data);
static void   gimp_container_grid_view_item_activated (GtkWidget              *widget,
						       gpointer                data);
static void   gimp_container_grid_view_item_context   (GtkWidget              *widget,
						       gpointer                data);
static void   gimp_container_grid_view_highlight_item (GimpContainerView      *view,
						       GimpViewable           *viewable,
						       gpointer                insert_data);

static void  gimp_container_grid_view_vieport_resized (GtkWidget              *widget,
                                                       GtkAllocation          *allocation,
                                                       GimpContainerGridView  *view);


static GimpContainerViewClass *parent_class = NULL;
static guint grid_view_signals[LAST_SIGNAL] = { 0 };

static GimpRGB  white_color;
static GimpRGB  black_color;


GType
gimp_container_grid_view_get_type (void)
{
  static GType view_type = 0;

  if (! view_type)
    {
      static const GTypeInfo view_info =
      {
        sizeof (GimpContainerGridViewClass),
        NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) gimp_container_grid_view_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GimpContainerGridView),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gimp_container_grid_view_init,
      };

      view_type = g_type_register_static (GIMP_TYPE_CONTAINER_VIEW,
                                          "GimpContainerGridView",
                                          &view_info, 0);
    }

  return view_type;
}

static void
gimp_container_grid_view_class_init (GimpContainerGridViewClass *klass)
{
  GimpContainerViewClass *container_view_class;
  GtkWidgetClass         *widget_class;
  GtkBindingSet          *binding_set;

  container_view_class = GIMP_CONTAINER_VIEW_CLASS (klass);
  widget_class         = GTK_WIDGET_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);
  binding_set  = gtk_binding_set_by_class (klass);

  widget_class->focus                    = gimp_container_grid_view_focus;

  container_view_class->insert_item      = gimp_container_grid_view_insert_item;
  container_view_class->remove_item      = gimp_container_grid_view_remove_item;
  container_view_class->reorder_item     = gimp_container_grid_view_reorder_item;
  container_view_class->select_item      = gimp_container_grid_view_select_item;
  container_view_class->clear_items      = gimp_container_grid_view_clear_items;
  container_view_class->set_preview_size = gimp_container_grid_view_set_preview_size;

  klass->move_cursor                     = gimp_container_grid_view_move_cursor;

  grid_view_signals[MOVE_CURSOR] =
    g_signal_new ("move_cursor",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GimpContainerGridViewClass, move_cursor),
		  NULL, NULL,
		  gimp_marshal_BOOLEAN__ENUM_INT,
		  G_TYPE_BOOLEAN, 2,
		  GTK_TYPE_MOVEMENT_STEP,
		  G_TYPE_INT);

  gtk_binding_entry_add_signal (binding_set, GDK_Home, 0,
                                "move_cursor", 2,
                                G_TYPE_ENUM, GTK_MOVEMENT_BUFFER_ENDS,
                                G_TYPE_INT, -1);
  gtk_binding_entry_add_signal (binding_set, GDK_End, 0,
                                "move_cursor", 2,
                                G_TYPE_ENUM, GTK_MOVEMENT_BUFFER_ENDS,
                                G_TYPE_INT, 1);
  gtk_binding_entry_add_signal (binding_set, GDK_Page_Up, 0,
                                "move_cursor", 2,
                                G_TYPE_ENUM, GTK_MOVEMENT_PAGES,
                                G_TYPE_INT, -1);
  gtk_binding_entry_add_signal (binding_set, GDK_Page_Down, 0,
                                "move_cursor", 2,
                                G_TYPE_ENUM, GTK_MOVEMENT_PAGES,
                                G_TYPE_INT, 1);
 
  gimp_rgba_set (&white_color, 1.0, 1.0, 1.0, 1.0);
  gimp_rgba_set (&black_color, 0.0, 0.0, 0.0, 1.0);
}

static void
gimp_container_grid_view_init (GimpContainerGridView *grid_view)
{
  GimpContainerView *view;

  view = GIMP_CONTAINER_VIEW (grid_view);

  grid_view->rows         = 1;
  grid_view->columns      = 1;
  grid_view->visible_rows = 0;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view->scrolled_win),
                                  GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

  grid_view->name_label = gtk_label_new (_("(None)"));
  gtk_misc_set_alignment (GTK_MISC (grid_view->name_label), 0.0, 0.5);
  gtk_misc_set_padding (GTK_MISC (grid_view->name_label),
			grid_view->name_label->style->xthickness, 0);
  gtk_box_pack_start (GTK_BOX (grid_view), grid_view->name_label,
		      FALSE, FALSE, 0);
  gtk_box_reorder_child (GTK_BOX (grid_view), grid_view->name_label, 0);
  gtk_widget_show (grid_view->name_label);

  grid_view->wrap_box = gtk_hwrap_box_new (FALSE);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (view->scrolled_win),
                                         grid_view->wrap_box);
  gtk_widget_show (grid_view->wrap_box);

  g_signal_connect (grid_view->wrap_box->parent, "size_allocate",
                    G_CALLBACK (gimp_container_grid_view_vieport_resized),
                    grid_view);

  gtk_container_set_focus_vadjustment
    (GTK_CONTAINER (grid_view->wrap_box->parent),
     gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW
                                          (view->scrolled_win)));

  GTK_WIDGET_SET_FLAGS (grid_view, GTK_CAN_FOCUS);
}

GtkWidget *
gimp_container_grid_view_new (GimpContainer *container,
			      GimpContext   *context,
			      gint           preview_size,
                              gint           preview_border_width,
                              gboolean       reorderable)
{
  GimpContainerGridView *grid_view;

  g_return_val_if_fail (container == NULL || GIMP_IS_CONTAINER (container),
                        NULL);
  g_return_val_if_fail (context == NULL || GIMP_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (preview_size  > 0 &&
			preview_size <= GIMP_VIEWABLE_MAX_PREVIEW_SIZE, NULL);
  g_return_val_if_fail (preview_border_width >= 0 &&
                        preview_border_width <= GIMP_PREVIEW_MAX_BORDER_WIDTH,
                        NULL);

  grid_view = g_object_new (GIMP_TYPE_CONTAINER_GRID_VIEW, NULL);

  gimp_container_view_construct (GIMP_CONTAINER_VIEW (grid_view),
                                 container, context,
                                 preview_size, preview_border_width,
                                 reorderable);

  return GTK_WIDGET (grid_view);
}

static gboolean
gimp_container_grid_view_move_by (GimpContainerGridView *view,
                                  gint                   x,
                                  gint                   y)
{
  GimpContainer *container;
  GimpViewable  *item;
  GimpPreview   *preview;
  gint           index;

  preview = g_object_get_data (G_OBJECT (view), "last_selected_item");
  if (!preview)
    return FALSE;

  container = GIMP_CONTAINER_VIEW (view)->container;

  index = gimp_container_get_child_index (container,
                                          GIMP_OBJECT (preview->viewable));

  index += x;
  index = CLAMP (index, 0, container->num_children - 1);

  index += y * view->columns;
  while (index < 0)
    index += view->columns;
  while (index >= container->num_children)
    index -= view->columns;

  item = (GimpViewable *) gimp_container_get_child_by_index (container, index);
  if (item)
    gimp_container_view_item_selected (GIMP_CONTAINER_VIEW (view), item);

  return TRUE;
}

static gboolean
gimp_container_grid_view_move_cursor (GimpContainerGridView *view,
                                      GtkMovementStep        step,
                                      gint                   count)
{
  GimpContainer *container;
  GimpViewable  *item;

  if (!GTK_WIDGET_HAS_FOCUS (GTK_WIDGET (view)) || count == 0)
    return FALSE;

  switch (step)
    {
    case GTK_MOVEMENT_PAGES:
      return gimp_container_grid_view_move_by (view,
                                               0, count * view->visible_rows);

    case GTK_MOVEMENT_BUFFER_ENDS:
      container = GIMP_CONTAINER_VIEW (view)->container;
      count = count < 0 ? 0 : container->num_children - 1;

      item = (GimpViewable *) gimp_container_get_child_by_index (container,
                                                                 count);
      if (item)
        gimp_container_view_item_selected (GIMP_CONTAINER_VIEW (view), item);

      return TRUE;

    default:
      break;
    }

  return FALSE;
}

static gboolean
gimp_container_grid_view_focus (GtkWidget        *widget,
                                GtkDirectionType  direction)
{
  GimpContainerGridView *view;

  if (GTK_WIDGET_CAN_FOCUS (widget) && !GTK_WIDGET_HAS_FOCUS (widget))
    {
      gtk_widget_grab_focus (GTK_WIDGET (widget));
      return TRUE;
    }

  view = GIMP_CONTAINER_GRID_VIEW (widget);

  switch (direction)
    {
    case GTK_DIR_UP:
      return gimp_container_grid_view_move_by (view,  0, -1);
    case GTK_DIR_DOWN:
      return gimp_container_grid_view_move_by (view,  0,  1);
    case GTK_DIR_LEFT:
      return gimp_container_grid_view_move_by (view, -1,  0);
    case GTK_DIR_RIGHT:
      return gimp_container_grid_view_move_by (view,  1,  0);

    case GTK_DIR_TAB_FORWARD:
    case GTK_DIR_TAB_BACKWARD:
      break;
    }

  return FALSE;
}

static gpointer
gimp_container_grid_view_insert_item (GimpContainerView *view,
				      GimpViewable      *viewable,
				      gint               index)
{
  GimpContainerGridView *grid_view;
  GtkWidget             *preview;

  grid_view = GIMP_CONTAINER_GRID_VIEW (view);

  preview = gimp_preview_new_full (viewable,
				   view->preview_size,
				   view->preview_size,
				   1,
				   FALSE, TRUE, TRUE);
  gimp_preview_set_border_color (GIMP_PREVIEW (preview), &white_color);

  gtk_wrap_box_pack (GTK_WRAP_BOX (grid_view->wrap_box), preview,
		     FALSE, FALSE, FALSE, FALSE);

  if (index != -1)
    gtk_wrap_box_reorder_child (GTK_WRAP_BOX (grid_view->wrap_box),
				preview, index);

  gtk_widget_show (preview);

  g_signal_connect (preview, "clicked",
                    G_CALLBACK (gimp_container_grid_view_item_selected),
                    view);
  g_signal_connect (preview, "double_clicked",
                    G_CALLBACK (gimp_container_grid_view_item_activated),
                    view);
  g_signal_connect (preview, "context",
                    G_CALLBACK (gimp_container_grid_view_item_context),
                    view);

  return (gpointer) preview;
}

static void
gimp_container_grid_view_remove_item (GimpContainerView *view,
				      GimpViewable      *viewable,
				      gpointer           insert_data)
{
  GimpContainerGridView *grid_view;
  GtkWidget             *preview;

  grid_view = GIMP_CONTAINER_GRID_VIEW (view);
  preview   = GTK_WIDGET (insert_data);

  if (preview)
    {
      if (g_object_get_data (G_OBJECT (view), "last_selected_item") == preview)
        g_object_set_data (G_OBJECT (view), "last_selected_item", NULL);

      gtk_container_remove (GTK_CONTAINER (grid_view->wrap_box), preview);
    }
}

static void
gimp_container_grid_view_reorder_item (GimpContainerView *view,
				       GimpViewable      *viewable,
				       gint               new_index,
				       gpointer           insert_data)
{
  GimpContainerGridView *grid_view;
  GtkWidget             *preview;

  grid_view = GIMP_CONTAINER_GRID_VIEW (view);
  preview   = GTK_WIDGET (insert_data);

  gtk_wrap_box_reorder_child (GTK_WRAP_BOX (grid_view->wrap_box),
			      preview, new_index);
}

static void
gimp_container_grid_view_select_item (GimpContainerView *view,
                                      GimpViewable      *viewable,
                                      gpointer           insert_data)
{
  gimp_container_grid_view_highlight_item (view, viewable, insert_data);
}

static void
gimp_container_grid_view_clear_items (GimpContainerView *view)
{
  GimpContainerGridView *grid_view;

  grid_view = GIMP_CONTAINER_GRID_VIEW (view);

  g_object_set_data (G_OBJECT (view), "last_selected_item", NULL);

  while (GTK_WRAP_BOX (grid_view->wrap_box)->children)
    gtk_container_remove (GTK_CONTAINER (grid_view->wrap_box),
			  GTK_WRAP_BOX (grid_view->wrap_box)->children->widget);

  GIMP_CONTAINER_VIEW_CLASS (parent_class)->clear_items (view);
}

static void
gimp_container_grid_view_set_preview_size (GimpContainerView *view)
{
  GimpContainerGridView *grid_view;
  GtkWrapBoxChild       *child;

  grid_view = GIMP_CONTAINER_GRID_VIEW (view);

  for (child = GTK_WRAP_BOX (grid_view->wrap_box)->children;
       child;
       child = child->next)
    {
      GimpPreview *preview;

      preview = GIMP_PREVIEW (child->widget);

      gimp_preview_set_size (preview, view->preview_size,
                             preview->renderer->border_width);
    }

  gtk_widget_queue_resize (grid_view->wrap_box);
}

static void
gimp_container_grid_view_item_selected (GtkWidget       *widget,
                                        GdkModifierType  state,
					gpointer         data)
{
  if (GTK_WIDGET_CAN_FOCUS (data) && !GTK_WIDGET_HAS_FOCUS (data))
    gtk_widget_grab_focus (GTK_WIDGET (data));
  
  gimp_container_view_item_selected (GIMP_CONTAINER_VIEW (data),
				     GIMP_PREVIEW (widget)->viewable);
}

static void
gimp_container_grid_view_item_activated (GtkWidget *widget,
					 gpointer   data)
{
  gimp_container_view_item_activated (GIMP_CONTAINER_VIEW (data),
				      GIMP_PREVIEW (widget)->viewable);
}

static void
gimp_container_grid_view_item_context (GtkWidget *widget,
				       gpointer   data)
{
  gimp_container_view_item_selected (GIMP_CONTAINER_VIEW (data),
				     GIMP_PREVIEW (widget)->viewable);
  gimp_container_view_item_context (GIMP_CONTAINER_VIEW (data),
				    GIMP_PREVIEW (widget)->viewable);
}

static void
gimp_container_grid_view_highlight_item (GimpContainerView *view,
					 GimpViewable      *viewable,
					 gpointer           insert_data)
{
  GimpContainerGridView *grid_view;
  GimpPreview           *preview;

  grid_view = GIMP_CONTAINER_GRID_VIEW (view);

  preview = g_object_get_data (G_OBJECT (view), "last_selected_item");

  if (preview)
    {
      gimp_preview_set_border_color (preview, &white_color);
      gimp_preview_renderer_update (preview->renderer);
    }

  if (insert_data)
    preview = GIMP_PREVIEW (insert_data);
  else
    preview = NULL;

  if (preview)
    {
      GtkAdjustment *adj;
      gint           item_height;
      gint           index;
      gint           row;

      adj = gtk_scrolled_window_get_vadjustment
	(GTK_SCROLLED_WINDOW (view->scrolled_win));

      item_height = GTK_WIDGET (preview)->allocation.height;

      index = gimp_container_get_child_index (view->container,
                                              GIMP_OBJECT (viewable));

      row = index / grid_view->columns;

      if (row * item_height < adj->value)
        {
          gtk_adjustment_set_value (adj, row * item_height);
        }
      else if ((row + 1) * item_height > adj->value + adj->page_size)
        {
          gtk_adjustment_set_value (adj,
                                    (row + 1) * item_height - adj->page_size);
        }

      gimp_preview_set_border_color (preview, &black_color);
      gimp_preview_renderer_update (preview->renderer);

      if (view->get_name_func)
	{
	  gchar *name;

	  name = view->get_name_func (G_OBJECT (preview), NULL);

	  gtk_label_set_text (GTK_LABEL (grid_view->name_label), name);

	  g_free (name);
	}
      else
	{
	  gtk_label_set_text (GTK_LABEL (grid_view->name_label),
			      GIMP_OBJECT (viewable)->name);
	}
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (grid_view->name_label), _("(None)"));
    }

  g_object_set_data (G_OBJECT (view), "last_selected_item", preview);
}

static void
gimp_container_grid_view_vieport_resized (GtkWidget             *widget,
                                          GtkAllocation         *allocation,
                                          GimpContainerGridView *grid_view)
{
  GimpContainerView *view;

  view = GIMP_CONTAINER_VIEW (grid_view);

  if (view->container)
    {
      GList *children;
      gint   n_children;

      children = gtk_container_get_children (GTK_CONTAINER (grid_view->wrap_box));
      n_children = g_list_length (children);

      if (children)
        {
          GtkRequisition preview_requisition;
          gint           columns;
          gint           rows;

          gtk_widget_size_request (GTK_WIDGET (children->data),
                                   &preview_requisition);

          g_list_free (children);

          columns = MAX (1, allocation->width / preview_requisition.width);

          rows = n_children / columns;

          if (n_children % columns)
            rows++;

          if ((rows != grid_view->rows) || (columns != grid_view->columns))
            {
              grid_view->rows    = rows;
              grid_view->columns = columns;

              gtk_widget_set_size_request (grid_view->wrap_box,
                                           columns * preview_requisition.width,
                                           rows    * preview_requisition.height);

            }
          
          grid_view->visible_rows = (allocation->height /
                                     preview_requisition.height);
        }
    }
}
