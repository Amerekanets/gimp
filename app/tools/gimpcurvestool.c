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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef __GNUC__
#warning GTK_DISABLE_DEPRECATED
#endif
#undef GTK_DISABLE_DEPRECATED

#include <gtk/gtk.h>

#include "libgimpmath/gimpmath.h"
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "tools-types.h"

#include "base/curves.h"
#include "base/gimphistogram.h"
#include "base/gimplut.h"

#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimpimagemap.h"

#include "widgets/gimpcursor.h"
#include "widgets/gimpenummenu.h"

#include "display/gimpdisplay.h"

#include "gimpcurvestool.h"
#include "tool_manager.h"

#include "libgimp/gimpintl.h"


#define XRANGE_TOP     (1 << 0)
#define XRANGE_BOTTOM  (1 << 1)
#define YRANGE         (1 << 2)
#define ALL            (XRANGE_TOP | XRANGE_BOTTOM | YRANGE)

/*  NB: take care when changing these values: make sure the curve[] array in
 *  base/curves.h is large enough.
 */
#define GRAPH_WIDTH    256
#define GRAPH_HEIGHT   256
#define XRANGE_WIDTH   256
#define XRANGE_HEIGHT   16
#define YRANGE_WIDTH    16
#define YRANGE_HEIGHT  256
#define RADIUS           3
#define MIN_DISTANCE     8

#define GRAPH_MASK  (GDK_EXPOSURE_MASK       | \
                     GDK_LEAVE_NOTIFY_MASK   | \
		     GDK_POINTER_MOTION_MASK | \
		     GDK_BUTTON_PRESS_MASK   | \
		     GDK_BUTTON_RELEASE_MASK)


/*  local function prototypes  */

static void   gimp_curves_tool_class_init     (GimpCurvesToolClass *klass);
static void   gimp_curves_tool_init           (GimpCurvesTool      *c_tool);

static void   gimp_curves_tool_finalize       (GObject          *object);

static void   gimp_curves_tool_initialize     (GimpTool         *tool,
					       GimpDisplay      *gdisp);
static void   gimp_curves_tool_button_press   (GimpTool         *tool,
                                               GimpCoords       *coords,
                                               guint32           time,
					       GdkModifierType   state,
					       GimpDisplay      *gdisp);
static void   gimp_curves_tool_button_release (GimpTool         *tool,
                                               GimpCoords       *coords,
                                               guint32           time,
					       GdkModifierType   state,
					       GimpDisplay      *gdisp);
static void   gimp_curves_tool_motion         (GimpTool         *tool,
                                               GimpCoords       *coords,
                                               guint32           time,
					       GdkModifierType   state,
					       GimpDisplay      *gdisp);
static void   gimp_curves_tool_cursor_update  (GimpTool         *tool,
                                               GimpCoords       *coords,
                                               GdkModifierType   state,
                                               GimpDisplay      *gdisp);

static void   gimp_curves_tool_map            (GimpImageMapTool *image_map_tool);
static void   gimp_curves_tool_dialog         (GimpImageMapTool *image_map_tool);
static void   gimp_curves_tool_reset          (GimpImageMapTool *image_map_tool);

static void   curves_color_update             (GimpTool         *tool,
					       GimpDisplay      *gdisp,
					       GimpDrawable     *drawable,
					       GimpCoords       *coords);
static void   curves_add_point                (GimpCurvesTool   *c_tool,
					       gint              x,
					       gint              y,
					       gint              cchan);

static void   curves_update                   (GimpCurvesTool   *c_tool,
                                               gint              update);

static void   curves_channel_callback         (GtkWidget        *widget,
                                               GimpCurvesTool   *c_tool);
static void   curves_channel_reset_callback   (GtkWidget        *widget,
                                               GimpCurvesTool   *c_tool);

static gboolean curves_set_sensitive_callback (gpointer          item_data,
                                               GimpCurvesTool   *c_tool);
static void   curves_curve_type_callback      (GtkWidget        *widget,
                                               GimpCurvesTool   *c_tool);
static void   curves_load_callback            (GtkWidget        *widget,
                                               GimpCurvesTool   *c_tool);
static void   curves_save_callback            (GtkWidget        *widget,
                                               GimpCurvesTool   *c_tool);
static gint   curves_graph_events             (GtkWidget        *widget,
                                               GdkEvent         *event,
                                               GimpCurvesTool   *c_tool);
static void   curves_graph_expose             (GtkWidget        *widget,
                                               GdkRectangle     *area,
                                               GimpCurvesTool   *c_tool);

static void       file_dialog_create          (GimpCurvesTool   *c_tool);
static void       file_dialog_ok_callback     (GimpCurvesTool   *c_tool);

static gboolean   curves_read_from_file       (GimpCurvesTool   *c_tool,
                                               FILE             *file);
static void       curves_write_to_file        (GimpCurvesTool   *c_tool,
                                               FILE             *file);


static GimpImageMapToolClass *parent_class = NULL;


/*  public functions  */

void
gimp_curves_tool_register (GimpToolRegisterCallback  callback,
                           gpointer                  data)
{
  (* callback) (GIMP_TYPE_CURVES_TOOL,
                G_TYPE_NONE, NULL,
                FALSE,
                "gimp-curves-tool",
                _("Curves"),
                _("Adjust color curves"),
                N_("/Layer/Colors/Curves..."), NULL,
                NULL, "tools/curves.html",
                GIMP_STOCK_TOOL_CURVES,
                data);
}

