/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpbrushfactoryview.c
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

#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "widgets-types.h"

#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimpbrush.h"
#include "core/gimpbrushgenerated.h"
#include "core/gimpdatafactory.h"

#include "gimpcontainerview.h"
#include "gimpbrushfactoryview.h"
#include "gimppreview.h"

#include "libgimp/gimpintl.h"


static void   gimp_brush_factory_view_class_init (GimpBrushFactoryViewClass *klass);
static void   gimp_brush_factory_view_init       (GimpBrushFactoryView      *view);
static void   gimp_brush_factory_view_destroy    (GtkObject                *object);

static void   gimp_brush_factory_view_select_item     (GimpContainerEditor  *editor,
						       GimpViewable         *viewable);

static void   gimp_brush_factory_view_spacing_changed (GimpBrush            *brush,
						       GimpBrushFactoryView *view);
static void   gimp_brush_factory_view_spacing_update  (GtkAdjustment        *adjustment,
						       GimpBrushFactoryView *view);


static GimpDataFactoryViewClass *parent_class = NULL;


GType
gimp_brush_factory_view_get_type (void)
{
  static GType view_type = 0;

  if (! view_type)
    {
      static const GTypeInfo view_info =
      {
        sizeof (GimpBrushFactoryViewClass),
        NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) gimp_brush_factory_view_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GimpBrushFactoryView),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gimp_brush_factory_view_init,
      };

      view_type = g_type_register_static (GIMP_TYPE_DATA_FACTORY_VIEW,
                                          "GimpBrushFactoryView",
                                          &view_info, 0);
    }

  return view_type;
}

static void
gimp_brush_factory_view_class_init (GimpBrushFactoryViewClass *klass)
{
  GtkObjectClass           *object_class;
  GimpContainerEditorClass *editor_class;

  object_class = GTK_OBJECT_CLASS (klass);
  editor_class = GIMP_CONTAINER_EDITOR_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->destroy     = gimp_brush_factory_view_destroy;

  editor_class->select_item = gimp_brush_factory_view_select_item;
}

static void
gimp_brush_factory_view_init (GimpBrushFactoryView *view)
{
  GtkWidget *table;

  table = gtk_table_new (1, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 2);
  gtk_widget_show (table);

  view->spacing_adjustment =
    GTK_ADJUSTMENT (gimp_scale_entry_new (GTK_TABLE (table), 0, 0,
                                          _("Spacing:"), -1, -1,
                                          0.0, 1.0, 1000.0, 1.0, 10.0, 1,
                                          TRUE, 0.0, 0.0,
                                          NULL, NULL));

  view->spacing_scale = GIMP_SCALE_ENTRY_SCALE (view->spacing_adjustment);

  g_signal_connect (G_OBJECT (view->spacing_adjustment), "value_changed",
		    G_CALLBACK (gimp_brush_factory_view_spacing_update),
		    view);

  view->spacing_changed_handler_id = 0;
}

