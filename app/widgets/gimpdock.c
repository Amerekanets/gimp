/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpdock.c
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

#include "core/gimpcontext.h"
#include "core/gimpmarshal.h"

#include "gimpdialogfactory.h"
#include "gimpdnd.h"
#include "gimpdock.h"
#include "gimpdockable.h"
#include "gimpdockbook.h"

#include "libgimp/gimpintl.h"


#define DEFAULT_SEPARATOR_HEIGHT 6


enum
{
  BOOK_ADDED,
  BOOK_REMOVED,
  LAST_SIGNAL
};


static void        gimp_dock_class_init               (GimpDockClass  *klass);
static void        gimp_dock_init                     (GimpDock       *dock);

static GtkWidget * gimp_dock_separator_new            (GimpDock       *dock);

static void        gimp_dock_destroy                  (GtkObject      *object);
static void        gimp_dock_style_set                (GtkWidget      *widget,
                                                       GtkStyle       *prev_style);

static void        gimp_dock_real_book_added          (GimpDock       *dock,
                                                       GimpDockbook   *dockbook);
static void        gimp_dock_real_book_removed        (GimpDock       *dock,
                                                       GimpDockbook   *dockbook);

static void        gimp_dock_separator_realize        (GtkWidget      *widget,
                                                       gpointer        data);
static gboolean    gimp_dock_separator_drag_drop      (GtkWidget      *widget,
						       GdkDragContext *context,
						       gint            x,
						       gint            y,
						       guint           time,
						       gpointer        data);

/*
static gboolean    gimp_dock_separator_button_press   (GtkWidget      *widget,
						       GdkEventButton *bevent,
						       gpointer        data);
static gboolean    gimp_dock_separator_button_release (GtkWidget      *widget,
						       GdkEventButton *bevent,
						       gpointer        data);
static void        gimp_dock_separator_drag_begin     (GtkWidget      *widget,
			  			       GdkDragContext *context,
						       gpointer        data);
static void        gimp_dock_separator_drag_end       (GtkWidget      *widget,
						       GdkDragContext *context,
						       gpointer        data);
*/


static GtkWindowClass *parent_class = NULL;

static guint  dock_signals[LAST_SIGNAL] = { 0 };

static GtkTargetEntry dialog_target_table[] =
{
  GIMP_TARGET_DIALOG
};


GType
gimp_dock_get_type (void)
{
  static GType dock_type = 0;

  if (! dock_type)
    {
      static const GTypeInfo dock_info =
      {
        sizeof (GimpDockClass),
        NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) gimp_dock_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GimpDock),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gimp_dock_init,
      };

      dock_type = g_type_register_static (GTK_TYPE_WINDOW,
                                          "GimpDock",
                                          &dock_info, 0);
    }

  return dock_type;
}

static void
gimp_dock_class_init (GimpDockClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = GTK_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  dock_signals[BOOK_ADDED] =
    g_signal_new ("book_added",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GimpDockClass, book_added),
		  NULL, NULL,
		  gimp_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GIMP_TYPE_DOCKBOOK);  

  dock_signals[BOOK_REMOVED] =
    g_signal_new ("book_removed",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GimpDockClass, book_removed),
		  NULL, NULL,
		  gimp_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GIMP_TYPE_DOCKBOOK);  

  object_class->destroy   = gimp_dock_destroy;

  widget_class->style_set = gimp_dock_style_set;

  klass->book_added       = gimp_dock_real_book_added;
  klass->book_removed     = gimp_dock_real_book_removed;

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("separator_height",
                                                             NULL, NULL,
                                                             0,
                                                             G_MAXINT,
                                                             DEFAULT_SEPARATOR_HEIGHT,
                                                             G_PARAM_READABLE));
}

static void
gimp_dock_init (GimpDock *dock)
{
  GtkWidget *separator;

  dock->context = NULL;

  gtk_window_set_wmclass (GTK_WINDOW (dock), "dock", "Gimp");
  gtk_window_set_type_hint (GTK_WINDOW (dock), GDK_WINDOW_TYPE_HINT_UTILITY);
  gtk_window_set_resizable (GTK_WINDOW (dock), TRUE);

  dock->main_vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (dock), dock->main_vbox);
  gtk_widget_show (dock->main_vbox);

  dock->vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (dock->main_vbox), dock->vbox);
  gtk_widget_show (dock->vbox);

  separator = gimp_dock_separator_new (dock);
  gtk_box_pack_start (GTK_BOX (dock->vbox), separator, FALSE, FALSE, 0);
  gtk_widget_show (separator);
}