GType
gimp_curves_tool_get_type (void)
{
  static GType tool_type = 0;

  if (! tool_type)
    {
      static const GTypeInfo tool_info =
      {
        sizeof (GimpCurvesToolClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_curves_tool_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data     */
	sizeof (GimpCurvesTool),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_curves_tool_init,
      };

      tool_type = g_type_register_static (GIMP_TYPE_IMAGE_MAP_TOOL,
					  "GimpCurvesTool", 
                                          &tool_info, 0);
    }

  return tool_type;
}


/*  private functions  */

static void
gimp_curves_tool_class_init (GimpCurvesToolClass *klass)
{
  GObjectClass          *object_class;
  GimpToolClass         *tool_class;
  GimpImageMapToolClass *image_map_tool_class;

  object_class         = G_OBJECT_CLASS (klass);
  tool_class           = GIMP_TOOL_CLASS (klass);
  image_map_tool_class = GIMP_IMAGE_MAP_TOOL_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gimp_curves_tool_finalize;

  tool_class->initialize     = gimp_curves_tool_initialize;
  tool_class->button_press   = gimp_curves_tool_button_press;
  tool_class->button_release = gimp_curves_tool_button_release;
  tool_class->motion         = gimp_curves_tool_motion;
  tool_class->cursor_update  = gimp_curves_tool_cursor_update;

  image_map_tool_class->map    = gimp_curves_tool_map;
  image_map_tool_class->dialog = gimp_curves_tool_dialog;
  image_map_tool_class->reset  = gimp_curves_tool_reset;
}

static void
gimp_curves_tool_init (GimpCurvesTool *c_tool)
{
  GimpImageMapTool *image_map_tool;
  gint              i;

  image_map_tool = GIMP_IMAGE_MAP_TOOL (c_tool);

  image_map_tool->shell_desc = _("Adjust Color Curves");

  c_tool->curves  = g_new0 (Curves, 1);
  c_tool->lut     = gimp_lut_new ();
  c_tool->channel = GIMP_HISTOGRAM_VALUE;

  curves_init (c_tool->curves);

  for (i = 0; i < G_N_ELEMENTS (c_tool->col_value); i++)
    c_tool->col_value[i] = -1;

  c_tool->cursor_x = -1;
  c_tool->cursor_y = -1;
}

static void
gimp_curves_tool_finalize (GObject *object)
{
  GimpCurvesTool *c_tool;

  c_tool = GIMP_CURVES_TOOL (object);

  if (c_tool->curves)
    {
      g_free (c_tool->curves);
      c_tool->curves = NULL;
    }
  if (c_tool->lut)
    {
      gimp_lut_free (c_tool->lut);
      c_tool->lut = NULL;
    }
  if (c_tool->cursor_layout)
    {
      g_object_unref (c_tool->cursor_layout);
      c_tool->cursor_layout = NULL;
    }
  if (c_tool->xpos_layout)
    {
      g_object_unref (c_tool->xpos_layout);
      c_tool->xpos_layout = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_curves_tool_initialize (GimpTool    *tool,
			     GimpDisplay *gdisp)
{
  GimpCurvesTool *c_tool;
  GimpDrawable   *drawable;

  c_tool = GIMP_CURVES_TOOL (tool);

  if (gimp_drawable_is_indexed (gimp_image_active_drawable (gdisp->gimage)))
    {
      g_message (_("Curves for indexed drawables cannot be adjusted."));
      return;
    }

  drawable = gimp_image_active_drawable (gdisp->gimage);

  curves_init (c_tool->curves);

  c_tool->color   = gimp_drawable_is_rgb (drawable);
  c_tool->channel = GIMP_HISTOGRAM_VALUE;

  c_tool->grab_point = -1;
  c_tool->last       = 0;

  GIMP_TOOL_CLASS (parent_class)->initialize (tool, gdisp);

  /* set the sensitivity of the channel menu based on the drawable type */
  gimp_option_menu_set_sensitive (GTK_OPTION_MENU (c_tool->channel_menu),
                                  (GimpOptionMenuSensitivityCallback) curves_set_sensitive_callback,
                                  c_tool);

  /* set the current selection */
  gimp_option_menu_set_history (GTK_OPTION_MENU (c_tool->channel_menu),
                                GINT_TO_POINTER (c_tool->channel));

  curves_update (c_tool, ALL);
}

static void
gimp_curves_tool_button_press (GimpTool        *tool,
                               GimpCoords      *coords,
                               guint32          time,
			       GdkModifierType  state,
			       GimpDisplay     *gdisp)
{
  GimpCurvesTool *c_tool;
  GimpDrawable   *drawable;

  c_tool = GIMP_CURVES_TOOL (tool);

  drawable = gimp_image_active_drawable (gdisp->gimage);

  tool->gdisp = gdisp;

  g_assert (drawable == tool->drawable);

#if 0
  if (drawable != tool->drawable)
    {
      gimp_tool_control_set_preserve (tool->control, TRUE);
      gimp_image_map_abort (GIMP_IMAGE_MAP_TOOL (tool)->image_map);
      gimp_tool_control_set_preserve (tool->control, FALSE);

      tool->drawable = drawable;

      c_tool->color  = gimp_drawable_is_rgb (drawable);

      GIMP_IMAGE_MAP_TOOL (tool)->drawable  = drawable;
      GIMP_IMAGE_MAP_TOOL (tool)->image_map = gimp_image_map_new (TRUE,
                                                                  drawable);

      gimp_option_menu_set_sensitive 
        (GTK_OPTION_MENU (c_tool->channel_menu),
         (GimpOptionMenuSensitivityCallback) curves_set_sensitive_callback,
         c_tool);
    }
#endif

  gimp_tool_control_activate (tool->control);

  curves_color_update (tool, gdisp, drawable, coords);
  gtk_widget_queue_draw (c_tool->graph);
}

static void
gimp_curves_tool_button_release (GimpTool        *tool,
                                 GimpCoords      *coords,
                                 guint32          time,
				 GdkModifierType  state,
				 GimpDisplay     *gdisp)
{
  GimpCurvesTool *c_tool;
  GimpDrawable   *drawable;

  c_tool = GIMP_CURVES_TOOL (tool);

  drawable = gimp_image_active_drawable (gdisp->gimage);

  curves_color_update (tool, gdisp, drawable, coords);

  if (state & GDK_SHIFT_MASK)
    {
      curves_add_point (c_tool, coords->x, coords->y, c_tool->channel);
      curves_calculate_curve (c_tool->curves, c_tool->channel);
    }
  else if (state & GDK_CONTROL_MASK)
    {
      gint i;

      for (i = 0; i < 5; i++)
        {
          curves_add_point (c_tool, coords->x, coords->y, i);
          curves_calculate_curve (c_tool->curves, c_tool->channel);
        }
    }

  gimp_tool_control_halt (tool->control);

  gtk_widget_queue_draw (c_tool->graph);
}

static void
gimp_curves_tool_motion (GimpTool        *tool,
                         GimpCoords      *coords,
                         guint32          time,
			 GdkModifierType  state,
			 GimpDisplay     *gdisp)
{
  GimpCurvesTool *c_tool;
  GimpDrawable   *drawable;

  c_tool = GIMP_CURVES_TOOL (tool);

  drawable = gimp_image_active_drawable (gdisp->gimage);

  curves_color_update (tool, gdisp, drawable, coords);
  gtk_widget_queue_draw (c_tool->graph);
}

static void
gimp_curves_tool_cursor_update (GimpTool        *tool,
                                GimpCoords      *coords,
                                GdkModifierType  state,
                                GimpDisplay     *gdisp)
{
  if (gimp_display_coords_in_active_drawable (gdisp, coords))
    {
      gimp_tool_control_set_tool_cursor (tool->control,
                                         GIMP_COLOR_PICKER_TOOL_CURSOR);
      gimp_tool_control_set_cursor_modifier (tool->control,
                                             (state & (GDK_SHIFT_MASK |
                                                       GDK_CONTROL_MASK) ?
                                              GIMP_CURSOR_MODIFIER_PLUS  :
                                              GIMP_CURSOR_MODIFIER_NONE));
    }
  else
    {
      gimp_tool_control_set_tool_cursor (tool->control,
                                         GIMP_TOOL_CURSOR_NONE);
      gimp_tool_control_set_cursor_modifier (tool->control,
                                             GIMP_CURSOR_MODIFIER_NONE);
    }

  GIMP_TOOL_CLASS (parent_class)->cursor_update (tool, coords, state, gdisp);
}

static void
curves_color_update (GimpTool       *tool,
                     GimpDisplay    *gdisp,
                     GimpDrawable   *drawable,
                     GimpCoords     *coords)
{
  GimpCurvesTool *c_tool;
  guchar         *color;
  gint            x;
  gint            y;

  if (! (tool && gimp_tool_control_is_active (tool->control)))
    return;

  c_tool = GIMP_CURVES_TOOL (tool);

  gimp_drawable_offsets (drawable, &x, &y);

  x = RINT (coords->x) - x;
  y = RINT (coords->y) - y;

  color = gimp_image_map_get_color_at (GIMP_IMAGE_MAP_TOOL (tool)->image_map,
                                       x, y);
  if (color)
    {
      gint maxval;

      c_tool->col_value[GIMP_HISTOGRAM_RED]   = color[RED_PIX];
      c_tool->col_value[GIMP_HISTOGRAM_GREEN] = color[GREEN_PIX];
      c_tool->col_value[GIMP_HISTOGRAM_BLUE]  = color[BLUE_PIX];
      
      if (gimp_drawable_has_alpha (drawable))
        c_tool->col_value[GIMP_HISTOGRAM_ALPHA] = color[3];
      
      if (gimp_drawable_is_indexed (drawable))
        c_tool->col_value[GIMP_HISTOGRAM_ALPHA] = color[4];
      
      maxval = MAX (color[RED_PIX], color[GREEN_PIX]);
      c_tool->col_value[GIMP_HISTOGRAM_VALUE] = MAX (maxval, color[BLUE_PIX]);
      
      g_free (color);
    }
  else
    {
      gint i;

      for (i = 0; i <= GIMP_HISTOGRAM_ALPHA; i++)
        c_tool->col_value[i] = -1;
      
      return;
    }
}

static void
curves_add_point (GimpCurvesTool *c_tool,
		  gint            x,
		  gint            y,
		  gint            cchan)
{
  /* Add point onto the curve */
  gint closest_point = 0;
  gint distance;
  gint curvex;
  gint i;

  switch (c_tool->curves->curve_type[cchan])
    {
    case GIMP_CURVE_SMOOTH:
      curvex   = c_tool->col_value[cchan];
      distance = G_MAXINT;

      for (i = 0; i < 17; i++)
	{
	  if (c_tool->curves->points[cchan][i][0] != -1)
	    if (abs (curvex - c_tool->curves->points[cchan][i][0]) < distance)
	      {
		distance = abs (curvex - c_tool->curves->points[cchan][i][0]);
		closest_point = i;
	      }
	}
      
      if (distance > MIN_DISTANCE)
	closest_point = (curvex + 8) / 16;
      
      c_tool->curves->points[cchan][closest_point][0] = curvex;
      c_tool->curves->points[cchan][closest_point][1] = c_tool->curves->curve[cchan][curvex];
      break;
      
    case GIMP_CURVE_FREE:
      c_tool->curves->curve[cchan][x] = 255 - y;
      break;
    }
}

static void
gimp_curves_tool_map (GimpImageMapTool *image_map_tool)
{
  GimpCurvesTool *c_tool;

  c_tool = GIMP_CURVES_TOOL (image_map_tool);

  gimp_lut_setup (c_tool->lut, 
		  (GimpLutFunc) curves_lut_func,
                  c_tool->curves,
		  gimp_drawable_bytes (image_map_tool->drawable));

  gimp_image_map_apply (image_map_tool->image_map,
                        (GimpImageMapApplyFunc) gimp_lut_process_2,
                        c_tool->lut);
}


/*******************/
/*  Curves dialog  */
/*******************/

static void
gimp_curves_tool_dialog (GimpImageMapTool *image_map_tool)
{
  GimpCurvesTool *c_tool;
  GtkWidget      *hbox;
  GtkWidget      *vbox;
  GtkWidget      *hbbox;
  GtkWidget      *frame;
  GtkWidget      *menu;
  GtkWidget      *table;
  GtkWidget      *button;

  c_tool = GIMP_CURVES_TOOL (image_map_tool);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (image_map_tool->main_vbox), hbox,
                      FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  vbox = gtk_vbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, FALSE, 0);
  gtk_widget_show (vbox);

  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  /*  The option menu for selecting channels  */
  hbox = gtk_hbox_new (FALSE, 4);
  menu = gimp_enum_option_menu_new (GIMP_TYPE_HISTOGRAM_CHANNEL,
                                    G_CALLBACK (curves_channel_callback),
                                    c_tool);
  gimp_enum_option_menu_set_stock_prefix (GTK_OPTION_MENU (menu),
                                          "gimp-channel");
  gtk_box_pack_start (GTK_BOX (hbox), menu, FALSE, FALSE, 0);
  gtk_widget_show (menu);

  c_tool->channel_menu = menu;

  button = gtk_button_new_with_mnemonic (_("R_eset Channel"));
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  g_signal_connect (button, "clicked",
                    G_CALLBACK (curves_channel_reset_callback),
                    c_tool);

  gimp_table_attach_aligned (GTK_TABLE (table), 0, 0,
                             _("Modify Curves for Channel:"), 1.0, 0.5,
                             hbox, 1, FALSE);

  /*  The table for the yrange and the graph  */
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 2);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_box_pack_start (GTK_BOX (hbox), table, TRUE, FALSE, 0);

  /*  The range drawing area  */
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_table_attach (GTK_TABLE (table), frame, 0, 1, 0, 1,
		    GTK_EXPAND, GTK_EXPAND, 0, 0);
  gtk_widget_show (frame);

  c_tool->yrange = gtk_preview_new (GTK_PREVIEW_COLOR);
  gtk_preview_size (GTK_PREVIEW (c_tool->yrange), YRANGE_WIDTH, YRANGE_HEIGHT);
  gtk_container_add (GTK_CONTAINER (frame), c_tool->yrange);
  gtk_widget_show (c_tool->yrange);

  /*  The curves graph  */
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_table_attach (GTK_TABLE (table), frame, 1, 2, 0, 1,
		    GTK_SHRINK | GTK_FILL,
		    GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (frame);

  c_tool->graph = gtk_drawing_area_new ();
  gtk_widget_set_size_request (c_tool->graph,
                               GRAPH_WIDTH  + RADIUS * 2,
                               GRAPH_HEIGHT + RADIUS * 2);
  gtk_widget_set_events (c_tool->graph, GRAPH_MASK);
  gtk_container_add (GTK_CONTAINER (frame), c_tool->graph);
  gtk_widget_show (c_tool->graph);

  g_signal_connect (c_tool->graph, "event",
		    G_CALLBACK (curves_graph_events),
		    c_tool);

  /*  The range drawing area  */
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_table_attach (GTK_TABLE (table), frame, 1, 2, 1, 2,
		    GTK_EXPAND, GTK_EXPAND, 0, 0);
  gtk_widget_show (frame);

  c_tool->xrange = gtk_preview_new (GTK_PREVIEW_COLOR);
  gtk_preview_size (GTK_PREVIEW (c_tool->xrange), XRANGE_WIDTH, XRANGE_HEIGHT);
  gtk_container_add (GTK_CONTAINER (frame), c_tool->xrange);
  gtk_widget_show (c_tool->xrange);

  gtk_widget_show (table);

  hbox = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_end (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);
  
  /*  Horizontal button box for load / save */
  frame = gtk_frame_new (_("All Channels"));
  gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  hbbox = gtk_hbutton_box_new ();
  gtk_container_set_border_width (GTK_CONTAINER (hbbox), 2);
  gtk_box_set_spacing (GTK_BOX (hbbox), 4);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbbox), GTK_BUTTONBOX_SPREAD);
  gtk_container_add (GTK_CONTAINER (frame), hbbox);
  gtk_widget_show (hbbox);

  button = gtk_button_new_from_stock (GTK_STOCK_OPEN);
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (hbbox), button, FALSE, FALSE, 0);
  gimp_help_set_help_data (button, _("Read curves settings from file"), NULL);
  gtk_widget_show (button);

  g_signal_connect (button, "clicked",
		    G_CALLBACK (curves_load_callback),
		    c_tool);

  button = gtk_button_new_from_stock (GTK_STOCK_SAVE);
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (hbbox), button, FALSE, FALSE, 0);
  gimp_help_set_help_data (button, _("Save curves settings to file"), NULL);
  gtk_widget_show (button);

  g_signal_connect (button, "clicked",
		    G_CALLBACK (curves_save_callback),
		    c_tool);

  /*  The radio box for selecting the curve type  */
  frame = gtk_frame_new (_("Curve Type"));
  gtk_box_pack_end (GTK_BOX (hbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  hbox = gimp_enum_stock_box_new (GIMP_TYPE_CURVE_TYPE,
				  "gimp-curve",
				  G_CALLBACK (curves_curve_type_callback),
				  c_tool,
				  &c_tool->curve_type);

  gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);
  gtk_box_set_spacing (GTK_BOX (hbox), 4);

  gtk_container_add (GTK_CONTAINER (frame), hbox);
  gtk_widget_show (hbox);
}