static void
gimp_brush_factory_view_destroy (GtkObject *object)
{
  GimpBrushFactoryView *view;

  view = GIMP_BRUSH_FACTORY_VIEW (object);

  if (view->spacing_changed_handler_id)
    {
      gimp_container_remove_handler (GIMP_CONTAINER_EDITOR (view)->view->container,
                                     view->spacing_changed_handler_id);

      view->spacing_changed_handler_id = 0;
    }

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

GtkWidget *
gimp_brush_factory_view_new (GimpViewType      view_type,
			     GimpDataFactory  *factory,
			     GimpDataEditFunc  edit_func,
			     GimpContext      *context,
			     gboolean          change_brush_spacing,
			     gint              preview_size,
			     gint              min_items_x,
			     gint              min_items_y,
			     GimpItemFactory  *item_factory)
{
  GimpBrushFactoryView *factory_view;
  GimpContainerEditor  *editor;

  g_return_val_if_fail (GIMP_IS_DATA_FACTORY (factory), NULL);
  g_return_val_if_fail (preview_size > 0 &&
			preview_size <= GIMP_PREVIEW_MAX_SIZE, NULL);
  g_return_val_if_fail (min_items_x > 0 && min_items_x <= 64, NULL);
  g_return_val_if_fail (min_items_y > 0 && min_items_y <= 64, NULL);

  factory_view = g_object_new (GIMP_TYPE_BRUSH_FACTORY_VIEW, NULL);

  factory_view->change_brush_spacing = change_brush_spacing;

  if (! gimp_data_factory_view_construct (GIMP_DATA_FACTORY_VIEW (factory_view),
					  view_type,
					  factory,
					  edit_func,
					  context,
					  preview_size,
					  min_items_x,
					  min_items_y,
					  item_factory))
    {
      g_object_unref (G_OBJECT (factory_view));
      return NULL;
    }

  editor = GIMP_CONTAINER_EDITOR (factory_view);

  /*  eek  */
  gtk_box_pack_end (GTK_BOX (editor->view),
                    factory_view->spacing_scale->parent,
                    FALSE, FALSE, 0);

  factory_view->spacing_changed_handler_id =
    gimp_container_add_handler (editor->view->container, "spacing_changed",
                                G_CALLBACK (gimp_brush_factory_view_spacing_changed),
                                factory_view);

  return GTK_WIDGET (factory_view);
}

static void
gimp_brush_factory_view_select_item (GimpContainerEditor *editor,
				     GimpViewable        *viewable)
{
  GimpBrushFactoryView *view;

  gboolean  edit_sensitive    = FALSE;
  gboolean  spacing_sensitive = FALSE;

  if (GIMP_CONTAINER_EDITOR_CLASS (parent_class)->select_item)
    GIMP_CONTAINER_EDITOR_CLASS (parent_class)->select_item (editor, viewable);

  view = GIMP_BRUSH_FACTORY_VIEW (editor);

  if (viewable &&
      gimp_container_have (GIMP_CONTAINER_EDITOR (view)->view->container,
			   GIMP_OBJECT (viewable)))
    {
      GimpBrush *brush;

      brush = GIMP_BRUSH (viewable);

      edit_sensitive    = GIMP_IS_BRUSH_GENERATED (brush);
      spacing_sensitive = TRUE;

      g_signal_handlers_block_by_func (G_OBJECT (view->spacing_adjustment),
				       gimp_brush_factory_view_spacing_update,
				       view);

      gtk_adjustment_set_value (view->spacing_adjustment,
				gimp_brush_get_spacing (brush));

      g_signal_handlers_unblock_by_func (G_OBJECT (view->spacing_adjustment),
					 gimp_brush_factory_view_spacing_update,
					 view);
    }

  gtk_widget_set_sensitive (GIMP_DATA_FACTORY_VIEW (view)->edit_button,
			    edit_sensitive);
  gtk_widget_set_sensitive (view->spacing_scale, spacing_sensitive);
}

static void
gimp_brush_factory_view_spacing_changed (GimpBrush            *brush,
					 GimpBrushFactoryView *view)
{
  if (brush == GIMP_CONTAINER_EDITOR (view)->view->context->brush)
    {
      g_signal_handlers_block_by_func (G_OBJECT (view->spacing_adjustment),
				       gimp_brush_factory_view_spacing_update,
				       view);

      gtk_adjustment_set_value (view->spacing_adjustment,
				gimp_brush_get_spacing (brush));

      g_signal_handlers_unblock_by_func (G_OBJECT (view->spacing_adjustment),
					 gimp_brush_factory_view_spacing_update,
					 view);
    }
}

static void
gimp_brush_factory_view_spacing_update (GtkAdjustment        *adjustment,
					GimpBrushFactoryView *view)
{
  GimpBrush *brush;

  brush = GIMP_CONTAINER_EDITOR (view)->view->context->brush;

  if (brush && view->change_brush_spacing)
    {
      g_signal_handlers_block_by_func (G_OBJECT (brush),
				       gimp_brush_factory_view_spacing_changed,
				       view);

      gimp_brush_set_spacing (brush, adjustment->value);

      g_signal_handlers_unblock_by_func (G_OBJECT (brush),
					 gimp_brush_factory_view_spacing_changed,
					 view);
    }
}
