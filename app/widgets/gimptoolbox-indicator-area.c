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

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "libgimpwidgets/gimpwidgets.h"

#include "widgets-types.h"

#include "core/gimpbrush.h"
#include "core/gimpcontext.h"
#include "core/gimpgradient.h"
#include "core/gimppattern.h"

#include "gimpdialogfactory.h"
#include "gimpdnd.h"
#include "gimppreview.h"
#include "gimptoolbox.h"
#include "gimptoolbox-indicator-area.h"

#include "libgimp/gimpintl.h"


#define CELL_SIZE        23  /*  The size of the previews  */
#define GRAD_CELL_WIDTH  48  /*  The width of the gradient preview  */
#define GRAD_CELL_HEIGHT 12  /*  The height of the gradient preview  */
#define CELL_PADDING      2  /*  How much between brush and pattern cells  */


static void
brush_preview_clicked (GtkWidget   *widget, 
		       GimpToolbox *toolbox)
{
  gimp_dialog_factory_dialog_raise (GIMP_DOCK (toolbox)->dialog_factory,
				    "gimp-brush-grid", -1);
}

static void
brush_preview_drop_brush (GtkWidget    *widget,
			  GimpViewable *viewable,
			  gpointer      data)
{
  GimpContext *context;

  context = GIMP_CONTEXT (data);

  gimp_context_set_brush (context, GIMP_BRUSH (viewable));
}

static void
pattern_preview_clicked (GtkWidget   *widget, 
			 GimpToolbox *toolbox)
{
  gimp_dialog_factory_dialog_raise (GIMP_DOCK (toolbox)->dialog_factory,
				    "gimp-pattern-grid", -1);
}

static void
pattern_preview_drop_pattern (GtkWidget    *widget,
			      GimpViewable *viewable,
			      gpointer      data)
{
  GimpContext *context;

  context = GIMP_CONTEXT (data);

  gimp_context_set_pattern (context, GIMP_PATTERN (viewable));
}

static void
gradient_preview_clicked (GtkWidget   *widget, 
			  GimpToolbox *toolbox)
{
  gimp_dialog_factory_dialog_raise (GIMP_DOCK (toolbox)->dialog_factory,
				    "gimp-gradient-list", -1);
}

static void
gradient_preview_drop_gradient (GtkWidget    *widget,
				GimpViewable *viewable,
				gpointer      data)
{
  GimpContext *context;

  context = GIMP_CONTEXT (data);

  gimp_context_set_gradient (context, GIMP_GRADIENT (viewable));
}


/*  public functions  */

GtkWidget *
gimp_toolbox_indicator_area_create (GimpToolbox *toolbox)
{
  GimpContext *context;
  GtkWidget   *indicator_table;
  GtkWidget   *brush_preview;
  GtkWidget   *pattern_preview;
  GtkWidget   *gradient_preview;

  g_return_val_if_fail (GIMP_IS_TOOLBOX (toolbox), NULL);

  context = GIMP_DOCK (toolbox)->context;

  indicator_table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (indicator_table), CELL_PADDING);
  gtk_table_set_col_spacings (GTK_TABLE (indicator_table), CELL_PADDING);

  /*  brush preview  */

  brush_preview =
    gimp_preview_new_full (GIMP_VIEWABLE (gimp_context_get_brush (context)),
                           CELL_SIZE, CELL_SIZE, 0,
                           FALSE, TRUE, TRUE);
  gtk_table_attach_defaults (GTK_TABLE (indicator_table), brush_preview,
			     0, 1, 0, 1);
  gtk_widget_show (brush_preview);

  gimp_help_set_help_data (brush_preview, 
			   _("The active brush.\n"
			     "Click to open the Brush Dialog."), NULL);

  g_signal_connect_object (G_OBJECT (context), "brush_changed",
			   G_CALLBACK (gimp_preview_set_viewable),
			   G_OBJECT (brush_preview),
			   G_CONNECT_SWAPPED);

  g_signal_connect (G_OBJECT (brush_preview), "clicked",
		    G_CALLBACK (brush_preview_clicked),
		    toolbox);

  gimp_gtk_drag_dest_set_by_type (brush_preview,
                                  GTK_DEST_DEFAULT_ALL,
                                  GIMP_TYPE_BRUSH,
                                  GDK_ACTION_COPY);
  gimp_dnd_viewable_dest_set (brush_preview,
                              GIMP_TYPE_BRUSH,
                              brush_preview_drop_brush,
                              context);

  /*  pattern preview  */

  pattern_preview =
    gimp_preview_new_full (GIMP_VIEWABLE (gimp_context_get_pattern (context)),
                           CELL_SIZE, CELL_SIZE, 0,
                           FALSE, TRUE, TRUE);
  gtk_table_attach_defaults (GTK_TABLE (indicator_table), pattern_preview,
			     1, 2, 0, 1);
  gtk_widget_show (pattern_preview);

  gimp_help_set_help_data (pattern_preview,
			   _("The active pattern.\n"
			     "Click to open the Pattern Dialog."), NULL);

  g_signal_connect_object (G_OBJECT (context), "pattern_changed",
			   G_CALLBACK (gimp_preview_set_viewable),
			   G_OBJECT (pattern_preview),
			   G_CONNECT_SWAPPED);

  g_signal_connect (G_OBJECT (pattern_preview), "clicked",
		    G_CALLBACK (pattern_preview_clicked),
		    toolbox);

  gimp_gtk_drag_dest_set_by_type (pattern_preview,
                                  GTK_DEST_DEFAULT_ALL,
                                  GIMP_TYPE_PATTERN,
                                  GDK_ACTION_COPY);
  gimp_dnd_viewable_dest_set (pattern_preview,
                              GIMP_TYPE_PATTERN,
                              pattern_preview_drop_pattern,
                              context);

  /*  gradient preview  */

  gradient_preview =
    gimp_preview_new_full (GIMP_VIEWABLE (gimp_context_get_gradient (context)),
                           GRAD_CELL_WIDTH, GRAD_CELL_HEIGHT, 0,
                           FALSE, TRUE, TRUE);
  gtk_table_attach_defaults (GTK_TABLE (indicator_table), gradient_preview,
			     0, 2, 1, 2);
  gtk_widget_show (gradient_preview);

  gimp_help_set_help_data (gradient_preview, 
			   _("The active gradient.\n"
			     "Click to open the Gradient Dialog."), NULL);

  g_signal_connect_object (G_OBJECT (context), "gradient_changed",
			   G_CALLBACK (gimp_preview_set_viewable),
			   G_OBJECT (gradient_preview),
			   G_CONNECT_SWAPPED);

  g_signal_connect (G_OBJECT (gradient_preview), "clicked",
		    G_CALLBACK (gradient_preview_clicked),
		    toolbox);

  gimp_gtk_drag_dest_set_by_type (gradient_preview,
                                  GTK_DEST_DEFAULT_ALL,
                                  GIMP_TYPE_GRADIENT,
                                  GDK_ACTION_COPY);
  gimp_dnd_viewable_dest_set (gradient_preview,
                              GIMP_TYPE_GRADIENT,
                              gradient_preview_drop_gradient,
                              context);

  gtk_widget_show (indicator_table);

  return indicator_table;
}