static void
gimp_dock_destroy (GtkObject *object)
{
  GimpDock *dock;

  dock = GIMP_DOCK (object);

  while (dock->dockbooks)
    gimp_dock_remove_book (dock, GIMP_DOCKBOOK (dock->dockbooks->data));

  if (dock->context)
    {
      g_object_unref (dock->context);
      dock->context = NULL;
    }

  if (GTK_OBJECT_CLASS (parent_class))
    GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gimp_dock_style_set (GtkWidget *widget,
                     GtkStyle  *prev_style)
{
  GimpDock *dock;
  GList    *children;
  GList    *list;
  gint      separator_height;

  dock = GIMP_DOCK (widget);

  if (GTK_WIDGET_CLASS (parent_class)->style_set)
    GTK_WIDGET_CLASS (parent_class)->style_set (widget, prev_style);

  gtk_widget_style_get (widget,
                        "separator_height", &separator_height,
                        NULL);

  children = gtk_container_get_children (GTK_CONTAINER (dock->vbox));

  for (list = children; list; list = g_list_next (list))
    {
      if (GTK_IS_EVENT_BOX (list->data))
        {
          gtk_widget_set_size_request (GTK_WIDGET (list->data),
                                       -1, separator_height);
       }
    }

  g_list_free (children);
}

static void
gimp_dock_real_book_added (GimpDock     *dock,
                           GimpDockbook *dockbook)
{
}

static void
gimp_dock_real_book_removed (GimpDock     *dock,
                             GimpDockbook *dockbook)
{
  if (dock->dockbooks == NULL)
    gtk_widget_destroy (GTK_WIDGET (dock));
}

static GtkWidget *
gimp_dock_separator_new (GimpDock *dock)
{
  GtkWidget  *event_box;
  GtkWidget  *frame;
  gint        separator_height;

  event_box = gtk_event_box_new ();

  gtk_widget_set_name (event_box, "dock-separator");

  gtk_widget_style_get (GTK_WIDGET (dock),
                        "separator_height", &separator_height,
                        NULL);

  gtk_widget_set_size_request (event_box, -1, separator_height);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER (event_box), frame);
  gtk_widget_show (frame);

  gimp_help_set_help_data (event_box,
                           _("You can drop dockable dialogs here."), NULL);

  g_signal_connect (event_box, "realize",
		    G_CALLBACK (gimp_dock_separator_realize),
		    frame);

  gtk_drag_dest_set (GTK_WIDGET (event_box),
                     GTK_DEST_DEFAULT_ALL,
                     dialog_target_table, G_N_ELEMENTS (dialog_target_table),
                     GDK_ACTION_MOVE);
  g_signal_connect (event_box, "drag_drop",
		    G_CALLBACK (gimp_dock_separator_drag_drop),
		    dock);

  return event_box;
}

gboolean
gimp_dock_construct (GimpDock          *dock,
                     GimpDialogFactory *dialog_factory,
                     GimpContext       *context)
{
  g_return_val_if_fail (GIMP_IS_DOCK (dock), FALSE);
  g_return_val_if_fail (GIMP_IS_DIALOG_FACTORY (dialog_factory), FALSE);
  g_return_val_if_fail (GIMP_IS_CONTEXT (context), FALSE);

  dock->dialog_factory = dialog_factory;
  dock->context        = context;

  g_object_ref (dock->context);

  return TRUE;
}

void
gimp_dock_add (GimpDock     *dock,
	       GimpDockable *dockable,
	       gint          section,
	       gint          position)
{
  GimpDockbook *dockbook;

  g_return_if_fail (GIMP_IS_DOCK (dock));
  g_return_if_fail (GIMP_IS_DOCKABLE (dockable));

  g_return_if_fail (dockable->dockbook == NULL);

  dockbook = GIMP_DOCKBOOK (dock->dockbooks->data);

  gimp_dockbook_add (dockbook, dockable, position);
}

void
gimp_dock_remove (GimpDock     *dock,
		  GimpDockable *dockable)
{
  g_return_if_fail (GIMP_IS_DOCK (dock));
  g_return_if_fail (GIMP_IS_DOCKABLE (dockable));

  g_return_if_fail (dockable->dockbook != NULL);
  g_return_if_fail (dockable->dockbook->dock != NULL);
  g_return_if_fail (dockable->dockbook->dock == dock);

  gimp_dockbook_remove (dockable->dockbook, dockable);
}