static void
gimp_curves_tool_reset (GimpImageMapTool *image_map_tool)
{
  GimpCurvesTool       *c_tool;
  GimpHistogramChannel  channel;

  c_tool = GIMP_CURVES_TOOL (image_map_tool);

  c_tool->grab_point = -1;

  for (channel =  GIMP_HISTOGRAM_VALUE;
       channel <= GIMP_HISTOGRAM_ALPHA;
       channel++)
    curves_channel_reset (c_tool->curves, channel);

  curves_update (c_tool, XRANGE_TOP);
  gtk_widget_queue_draw (c_tool->graph);
}


/* TODO: preview alpha channel stuff correctly.  -- austin, 20/May/99 */
static void
curves_update (GimpCurvesTool *c_tool,
	       gint            update)
{
  GimpHistogramChannel sel_channel;
  gint                 i, j;
  
  if (c_tool->color)
    {
      sel_channel = c_tool->channel;
    }
  else 
    {
      if (c_tool->channel == 2)
        sel_channel = GIMP_HISTOGRAM_ALPHA;
      else
        sel_channel = GIMP_HISTOGRAM_VALUE;
    }

  if (update & XRANGE_TOP)
    {
      guchar buf[XRANGE_WIDTH * 3];

      switch (sel_channel)
	{
	case GIMP_HISTOGRAM_VALUE:
	case GIMP_HISTOGRAM_ALPHA:
	  for (i = 0; i < XRANGE_HEIGHT / 2; i++)
	    {
	      for (j = 0; j < XRANGE_WIDTH ; j++)
		{
		  buf[j * 3 + 0] = c_tool->curves->curve[sel_channel][j];
		  buf[j * 3 + 1] = c_tool->curves->curve[sel_channel][j];
		  buf[j * 3 + 2] = c_tool->curves->curve[sel_channel][j];
		}
	      gtk_preview_draw_row (GTK_PREVIEW (c_tool->xrange),
				    buf, 0, i, XRANGE_WIDTH);
	    }
	  break;

	case GIMP_HISTOGRAM_RED:
	case GIMP_HISTOGRAM_GREEN:
	case GIMP_HISTOGRAM_BLUE:
	  {
	    for (i = 0; i < XRANGE_HEIGHT / 2; i++)
	      {
		for (j = 0; j < XRANGE_WIDTH; j++)
		  {
		    buf[j * 3 + 0] = c_tool->curves->curve[GIMP_HISTOGRAM_RED][j];
		    buf[j * 3 + 1] = c_tool->curves->curve[GIMP_HISTOGRAM_GREEN][j];
		    buf[j * 3 + 2] = c_tool->curves->curve[GIMP_HISTOGRAM_BLUE][j];
		  }
		gtk_preview_draw_row (GTK_PREVIEW (c_tool->xrange),
				      buf, 0, i, XRANGE_WIDTH);
	      }
	    break;
	  }

	default:
	  g_warning ("unknown channel type %d, can't happen!?!?",
		     c_tool->channel);
	  break;
	}

      gtk_widget_queue_draw_area (c_tool->xrange,
                                  0, 0,
                                  XRANGE_WIDTH, XRANGE_HEIGHT / 2);
    }

  if (update & XRANGE_BOTTOM)
    {
      guchar buf[XRANGE_WIDTH * 3];

      for (i = 0; i < XRANGE_WIDTH; i++)
        {
          buf[i * 3 + 0] = i;
          buf[i * 3 + 1] = i;
          buf[i * 3 + 2] = i;
        }

      for (i = XRANGE_HEIGHT / 2; i < XRANGE_HEIGHT; i++)
	gtk_preview_draw_row (GTK_PREVIEW (c_tool->xrange),
                              buf, 0, i, XRANGE_WIDTH);

      gtk_widget_queue_draw_area (c_tool->xrange,
                                  0, XRANGE_HEIGHT / 2,
                                  XRANGE_WIDTH, XRANGE_HEIGHT / 2);
    }

  if (update & YRANGE)
    {
      guchar buf[YRANGE_WIDTH * 3];
      guchar pix[3];

      for (i = 0; i < YRANGE_HEIGHT; i++)
	{
	  switch (sel_channel)
	    {
	    case GIMP_HISTOGRAM_VALUE:
	    case GIMP_HISTOGRAM_ALPHA:
	      pix[0] = pix[1] = pix[2] = (255 - i);
	      break;

	    case GIMP_HISTOGRAM_RED:
	    case GIMP_HISTOGRAM_GREEN:
	    case GIMP_HISTOGRAM_BLUE:
	      pix[0] = pix[1] = pix[2] = 0;
	      pix[sel_channel - 1] = (255 - i);
	      break;

	    default:
	      g_warning ("unknown channel type %d, can't happen!?!?",
			 c_tool->channel);
	      break;
	    }

	  for (j = 0; j < YRANGE_WIDTH * 3; j++)
	    buf[j] = pix[j%3];

	  gtk_preview_draw_row (GTK_PREVIEW (c_tool->yrange),
				buf, 0, i, YRANGE_WIDTH);
	}

      gtk_widget_queue_draw (c_tool->yrange);
    }
}

