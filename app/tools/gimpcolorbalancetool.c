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

#include "libgimpcolor/gimpcolor.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "tools-types.h"

#include "base/color-balance.h"

#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimpimagemap.h"

#include "display/gimpdisplay.h"

#include "widgets/gimpenummenu.h"

#include "gimpcolorbalancetool.h"
#include "tool_manager.h"

#include "libgimp/gimpintl.h"


#define CYAN_RED       (1 << 0)
#define MAGENTA_GREEN  (1 << 1)
#define YELLOW_BLUE    (1 << 2)
#define PRESERVE       (1 << 3)
#define ALL           (CYAN_RED | MAGENTA_GREEN | YELLOW_BLUE | PRESERVE)


/*  local function prototypes  */

static void   gimp_color_balance_tool_class_init (GimpColorBalanceToolClass *klass);
static void   gimp_color_balance_tool_init       (GimpColorBalanceTool      *cb_tool);

static void   gimp_color_balance_tool_finalize   (GObject          *object);

static void   gimp_color_balance_tool_initialize (GimpTool         *tool,
						  GimpDisplay      *gdisp);

static void   gimp_color_balance_tool_map        (GimpImageMapTool *image_map_tool);
static void   gimp_color_balance_tool_dialog     (GimpImageMapTool *image_map_tool);
static void   gimp_color_balance_tool_reset      (GimpImageMapTool *image_map_tool);

static void   color_balance_update               (GimpColorBalanceTool *cb_tool,
						  gint                  update);
static void   color_balance_range_callback       (GtkWidget            *widget,
						  gpointer              data);
static void   color_balance_range_reset_callback (GtkWidget            *widget,
						  gpointer              data);
static void   color_balance_preserve_update      (GtkWidget            *widget,
						  gpointer              data);
static void   color_balance_cr_adjustment_update (GtkAdjustment        *adj,
						  gpointer              data);
static void   color_balance_mg_adjustment_update (GtkAdjustment        *adj,
						  gpointer              data);
static void   color_balance_yb_adjustment_update (GtkAdjustment        *adj,
						  gpointer              data);


static GimpImageMapToolClass *parent_class = NULL;


/*  functions  */

void
gimp_color_balance_tool_register (GimpToolRegisterCallback  callback,
                                  gpointer                  data)
{
  (* callback) (GIMP_TYPE_COLOR_BALANCE_TOOL,
                NULL,
                FALSE,
                "gimp-color-balance-tool",
                _("Color Balance"),
                _("Adjust color balance"),
                N_("/Layer/Colors/Color Balance..."), NULL,
                NULL, "tools/color_balance.html",
                GIMP_STOCK_TOOL_COLOR_BALANCE,
                data);
}

