/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpcontainertreeview.h
 * Copyright (C) 2003 Michael Natterer <mitch@gimp.org>
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

#ifndef __GIMP_CONTAINER_TREE_VIEW_H__
#define __GIMP_CONTAINER_TREE_VIEW_H__


#include "gimpcontainerview.h"


#define GIMP_TYPE_CONTAINER_TREE_VIEW            (gimp_container_tree_view_get_type ())
#define GIMP_CONTAINER_TREE_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_CONTAINER_TREE_VIEW, GimpContainerTreeView))
#define GIMP_CONTAINER_TREE_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_CONTAINER_TREE_VIEW, GimpContainerTreeViewClass))
#define GIMP_IS_CONTAINER_TREE_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_CONTAINER_TREE_VIEW))
#define GIMP_IS_CONTAINER_TREE_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_CONTAINER_TREE_VIEW))
#define GIMP_CONTAINER_TREE_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_CONTAINER_TREE_VIEW, GimpContainerTreeViewClass))


typedef struct _GimpContainerTreeViewClass  GimpContainerTreeViewClass;

struct _GimpContainerTreeView
{
  GimpContainerView  parent_instance;

  GtkWidget         *scrolled_win;

  GtkTreeModel      *model;
  gint               n_model_columns;
  GType              model_columns[16];

  gint               model_column_renderer;
  gint               model_column_name;

  GtkTreeView       *view;
  GtkTreeSelection  *selection;

  GtkTreeViewColumn *main_column;
  GtkCellRenderer   *renderer_cell;
  GtkCellRenderer   *name_cell;

  GList             *toggle_columns;
  GList             *toggle_cells;
  GList             *renderer_cells;

  gint               preview_border_width;

  GQuark             invalidate_preview_handler_id;
  GQuark             name_changed_handler_id;

  GimpViewable      *dnd_viewable;
};

struct _GimpContainerTreeViewClass
{
  GimpContainerViewClass  parent_class;
};


GType       gimp_container_tree_view_get_type (void) G_GNUC_CONST;

GtkWidget * gimp_container_tree_view_new      (GimpContainer *container,
					       GimpContext   *context,
					       gint           preview_size,
                                               gboolean       reorderable,
					       gint           min_items_x,
					       gint           min_items_y);


#endif  /*  __GIMP_CONTAINER_TREE_VIEW_H__  */