static void
curves_channel_callback (GtkWidget      *widget,
			 GimpCurvesTool *c_tool)
{
  gimp_menu_item_update (widget, &c_tool->channel);

  if (! c_tool->color)
    {
      if (c_tool->channel > 1)
        c_tool->channel = 2;
      else 
        c_tool->channel = 1;
    }

  gimp_radio_group_set_active (GTK_RADIO_BUTTON (c_tool->curve_type),
			       GINT_TO_POINTER (c_tool->curves->curve_type[c_tool->channel]));

  curves_update (c_tool, XRANGE_TOP | YRANGE);
  gtk_widget_queue_draw (c_tool->graph);
}

static void
curves_channel_reset_callback (GtkWidget      *widget,
                               GimpCurvesTool *c_tool)
{
  c_tool->grab_point = -1;

  curves_channel_reset (c_tool->curves, c_tool->channel);

  curves_update (c_tool, XRANGE_TOP);
  gtk_widget_queue_draw (c_tool->graph);

  gimp_image_map_tool_preview (GIMP_IMAGE_MAP_TOOL (c_tool));
}

static gboolean
curves_set_sensitive_callback (gpointer        item_data,
                               GimpCurvesTool *c_tool)
{
  GimpHistogramChannel  channel = GPOINTER_TO_INT (item_data);

  switch (channel)
    {
    case GIMP_HISTOGRAM_VALUE:
      return TRUE;
    case GIMP_HISTOGRAM_RED:
    case GIMP_HISTOGRAM_GREEN:
    case GIMP_HISTOGRAM_BLUE:
      return c_tool->color;
    case GIMP_HISTOGRAM_ALPHA:
      return gimp_drawable_has_alpha (GIMP_IMAGE_MAP_TOOL (c_tool)->drawable);
    }

  return FALSE;
}