void
gimp_dock_add_book (GimpDock     *dock,
		    GimpDockbook *dockbook,
		    gint          index)
{
  gint old_length;

  g_return_if_fail (GIMP_IS_DOCK (dock));
  g_return_if_fail (GIMP_IS_DOCKBOOK (dockbook));

  g_return_if_fail (dockbook->dock == NULL);

  old_length = g_list_length (dock->dockbooks);

  if (index >= old_length || index < 0)
    index = old_length;

  dockbook->dock  = dock;
  dock->dockbooks = g_list_insert (dock->dockbooks, dockbook, index);

  if (old_length == 0)
    {
      GtkWidget *separator;

      gtk_box_pack_start (GTK_BOX (dock->vbox), GTK_WIDGET (dockbook),
                          TRUE, TRUE, 0);

      separator = gimp_dock_separator_new (dock);
      gtk_box_pack_end (GTK_BOX (dock->vbox), separator, FALSE, FALSE, 0);
      gtk_widget_show (separator);
    }
  else
    {
      GtkWidget *old_book;
      GtkWidget *parent;
      GtkWidget *paned;

      if (index == 0)
        old_book = g_list_nth_data (dock->dockbooks, index + 1);
      else
        old_book = g_list_nth_data (dock->dockbooks, index - 1);

      parent = old_book->parent;

      if ((old_length > 1) && (index > 0))
        {
          GtkWidget *grandparent;

          grandparent = parent->parent;

          old_book = parent;
          parent   = grandparent;
        }

      g_object_ref (old_book);

      gtk_container_remove (GTK_CONTAINER (parent), old_book);

      paned = gtk_vpaned_new ();

      if (GTK_IS_VPANED (parent))
        gtk_paned_pack1 (GTK_PANED (parent), paned, TRUE, FALSE);
      else
        gtk_box_pack_start (GTK_BOX (parent), paned, TRUE, TRUE, 0);

      gtk_widget_show (paned);

      if (index == 0)
        {
          gtk_paned_pack1 (GTK_PANED (paned), GTK_WIDGET (dockbook),
                           TRUE, FALSE);
          gtk_paned_pack2 (GTK_PANED (paned), old_book,
                           TRUE, FALSE);
        }
      else
        {
          gtk_paned_pack1 (GTK_PANED (paned), old_book,
                           TRUE, FALSE);
          gtk_paned_pack2 (GTK_PANED (paned), GTK_WIDGET (dockbook),
                           TRUE, FALSE);
        }

      g_object_unref (old_book);
    }

  gtk_widget_show (GTK_WIDGET (dockbook));

  g_signal_emit (dock, dock_signals[BOOK_ADDED], 0, dockbook);
}

void
gimp_dock_remove_book (GimpDock     *dock,
		       GimpDockbook *dockbook)
{
  gint old_length;
  gint index;

  g_return_if_fail (GIMP_IS_DOCK (dock));
  g_return_if_fail (GIMP_IS_DOCKBOOK (dockbook));

  g_return_if_fail (dockbook->dock == dock);

  old_length = g_list_length (dock->dockbooks);
  index      = g_list_index (dock->dockbooks, dockbook);

  dockbook->dock  = NULL;
  dock->dockbooks = g_list_remove (dock->dockbooks, dockbook);

  g_object_ref (dockbook);

  if (old_length == 1)
    {
      GtkWidget *separator;
      GList     *children;

      children = gtk_container_get_children (GTK_CONTAINER (dock->vbox));

      separator = g_list_nth_data (children, 2);

      gtk_container_remove (GTK_CONTAINER (dock->vbox), separator);
      gtk_container_remove (GTK_CONTAINER (dock->vbox), GTK_WIDGET (dockbook));

      g_list_free (children);
    }
  else
    {
      GtkWidget *other_book;
      GtkWidget *parent;
      GtkWidget *grandparent;

      parent      = GTK_WIDGET (dockbook)->parent;
      grandparent = parent->parent;

      if (index == 0)
        other_book = GTK_PANED (parent)->child2;
      else
        other_book = GTK_PANED (parent)->child1;

      g_object_ref (other_book);

      gtk_container_remove (GTK_CONTAINER (parent), other_book);
      gtk_container_remove (GTK_CONTAINER (parent), GTK_WIDGET (dockbook));

      gtk_container_remove (GTK_CONTAINER (grandparent), parent);

      if (GTK_IS_VPANED (grandparent))
        gtk_paned_pack1 (GTK_PANED (grandparent), other_book, TRUE, FALSE);
      else
        gtk_box_pack_start (GTK_BOX (dock->vbox), other_book, TRUE, TRUE, 0);

      g_object_unref (other_book);
    }

  g_signal_emit (dock, dock_signals[BOOK_REMOVED], 0, dockbook);

  g_object_unref (dockbook);
}


/*  fiddle with the color to make the drop area stand out  */
static void
gimp_dock_separator_realize (GtkWidget *widget,
                             gpointer   data)
{
  GdkColor *color;

  color = gtk_widget_get_style (widget)->bg + GTK_STATE_SELECTED;

  gtk_widget_modify_bg (widget, GTK_STATE_NORMAL, color);
  gtk_widget_modify_bg (GTK_WIDGET (data), GTK_STATE_NORMAL, color);
}