GType
gimp_color_balance_tool_get_type (void)
{
  static GType tool_type = 0;

  if (! tool_type)
    {
      static const GTypeInfo tool_info =
      {
        sizeof (GimpColorBalanceToolClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_color_balance_tool_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data     */
	sizeof (GimpColorBalanceTool),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_color_balance_tool_init,
      };

      tool_type = g_type_register_static (GIMP_TYPE_IMAGE_MAP_TOOL,
					  "GimpColorBalanceTool", 
                                          &tool_info, 0);
    }

  return tool_type;
}

static void
gimp_color_balance_tool_class_init (GimpColorBalanceToolClass *klass)
{
  GObjectClass          *object_class;
  GimpToolClass         *tool_class;
  GimpImageMapToolClass *image_map_tool_class;

  object_class         = G_OBJECT_CLASS (klass);
  tool_class           = GIMP_TOOL_CLASS (klass);
  image_map_tool_class = GIMP_IMAGE_MAP_TOOL_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize       = gimp_color_balance_tool_finalize;

  tool_class->initialize       = gimp_color_balance_tool_initialize;

  image_map_tool_class->map    = gimp_color_balance_tool_map;
  image_map_tool_class->dialog = gimp_color_balance_tool_dialog;
  image_map_tool_class->reset  = gimp_color_balance_tool_reset;
}

static void
gimp_color_balance_tool_init (GimpColorBalanceTool *cb_tool)
{
  GimpImageMapTool *image_map_tool;

  image_map_tool = GIMP_IMAGE_MAP_TOOL (cb_tool);

  image_map_tool->shell_desc  = _("Adjust Color Balance");

  cb_tool->color_balance      = g_new0 (ColorBalance, 1);
  cb_tool->transfer_mode      = GIMP_MIDTONES;

  color_balance_init (cb_tool->color_balance);
}

static void
gimp_color_balance_tool_finalize (GObject *object)
{
  GimpColorBalanceTool *cb_tool;

  cb_tool = GIMP_COLOR_BALANCE_TOOL (object);

  if (cb_tool->color_balance)
    {
      g_free (cb_tool->color_balance);
      cb_tool->color_balance = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_color_balance_tool_initialize (GimpTool    *tool,
				    GimpDisplay *gdisp)
{
  GimpColorBalanceTool *cb_tool;

  cb_tool = GIMP_COLOR_BALANCE_TOOL (tool);

  if (! gdisp)
    {
      GIMP_TOOL_CLASS (parent_class)->initialize (tool, gdisp);
      return;
    }

  if (! gimp_drawable_is_rgb (gimp_image_active_drawable (gdisp->gimage)))
    {
      g_message (_("Color balance operates only on RGB color drawables."));
      return;
    }

  color_balance_init (cb_tool->color_balance);

  cb_tool->transfer_mode = GIMP_MIDTONES;

  GIMP_TOOL_CLASS (parent_class)->initialize (tool, gdisp);

  color_balance_update (cb_tool, ALL);
}

static void
gimp_color_balance_tool_map (GimpImageMapTool *image_map_tool)
{
  GimpColorBalanceTool *cb_tool;

  cb_tool = GIMP_COLOR_BALANCE_TOOL (image_map_tool);

  color_balance_create_lookup_tables (cb_tool->color_balance);
  gimp_image_map_apply (image_map_tool->image_map,
                        color_balance,
                        cb_tool->color_balance);
}


/**************************/
/*  Color Balance dialog  */
/**************************/

static GtkAdjustment *
create_levels_scale (const gchar   *left,
                     const gchar   *right,
                     GtkWidget     *table,
                     gint           col)
{
  GtkWidget     *label;
  GtkWidget     *slider;
  GtkWidget     *spinbutton;
  GtkAdjustment *adj;

  adj = GTK_ADJUSTMENT (gtk_adjustment_new (0, -100.0, 100.0, 1.0, 10.0, 0.0));

  label = gtk_label_new (left);
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, col, col + 1,
		    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (label);

  slider = gtk_hscale_new (adj);
  gtk_scale_set_draw_value (GTK_SCALE (slider), FALSE);
  gtk_range_set_update_policy (GTK_RANGE (slider), GTK_UPDATE_DELAYED);
  gtk_widget_set_size_request (slider, 100, -1);
  gtk_table_attach_defaults (GTK_TABLE (table), slider, 1, 2, col, col + 1);
  gtk_widget_show (slider);

  label = gtk_label_new (right);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 2, 3, col, col + 1,
		    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (label);

  spinbutton = gtk_spin_button_new (adj, 1.0, 0);
  gtk_table_attach (GTK_TABLE (table), spinbutton, 3, 4, col, col + 1,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (spinbutton);

  return adj;
}

static void
gimp_color_balance_tool_dialog (GimpImageMapTool *image_map_tool)
{
  GimpColorBalanceTool *cb_tool;
  GtkWidget            *vbox;
  GtkWidget            *hbox;
  GtkWidget            *table;
  GtkWidget            *toggle;
  GtkWidget            *button;
  GtkWidget            *frame;

  cb_tool = GIMP_COLOR_BALANCE_TOOL (image_map_tool);

  frame = gimp_enum_radio_frame_new (GIMP_TYPE_TRANSFER_MODE,
                                     gtk_label_new (_("Select Range to Modify")),
                                     2,
                                     G_CALLBACK (color_balance_range_callback),
                                     cb_tool,
                                     &toggle);
  gtk_box_pack_start (GTK_BOX (image_map_tool->main_vbox), frame,
                      FALSE, FALSE, 0);
  gtk_widget_show (frame);

  frame = gtk_frame_new (_("Modify Selected Range's Color Levels"));
  gtk_box_pack_start (GTK_BOX (image_map_tool->main_vbox), frame,
                      FALSE, FALSE, 0);
  gtk_widget_show (frame);

  vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);

  /*  The table containing sliders  */
  table = gtk_table_new (3, 4, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  cb_tool->cyan_red_adj = 
    create_levels_scale (_("Cyan"), _("Red"), table, 0);

  g_signal_connect (G_OBJECT (cb_tool->cyan_red_adj), "value_changed",
                    G_CALLBACK (color_balance_cr_adjustment_update),
                    cb_tool);

  cb_tool->magenta_green_adj = 
    create_levels_scale (_("Magenta"), _("Green"), table, 1);

  g_signal_connect (G_OBJECT (cb_tool->magenta_green_adj), "value_changed",
                    G_CALLBACK (color_balance_mg_adjustment_update),
                    cb_tool);

  cb_tool->yellow_blue_adj = 
    create_levels_scale (_("Yellow"), _("Blue"), table, 2);

  g_signal_connect (G_OBJECT (cb_tool->yellow_blue_adj), "value_changed",
                    G_CALLBACK (color_balance_yb_adjustment_update),
                    cb_tool);

  hbox = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  button = gtk_button_new_with_mnemonic (_("R_eset Range"));
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (color_balance_range_reset_callback),
                    cb_tool);

  cb_tool->preserve_toggle =
    gtk_check_button_new_with_mnemonic (_("Preserve _Luminosity"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb_tool->preserve_toggle),
				cb_tool->color_balance->preserve_luminosity);
  gtk_box_pack_end (GTK_BOX (image_map_tool->main_vbox),
                    cb_tool->preserve_toggle, FALSE, FALSE, 0);
  gtk_widget_show (cb_tool->preserve_toggle);

  g_signal_connect (G_OBJECT (cb_tool->preserve_toggle), "toggled",
                    G_CALLBACK (color_balance_preserve_update),
                    cb_tool);

  /*  set range after everything is in place  */
  gimp_radio_group_set_active (GTK_RADIO_BUTTON (toggle),
                               GINT_TO_POINTER (cb_tool->transfer_mode));
}

static void
gimp_color_balance_tool_reset (GimpImageMapTool *image_map_tool)
{
  GimpColorBalanceTool *cb_tool;

  cb_tool = GIMP_COLOR_BALANCE_TOOL (image_map_tool);

  color_balance_init (cb_tool->color_balance);
  color_balance_update (cb_tool, ALL);
}

static void
color_balance_update (GimpColorBalanceTool *cb_tool,
		      gint                  update)
{
  GimpTransferMode tm;

  tm = cb_tool->transfer_mode;

  if (update & CYAN_RED)
    gtk_adjustment_set_value (cb_tool->cyan_red_adj,
                              cb_tool->color_balance->cyan_red[tm]);

  if (update & MAGENTA_GREEN)
    gtk_adjustment_set_value (cb_tool->magenta_green_adj,
                              cb_tool->color_balance->magenta_green[tm]);

  if (update & YELLOW_BLUE)
    gtk_adjustment_set_value (cb_tool->yellow_blue_adj,
                              cb_tool->color_balance->yellow_blue[tm]);

  if (update & PRESERVE)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb_tool->preserve_toggle),
                                  cb_tool->color_balance->preserve_luminosity);
}

static void
color_balance_range_callback (GtkWidget *widget,
			      gpointer   data)
{
  GimpColorBalanceTool *cb_tool;

  cb_tool = GIMP_COLOR_BALANCE_TOOL (data);

  gimp_radio_button_update (widget, &cb_tool->transfer_mode);
  color_balance_update (cb_tool, ALL);
}

static void
color_balance_range_reset_callback (GtkWidget *widget,
                                    gpointer   data)
{
  GimpColorBalanceTool *cb_tool;

  cb_tool = GIMP_COLOR_BALANCE_TOOL (data);

  color_balance_range_reset (cb_tool->color_balance,
                             cb_tool->transfer_mode);
  color_balance_update (cb_tool, ALL);

  gimp_image_map_tool_preview (GIMP_IMAGE_MAP_TOOL (cb_tool));
}

static void
color_balance_preserve_update (GtkWidget *widget,
			       gpointer   data)
{
  GimpColorBalanceTool *cb_tool;
  gboolean              active;

  cb_tool = GIMP_COLOR_BALANCE_TOOL (data);

  active = GTK_TOGGLE_BUTTON (widget)->active;

  if (cb_tool->color_balance->preserve_luminosity != active)
    {
      cb_tool->color_balance->preserve_luminosity = active;

      gimp_image_map_tool_preview (GIMP_IMAGE_MAP_TOOL (cb_tool));
    }
}

static void
color_balance_cr_adjustment_update (GtkAdjustment *adjustment,
				    gpointer       data)
{
  GimpColorBalanceTool *cb_tool;
  GimpTransferMode      tm;

  cb_tool = GIMP_COLOR_BALANCE_TOOL (data);

  tm = cb_tool->transfer_mode;

  if (cb_tool->color_balance->cyan_red[tm] != adjustment->value)
    {
      cb_tool->color_balance->cyan_red[tm] = adjustment->value;

      gimp_image_map_tool_preview (GIMP_IMAGE_MAP_TOOL (cb_tool));
    }
}

static void
color_balance_mg_adjustment_update (GtkAdjustment *adjustment,
				    gpointer       data)
{
  GimpColorBalanceTool *cb_tool;
  GimpTransferMode      tm;

  cb_tool = GIMP_COLOR_BALANCE_TOOL (data);

  tm = cb_tool->transfer_mode;

  if (cb_tool->color_balance->magenta_green[tm] != adjustment->value)
    {
      cb_tool->color_balance->magenta_green[tm] = adjustment->value;

      gimp_image_map_tool_preview (GIMP_IMAGE_MAP_TOOL (cb_tool));
    }
}

static void
color_balance_yb_adjustment_update (GtkAdjustment *adjustment,
				    gpointer       data)
{
  GimpColorBalanceTool *cb_tool;
  GimpTransferMode      tm;

  cb_tool = GIMP_COLOR_BALANCE_TOOL (data);

  tm = cb_tool->transfer_mode;

  if (cb_tool->color_balance->yellow_blue[tm] != adjustment->value)
    {
      cb_tool->color_balance->yellow_blue[tm] = adjustment->value;

      gimp_image_map_tool_preview (GIMP_IMAGE_MAP_TOOL (cb_tool));
    }
}