static void
curves_curve_type_callback (GtkWidget      *widget,
			    GimpCurvesTool *c_tool)
{
  GimpCurveType curve_type;

  gimp_radio_button_update (widget, &curve_type);

  if (c_tool->curves->curve_type[c_tool->channel] != curve_type)
    {
      c_tool->curves->curve_type[c_tool->channel] = curve_type;

      if (curve_type == GIMP_CURVE_SMOOTH)
        {
          gint   i;
          gint32 index;

          /*  pick representative points from the curve
           *  and make them control points
           */
          for (i = 0; i <= 8; i++)
            {
              index = CLAMP0255 (i * 32);
              c_tool->curves->points[c_tool->channel][i * 2][0] = index;
              c_tool->curves->points[c_tool->channel][i * 2][1] = c_tool->curves->curve[c_tool->channel][index];
            }
        }

      curves_calculate_curve (c_tool->curves, c_tool->channel);

      curves_update (c_tool, XRANGE_TOP);
      gtk_widget_queue_draw (c_tool->graph);

      gimp_image_map_tool_preview (GIMP_IMAGE_MAP_TOOL (c_tool));
    }
}

static gboolean
curves_graph_events (GtkWidget      *widget,
		     GdkEvent       *event,
		     GimpCurvesTool *c_tool)
{
  static GdkCursorType cursor_type = GDK_TOP_LEFT_ARROW;

  GdkCursorType   new_cursor = GDK_X_CURSOR;
  GdkEventButton *bevent;
  GdkEventMotion *mevent;
  gint            i;
  gint            tx, ty;
  gint            x, y;
  gint            closest_point;
  gint            distance;
  gint            x1, x2, y1, y2;

  if (event->type == GDK_EXPOSE)
    {
      curves_graph_expose (widget, &((GdkEventExpose *) event)->area, c_tool);
      return TRUE;
    }

  /*  get the pointer position  */
  gdk_window_get_pointer (c_tool->graph->window, &tx, &ty, NULL);
  x = CLAMP ((tx - RADIUS), 0, 255);
  y = CLAMP ((ty - RADIUS), 0, 255);

  distance = G_MAXINT;
  for (i = 0, closest_point = 0; i < 17; i++)
    {
      if (c_tool->curves->points[c_tool->channel][i][0] != -1)
	if (abs (x - c_tool->curves->points[c_tool->channel][i][0]) < distance)
	  {
	    distance = abs (x - c_tool->curves->points[c_tool->channel][i][0]);
	    closest_point = i;
	  }
    }
  if (distance > MIN_DISTANCE)
    closest_point = (x + 8) / 16;

  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
      bevent = (GdkEventButton *) event;

      new_cursor = GDK_TCROSS;

      switch (c_tool->curves->curve_type[c_tool->channel])
	{
	case GIMP_CURVE_SMOOTH:
	  /*  determine the leftmost and rightmost points  */
	  c_tool->leftmost = -1;
	  for (i = closest_point - 1; i >= 0; i--)
	    if (c_tool->curves->points[c_tool->channel][i][0] != -1)
	      {
		c_tool->leftmost = c_tool->curves->points[c_tool->channel][i][0];
		break;
	      }
	  c_tool->rightmost = 256;
	  for (i = closest_point + 1; i < 17; i++)
	    if (c_tool->curves->points[c_tool->channel][i][0] != -1)
	      {
		c_tool->rightmost = c_tool->curves->points[c_tool->channel][i][0];
		break;
	      }

	  c_tool->grab_point = closest_point;
	  c_tool->curves->points[c_tool->channel][c_tool->grab_point][0] = x;
	  c_tool->curves->points[c_tool->channel][c_tool->grab_point][1] = 255 - y;
	  break;

	case GIMP_CURVE_FREE:
	  c_tool->curves->curve[c_tool->channel][x] = 255 - y;
	  c_tool->grab_point = x;
	  c_tool->last = y;
	  break;
	}
      
      gtk_grab_add (widget);

      curves_calculate_curve (c_tool->curves, c_tool->channel);

      curves_update (c_tool, XRANGE_TOP);
      gtk_widget_queue_draw (c_tool->graph);
      return TRUE;

    case GDK_BUTTON_RELEASE:
      new_cursor = GDK_FLEUR;
      c_tool->grab_point = -1;

      gimp_image_map_tool_preview (GIMP_IMAGE_MAP_TOOL (c_tool));

      gtk_grab_remove (widget);
      return TRUE;

    case GDK_MOTION_NOTIFY:
      mevent = (GdkEventMotion *) event;

      switch (c_tool->curves->curve_type[c_tool->channel])
	{
	case GIMP_CURVE_SMOOTH:
	  /*  If no point is grabbed...  */
	  if (c_tool->grab_point == -1)
	    {
	      if (c_tool->curves->points[c_tool->channel][closest_point][0] != -1)
		new_cursor = GDK_FLEUR;
	      else
		new_cursor = GDK_TCROSS;
	    }
	  /*  Else, drag the grabbed point  */
	  else
	    {
	      new_cursor = GDK_TCROSS;

	      c_tool->curves->points[c_tool->channel][c_tool->grab_point][0] = -1;

	      if (x > c_tool->leftmost && x < c_tool->rightmost)
		{
		  closest_point = (x + 8) / 16;
		  if (c_tool->curves->points[c_tool->channel][closest_point][0] == -1)
		    c_tool->grab_point = closest_point;

		  c_tool->curves->points[c_tool->channel][c_tool->grab_point][0] = x;
		  c_tool->curves->points[c_tool->channel][c_tool->grab_point][1] = 255 - y;
		}

	      curves_calculate_curve (c_tool->curves, c_tool->channel);
	    }
	  break;

	case GIMP_CURVE_FREE:
	  if (c_tool->grab_point != -1)
	    {
	      if (c_tool->grab_point > x)
		{
		  x1 = x;
		  x2 = c_tool->grab_point;
		  y1 = y;
		  y2 = c_tool->last;
		}
	      else
		{
		  x1 = c_tool->grab_point;
		  x2 = x;
		  y1 = c_tool->last;
		  y2 = y;
		}

	      if (x2 != x1)
		for (i = x1; i <= x2; i++)
		  c_tool->curves->curve[c_tool->channel][i] = 255 - (y1 + ((y2 - y1) * (i - x1)) / (x2 - x1));
	      else
		c_tool->curves->curve[c_tool->channel][x] = 255 - y;

	      c_tool->grab_point = x;
	      c_tool->last = y;
	    }

	  if (mevent->state & GDK_BUTTON1_MASK)
	    new_cursor = GDK_TCROSS;
	  else
	    new_cursor = GDK_PENCIL;

	  break;
	}

      if (new_cursor != cursor_type)
	{
	  GdkCursor *cursor;

	  cursor_type = new_cursor;

	  cursor = gimp_cursor_new (cursor_type,
				    GIMP_TOOL_CURSOR_NONE,
				    GIMP_CURSOR_MODIFIER_NONE);
	  gdk_window_set_cursor (c_tool->graph->window, cursor);
	  gdk_cursor_unref (cursor);
	}

      curves_update (c_tool, XRANGE_TOP);

      c_tool->cursor_x = tx - RADIUS;
      c_tool->cursor_y = ty - RADIUS;

      gtk_widget_queue_draw (c_tool->graph);
      return TRUE;

    case GDK_LEAVE_NOTIFY:
      c_tool->cursor_x = -1;
      c_tool->cursor_y = -1;

      gtk_widget_queue_draw (c_tool->graph);
      return TRUE;

    default:
      break;
    }

  return FALSE;
}