static gboolean
gimp_dock_separator_drag_drop (GtkWidget      *widget,
			       GdkDragContext *context,
			       gint            x,
			       gint            y,
			       guint           time,
			       gpointer        data)
{
  GimpDock  *dock;
  GtkWidget *source;

  dock = GIMP_DOCK (data);

  source = gtk_drag_get_source_widget (context);

  if (source)
    {
      GimpDockable *src_dockable;

      src_dockable = (GimpDockable *) g_object_get_data (G_OBJECT (source),
                                                         "gimp-dockable");

      if (src_dockable)
	{
	  GtkWidget *dockbook;
	  GList     *children;
	  gint       index;

	  g_object_set_data (G_OBJECT (src_dockable),
                             "gimp-dock-drag-widget", NULL);

	  children = gtk_container_get_children (GTK_CONTAINER (widget->parent));
	  index = g_list_index (children, widget);
          g_list_free (children);

          if (index == 0)
            index = 0;
          else if (index == 2)
            index = -1;

          /*  if dropping to the same dock, take care that we don't try
           *  to reorder the *only* dockable in the dock
           */
          if (src_dockable->dockbook->dock == dock)
            {
              gint n_books;
              gint n_dockables;

              n_books = g_list_length (dock->dockbooks);

              children =
                gtk_container_get_children (GTK_CONTAINER (src_dockable->dockbook));
              n_dockables = g_list_length (children);
              g_list_free (children);

              if (n_books == 1 && n_dockables == 1)
                return TRUE; /* successfully do nothing */
            }

	  g_object_ref (src_dockable);

	  gimp_dockbook_remove (src_dockable->dockbook, src_dockable);

	  dockbook = gimp_dockbook_new (dock->dialog_factory->menu_factory);
	  gimp_dock_add_book (dock, GIMP_DOCKBOOK (dockbook), index);

	  gimp_dockbook_add (GIMP_DOCKBOOK (dockbook), src_dockable, -1);

	  g_object_unref (src_dockable);

	  return TRUE;
	}
    }

  return FALSE;
}


/*

static gboolean
gimp_dock_separator_button_press (GtkWidget      *widget,
				  GdkEventButton *bevent,
				  gpointer        data)
{
  if (bevent->type == GDK_BUTTON_PRESS)
    {
      if (bevent->button == 1)
        {
          gtk_grab_add (widget);
        }
    }

  return TRUE;
}

static gboolean
gimp_dock_separator_button_release (GtkWidget      *widget,
				    GdkEventButton *bevent,
				    gpointer        data)
{
  if (bevent->button == 1)
    {
      gtk_grab_remove (widget);
    }

  return TRUE;
}

static void
gimp_dock_tab_drag_begin (GtkWidget      *widget,
			  GdkDragContext *context,
			  gpointer        data)
{
  GimpDockable *dockable;
  GtkWidget    *window;
  GtkWidget    *frame;
  GtkWidget    *label;

  dockable = GIMP_DOCKABLE (data);

  window = gtk_window_new (GTK_WINDOW_POPUP);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER (window), frame);
  gtk_widget_show (frame);

  label = gtk_label_new (dockable->name);
  gtk_misc_set_padding (GTK_MISC (label), 10, 5);
  gtk_container_add (GTK_CONTAINER (frame), label);
  gtk_widget_show (label);

  gtk_widget_show (window);

  g_object_set_data_full (G_OBJECT (dockable), "gimp-dock-drag-widget",
			  window,
			  (GDestroyNotify) gtk_widget_destroy);

  gtk_drag_set_icon_widget (context, window,
			    -8, -8);
}

static void
gimp_dock_tab_drag_end (GtkWidget      *widget,
			GdkDragContext *context,
			gpointer        data)
{
  GimpDockable *dockable;
  GtkWidget    *drag_widget;

  dockable = GIMP_DOCKABLE (data);

  drag_widget = g_object_get_data (G_OBJECT (dockable),
				     "gimp-dock-drag-widget");

  if (drag_widget)
    {
      GtkWidget *dock;

      g_object_set_data (G_OBJECT (dockable), "gimp-dock-drag-widget", NULL);

      dock = gimp_dock_new ();

      gtk_window_set_position (GTK_WINDOW (dock), GTK_WIN_POS_MOUSE);

      g_object_ref (dockable);

      gimp_dock_remove (dockable->dock, dockable);
      gimp_dock_add (GIMP_DOCK (dock), dockable, -1, -1);

      g_object_unref (dockable);

      gtk_widget_show (dock);
    }
}
*/