static void
curve_print_loc (GimpCurvesTool *c_tool)
{
  gchar buf[32];
  gint  x, y;
  gint  w, h;

  if (c_tool->cursor_x < 0 || c_tool->cursor_x > 255)
    return;

  if (c_tool->cursor_y < 0 || c_tool->cursor_y > 255)
    return;

  if (! c_tool->cursor_layout)
    {
      c_tool->cursor_layout = gtk_widget_create_pango_layout (c_tool->graph,
                                                              "x:888 y:888");
      pango_layout_get_pixel_extents (c_tool->cursor_layout, 
                                      NULL, &c_tool->cursor_rect);
    }
  
  x = RADIUS * 2 + 2;
  y = RADIUS * 2 + 2;
  w = c_tool->cursor_rect.width  + 4;
  h = c_tool->cursor_rect.height + 4;
  
  gdk_draw_rectangle (c_tool->graph->window, 
                      c_tool->graph->style->bg_gc[GTK_STATE_ACTIVE],
                      TRUE,
                      x, y, w + 1, h + 1);
  gdk_draw_rectangle (c_tool->graph->window, 
                      c_tool->graph->style->black_gc,
                      FALSE,
                      x, y, w, h);
  
  g_snprintf (buf, sizeof (buf), "x:%3d y:%3d",
              c_tool->cursor_x, 255 - c_tool->cursor_y);
  pango_layout_set_text (c_tool->cursor_layout, buf, 11);
  
  gdk_draw_layout (c_tool->graph->window, 
                   c_tool->graph->style->black_gc,
                   x + 2, y + 2,
                   c_tool->cursor_layout);
}

static void
curves_graph_expose (GtkWidget      *widget,
                     GdkRectangle   *area,
                     GimpCurvesTool *c_tool)
{
  GimpHistogramChannel sel_channel;

  gchar    buf[32];
  gint     offset;
  gint     height;
  gint     i;
  GdkPoint points[256];

  if (c_tool->color)
    {
      sel_channel = c_tool->channel;
    }
  else 
    {
      if (c_tool->channel == 2)
        sel_channel = GIMP_HISTOGRAM_ALPHA;
      else
        sel_channel = GIMP_HISTOGRAM_VALUE;
    }

  /*  Draw the grid lines  */
  for (i = 0; i < 5; i++)
    {
      gdk_draw_line (widget->window,
                     c_tool->graph->style->dark_gc[GTK_STATE_NORMAL],
                     RADIUS, i * (GRAPH_HEIGHT / 4) + RADIUS,
                     GRAPH_WIDTH + RADIUS, i * (GRAPH_HEIGHT / 4) + RADIUS);
      gdk_draw_line (widget->window,
                     c_tool->graph->style->dark_gc[GTK_STATE_NORMAL],
                     i * (GRAPH_WIDTH / 4) + RADIUS, RADIUS,
                     i * (GRAPH_WIDTH / 4) + RADIUS, GRAPH_HEIGHT + RADIUS);
    }

  /*  Draw the curve  */
  for (i = 0; i < 256; i++)
    {
      points[i].x = i + RADIUS;
      points[i].y = 255 - c_tool->curves->curve[c_tool->channel][i] + RADIUS;
    }

  if (c_tool->curves->curve_type[c_tool->channel] == GIMP_CURVE_FREE)
    {
      gdk_draw_points (widget->window,
                       c_tool->graph->style->black_gc,
                       points, 256);
    }
  else
    {
      gdk_draw_lines (widget->window,
                      c_tool->graph->style->black_gc,
                      points, 256);
      
      /*  Draw the points  */
      for (i = 0; i < 17; i++)
        {
          if (c_tool->curves->points[c_tool->channel][i][0] != -1)
            gdk_draw_arc (widget->window,
                          c_tool->graph->style->black_gc,
                          TRUE,
                          c_tool->curves->points[c_tool->channel][i][0],
                          255 - c_tool->curves->points[c_tool->channel][i][1],
                          RADIUS * 2, RADIUS * 2, 0, 23040);
        }
    }
  
  if (c_tool->col_value[sel_channel] >= 0)
    {
      /* draw the color line */
      gdk_draw_line (widget->window,
                     c_tool->graph->style->black_gc,
                     c_tool->col_value[sel_channel] + RADIUS,
                     RADIUS,
                     c_tool->col_value[sel_channel] + RADIUS,
                     GRAPH_HEIGHT + RADIUS);
      
      /* and xpos indicator */
      g_snprintf (buf, sizeof (buf), "x:%d",
                  c_tool->col_value[sel_channel]);
      
      if (! c_tool->xpos_layout)
        c_tool->xpos_layout = gtk_widget_create_pango_layout (c_tool->graph,
                                                              buf);
      else
        pango_layout_set_text (c_tool->xpos_layout, buf, -1);
      
      pango_layout_get_pixel_size (c_tool->xpos_layout, &offset, &height);
      
      if ((c_tool->col_value[sel_channel] + RADIUS) < 127)
        offset = RADIUS + 4;
      else
        offset = - (offset + 2);
      
      gdk_draw_layout (widget->window,
                       c_tool->graph->style->black_gc,
                       c_tool->col_value[sel_channel] + offset,
                       GRAPH_HEIGHT - height - 2,
                       c_tool->xpos_layout);
    }
  
  curve_print_loc (c_tool);
}


static void
curves_load_callback (GtkWidget      *widget,
		      GimpCurvesTool *c_tool)
{
  if (! c_tool->file_dialog)
    file_dialog_create (c_tool);
  else if (GTK_WIDGET_VISIBLE (c_tool->file_dialog))
    return;

  c_tool->is_save = FALSE;

  gtk_window_set_title (GTK_WINDOW (c_tool->file_dialog), _("Load Curves"));
  gtk_widget_show (c_tool->file_dialog);
}

static void
curves_save_callback (GtkWidget      *widget,
		      GimpCurvesTool *c_tool)
{
  if (! c_tool->file_dialog)
    file_dialog_create (c_tool);
  else if (GTK_WIDGET_VISIBLE (c_tool->file_dialog))
    return;

  c_tool->is_save = TRUE;

  gtk_window_set_title (GTK_WINDOW (c_tool->file_dialog), _("Save Curves"));
  gtk_widget_show (c_tool->file_dialog);
}

static void
file_dialog_create (GimpCurvesTool *c_tool)
{
  GtkFileSelection *file_dlg;
  gchar            *temp;

  c_tool->file_dialog = gtk_file_selection_new ("");

  file_dlg = GTK_FILE_SELECTION (c_tool->file_dialog);

  gtk_window_set_wmclass (GTK_WINDOW (file_dlg), "load_save_curves", "Gimp");
  gtk_window_set_position (GTK_WINDOW (file_dlg), GTK_WIN_POS_MOUSE);

  gtk_container_set_border_width (GTK_CONTAINER (file_dlg), 2);
  gtk_container_set_border_width (GTK_CONTAINER (file_dlg->button_area), 2);

  g_object_add_weak_pointer (G_OBJECT (file_dlg),
                             (gpointer) &c_tool->file_dialog);

  gtk_window_set_transient_for (GTK_WINDOW (file_dlg),
                                GTK_WINDOW (GIMP_IMAGE_MAP_TOOL (c_tool)->shell));
  gtk_window_set_destroy_with_parent (GTK_WINDOW (file_dlg), TRUE);

  g_signal_connect_swapped (file_dlg->ok_button, "clicked",
                            G_CALLBACK (file_dialog_ok_callback),
                            c_tool);
  g_signal_connect_swapped (file_dlg->cancel_button, "clicked",
                            G_CALLBACK (gtk_widget_destroy),
                            file_dlg);

  temp = g_build_filename (gimp_directory (), "curves", ".", NULL);
  gtk_file_selection_set_filename (file_dlg, temp);
  g_free (temp);

  gimp_help_connect (c_tool->file_dialog, tool_manager_help_func, NULL);
}

static void
file_dialog_ok_callback (GimpCurvesTool *c_tool)
{
  FILE        *file = NULL;
  const gchar *filename;

  filename =
    gtk_file_selection_get_filename (GTK_FILE_SELECTION (c_tool->file_dialog));

  file = fopen (filename, c_tool->is_save ? "wt" : "rt");

  if (! file)
    {
      g_message (_("Failed to open file: '%s': %s"),
                 filename, g_strerror (errno));
      return;
    }

  if (c_tool->is_save)
    {
      curves_write_to_file (c_tool, file);
    }
  else if (! curves_read_from_file (c_tool, file))
    {
      g_message (("Error in reading file '%s'."), filename);
    }

  if (file)
    fclose (file);

  gtk_widget_destroy (c_tool->file_dialog);
}

static gboolean
curves_read_from_file (GimpCurvesTool *c_tool,
                       FILE           *file)
{
  gint  i, j;
  gint  fields;
  gchar buf[50];
  gint  index[5][17];
  gint  value[5][17];
  
  if (! fgets (buf, 50, file))
    return FALSE;

  if (strcmp (buf, "# GIMP Curves File\n") != 0)
    return FALSE;

  for (i = 0; i < 5; i++)
    {
      for (j = 0; j < 17; j++)
	{
	  fields = fscanf (file, "%d %d ", &index[i][j], &value[i][j]);
	  if (fields != 2)
	    {
	      g_print ("fields != 2");
	      return FALSE;
	    }
	}
    }

  for (i = 0; i < 5; i++)
    {
      c_tool->curves->curve_type[i] = GIMP_CURVE_SMOOTH;

      for (j = 0; j < 17; j++)
	{
	  c_tool->curves->points[i][j][0] = index[i][j];
	  c_tool->curves->points[i][j][1] = value[i][j];
	}
    }

  for (i = 0; i < 5; i++)
    curves_calculate_curve (c_tool->curves, i);

  curves_update (c_tool, ALL);
  gtk_widget_queue_draw (c_tool->graph);

  gimp_radio_group_set_active (GTK_RADIO_BUTTON (c_tool->curve_type),
			       GINT_TO_POINTER (GIMP_CURVE_SMOOTH));

  gimp_image_map_tool_preview (GIMP_IMAGE_MAP_TOOL (c_tool));

  return TRUE;
}

static void
curves_write_to_file (GimpCurvesTool *c_tool,
                      FILE           *file)
{
  gint   i, j;
  gint32 index;

  for (i = 0; i < 5; i++)
    if (c_tool->curves->curve_type[i] == GIMP_CURVE_FREE)
      {  
	/*  pick representative points from the curve
            and make them control points  */
	for (j = 0; j <= 8; j++)
	  {
	    index = CLAMP0255 (j * 32);
	    c_tool->curves->points[i][j * 2][0] = index;
	    c_tool->curves->points[i][j * 2][1] = c_tool->curves->curve[i][index];
	  }      
      }

  fprintf (file, "# GIMP Curves File\n");

  for (i = 0; i < 5; i++)
    {
      for (j = 0; j < 17; j++)
	fprintf (file, "%d %d ",
                 c_tool->curves->points[i][j][0], 
                 c_tool->curves->points[i][j][1]);
      
      fprintf (file, "\n");
    }
}
