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

#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "tools-types.h"

#ifdef __GNUC__
#warning FIXME #include "gui/gui-types.h"
#endif
#include "gui/gui-types.h"

#include "core/gimpimage.h"
#include "core/gimpdrawable-transform.h"
#include "core/gimpdrawable-transform-utils.h"
#include "core/gimptoolinfo.h"

#include "widgets/gimpviewabledialog.h"

#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"

#include "gui/info-dialog.h"

#include "gimprotatetool.h"
#include "transform_options.h"

#include "libgimp/gimpintl.h"


/*  index into trans_info array  */
#define ANGLE        0
#define REAL_ANGLE   1
#define CENTER_X     2
#define CENTER_Y     3

#define EPSILON      0.018  /*  ~ 1 degree  */
#define FIFTEEN_DEG  (G_PI / 12.0)


/*  local function prototypes  */

static void          gimp_rotate_tool_class_init (GimpRotateToolClass *klass);
static void          gimp_rotate_tool_init       (GimpRotateTool      *rotate_tool);

static void          gimp_rotate_tool_dialog    (GimpTransformTool *tr_tool);
static void          gimp_rotate_tool_prepare   (GimpTransformTool *tr_tool,
						 GimpDisplay       *gdisp);
static void          gimp_rotate_tool_motion    (GimpTransformTool *tr_tool,
						 GimpDisplay       *gdisp);
static void          gimp_rotate_tool_recalc    (GimpTransformTool *tr_tool,
						 GimpDisplay       *gdisp);
static TileManager * gimp_rotate_tool_transform (GimpTransformTool *tr_tool,
						 GimpDisplay       *gdisp);

static void          rotate_info_update         (GimpTransformTool *tr_tool);

static void          rotate_angle_changed       (GtkWidget         *entry,
						 gpointer           data);
static void          rotate_center_changed      (GtkWidget         *entry,
						 gpointer           data);


/*  variables local to this file  */
static gdouble    angle_val;
static gdouble    center_vals[2];

/*  needed for size update  */
static GtkWidget *sizeentry = NULL;

static GimpTransformToolClass *parent_class = NULL;


/*  public functions  */

void 
gimp_rotate_tool_register (GimpToolRegisterCallback  callback,
                           gpointer                  data)
{
  (* callback) (GIMP_TYPE_ROTATE_TOOL,
                transform_options_new,
                FALSE,
                "gimp-rotate-tool",
                _("Rotate"),
                _("Rotate the layer or selection"),
                N_("/Tools/Transform Tools/Rotate"), "<shift>R",
                NULL, "tools/rotate.html",
                GIMP_STOCK_TOOL_ROTATE,
                data);
}

GType
gimp_rotate_tool_get_type (void)
{
  static GType tool_type = 0;

  if (! tool_type)
    {
      static const GTypeInfo tool_info =
      {
        sizeof (GimpRotateToolClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_rotate_tool_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data     */
	sizeof (GimpRotateTool),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_rotate_tool_init,
      };

      tool_type = g_type_register_static (GIMP_TYPE_TRANSFORM_TOOL,
					  "GimpRotateTool", 
                                          &tool_info, 0);
    }

  return tool_type;
}


/*  private functions  */

static void
gimp_rotate_tool_class_init (GimpRotateToolClass *klass)
{
  GimpTransformToolClass *trans_class;

  trans_class = GIMP_TRANSFORM_TOOL_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  trans_class->dialog    = gimp_rotate_tool_dialog;
  trans_class->prepare   = gimp_rotate_tool_prepare;
  trans_class->motion    = gimp_rotate_tool_motion;
  trans_class->recalc    = gimp_rotate_tool_recalc;
  trans_class->transform = gimp_rotate_tool_transform;
}

static void
gimp_rotate_tool_init (GimpRotateTool *rotate_tool)
{
  GimpTool          *tool;
  GimpTransformTool *tr_tool;

  tool    = GIMP_TOOL (rotate_tool);
  tr_tool = GIMP_TRANSFORM_TOOL (rotate_tool);

  gimp_tool_control_set_tool_cursor (tool->control, GIMP_ROTATE_TOOL_CURSOR);

  tr_tool->shell_desc = _("Rotation Information");
}

static void
gimp_rotate_tool_dialog (GimpTransformTool *tr_tool)
{
  GtkWidget *widget;
  GtkWidget *spinbutton2;

  widget = info_dialog_add_spinbutton (tr_tool->info_dialog, _("Angle:"),
                                       &angle_val,
                                       -180, 180, 1, 15, 1, 1, 2,
                                       G_CALLBACK (rotate_angle_changed),
                                       tr_tool);
  gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (widget), TRUE);

  /*  this looks strange (-180, 181), but it works  */
  widget = info_dialog_add_scale (tr_tool->info_dialog, "",
                                  &angle_val,
                                  -180, 181, 0.01, 0.1, 1, -1,
                                  G_CALLBACK (rotate_angle_changed),
                                  tr_tool);
  gtk_widget_set_size_request (widget, 180, -1);

  spinbutton2 = info_dialog_add_spinbutton (tr_tool->info_dialog,
                                            _("Center X:"),
                                            NULL,
                                            -1, 1, 1, 10, 1, 1, 2,
                                            NULL, NULL);
  sizeentry = info_dialog_add_sizeentry (tr_tool->info_dialog, _("Y:"),
                                         center_vals, 1,
                                         GIMP_UNIT_PIXEL, "%a",
                                         TRUE, TRUE, FALSE,
                                         GIMP_SIZE_ENTRY_UPDATE_SIZE,
                                         G_CALLBACK (rotate_center_changed),
                                         tr_tool);

  gimp_size_entry_add_field (GIMP_SIZE_ENTRY (sizeentry),
                             GTK_SPIN_BUTTON (spinbutton2), NULL);

  gtk_table_set_row_spacing (GTK_TABLE (tr_tool->info_dialog->info_table),
                             1, 6);
  gtk_table_set_row_spacing (GTK_TABLE (tr_tool->info_dialog->info_table),
                             2, 0);
}

static void
gimp_rotate_tool_prepare (GimpTransformTool *tr_tool,
                          GimpDisplay       *gdisp)
{
  angle_val      = 0.0;
  center_vals[0] = tr_tool->cx;
  center_vals[1] = tr_tool->cy;

  g_signal_handlers_block_by_func (G_OBJECT (sizeentry), 
                                   rotate_center_changed,
                                   tr_tool);

  gimp_size_entry_set_unit (GIMP_SIZE_ENTRY (sizeentry),
                            gdisp->gimage->unit);

  if (GIMP_DISPLAY_SHELL (gdisp->shell)->dot_for_dot)
    gimp_size_entry_set_unit (GIMP_SIZE_ENTRY (sizeentry), GIMP_UNIT_PIXEL);

  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (sizeentry), 0,
                                  gdisp->gimage->xresolution, FALSE);
  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (sizeentry), 1,
                                  gdisp->gimage->yresolution, FALSE);

  gimp_size_entry_set_refval_boundaries (GIMP_SIZE_ENTRY (sizeentry), 0,
                                         -65536,
                                         65536 + gdisp->gimage->width);
  gimp_size_entry_set_refval_boundaries (GIMP_SIZE_ENTRY (sizeentry), 1,
                                         -65536,
                                         65536 + gdisp->gimage->height);

  gimp_size_entry_set_size (GIMP_SIZE_ENTRY (sizeentry), 0,
                            tr_tool->x1, tr_tool->x2);
  gimp_size_entry_set_size (GIMP_SIZE_ENTRY (sizeentry), 1,
                            tr_tool->y1, tr_tool->y2);

  gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (sizeentry), 0,
                              center_vals[0]);
  gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (sizeentry), 1,
                              center_vals[1]);

  g_signal_handlers_unblock_by_func (G_OBJECT (sizeentry), 
                                     rotate_center_changed,
                                     tr_tool);

  tr_tool->trans_info[ANGLE]      = angle_val;
  tr_tool->trans_info[REAL_ANGLE] = angle_val;
  tr_tool->trans_info[CENTER_X]   = center_vals[0];
  tr_tool->trans_info[CENTER_Y]   = center_vals[1];
}

static void
gimp_rotate_tool_motion (GimpTransformTool *tr_tool,
                         GimpDisplay       *gdisp)
{
  TransformOptions *options;
  gdouble           angle1, angle2, angle;
  gdouble           cx, cy;
  gdouble           x1, y1, x2, y2;

  if (tr_tool->function == TRANSFORM_HANDLE_CENTER)
    {
      tr_tool->trans_info[CENTER_X] = tr_tool->curx;
      tr_tool->trans_info[CENTER_Y] = tr_tool->cury;
      tr_tool->cx                   = tr_tool->curx;
      tr_tool->cy                   = tr_tool->cury;

      return;
    }

  options = (TransformOptions *) GIMP_TOOL (tr_tool)->tool_info->tool_options;

  cx = tr_tool->trans_info[CENTER_X];
  cy = tr_tool->trans_info[CENTER_Y];

  x1 = tr_tool->curx  - cx;
  x2 = tr_tool->lastx - cx;
  y1 = cy - tr_tool->cury;
  y2 = cy - tr_tool->lasty;

  /*  find the first angle  */
  angle1 = atan2 (y1, x1);

  /*  find the angle  */
  angle2 = atan2 (y2, x2);

  angle = angle2 - angle1;

  if (angle > G_PI || angle < -G_PI)
    angle = angle2 - ((angle1 < 0) ? 2.0 * G_PI + angle1 : angle1 - 2.0 * G_PI);

  /*  increment the transform tool's angle  */
  tr_tool->trans_info[REAL_ANGLE] += angle;

  /*  limit the angle to between 0 and 360 degrees  */
  if (tr_tool->trans_info[REAL_ANGLE] < - G_PI)
    tr_tool->trans_info[REAL_ANGLE] =
      2.0 * G_PI - tr_tool->trans_info[REAL_ANGLE];
  else if (tr_tool->trans_info[REAL_ANGLE] > G_PI)
    tr_tool->trans_info[REAL_ANGLE] =
      tr_tool->trans_info[REAL_ANGLE] - 2.0 * G_PI;

  /*  constrain the angle to 15-degree multiples if ctrl is held down  */
  if (options->constrain_1)
    {
      tr_tool->trans_info[ANGLE] =
        FIFTEEN_DEG * (int) ((tr_tool->trans_info[REAL_ANGLE] +
                              FIFTEEN_DEG / 2.0) /
                             FIFTEEN_DEG);
    }
  else
    {
      tr_tool->trans_info[ANGLE] = tr_tool->trans_info[REAL_ANGLE];
    }
}

static void
gimp_rotate_tool_recalc (GimpTransformTool *tr_tool,
                         GimpDisplay       *gdisp)
{
  gdouble cx, cy;

  cx = tr_tool->trans_info[CENTER_X];
  cy = tr_tool->trans_info[CENTER_Y];

  tr_tool->cx = cx;
  tr_tool->cy = cy;

  gimp_drawable_transform_matrix_rotate_center (tr_tool->cx,
                                                tr_tool->cy,
                                                tr_tool->trans_info[ANGLE],
                                                tr_tool->transform);

  /*  transform the bounding box  */
  gimp_transform_tool_transform_bounding_box (tr_tool);

  /*  update the information dialog  */
  rotate_info_update (tr_tool);
}

static TileManager *
gimp_rotate_tool_transform (GimpTransformTool *tr_tool,
			    GimpDisplay       *gdisp)
{
  return gimp_transform_tool_transform_tiles (tr_tool,
                                              _("Rotating..."));
}

static void
rotate_info_update (GimpTransformTool *tr_tool)
{
  angle_val      = gimp_rad_to_deg (tr_tool->trans_info[ANGLE]);
  center_vals[0] = tr_tool->trans_info[CENTER_X];
  center_vals[1] = tr_tool->trans_info[CENTER_Y];

  info_dialog_update (tr_tool->info_dialog);
  info_dialog_popup (tr_tool->info_dialog);
}

static void
rotate_angle_changed (GtkWidget *widget,
		      gpointer   data)
{
  GimpTool          *tool;
  GimpTransformTool *transform_tool;
  gdouble            value;

  tool           = GIMP_TOOL (data);
  transform_tool = GIMP_TRANSFORM_TOOL (data);

  value = gimp_deg_to_rad (GTK_ADJUSTMENT (widget)->value);

#define ANGLE_EPSILON 0.0001

  if (ABS (value - transform_tool->trans_info[ANGLE]) > ANGLE_EPSILON)
    {
      gimp_draw_tool_pause (GIMP_DRAW_TOOL (tool));

      transform_tool->trans_info[ANGLE] = value;

      gimp_rotate_tool_recalc (transform_tool, tool->gdisp);

      gimp_draw_tool_resume (GIMP_DRAW_TOOL (tool));
    }

#undef ANGLE_EPSILON
}

static void
rotate_center_changed (GtkWidget *widget,
		       gpointer   data)
{
  GimpTool          *tool;
  GimpTransformTool *transform_tool;
  gdouble            cx;
  gdouble            cy;

  tool           = GIMP_TOOL (data);
  transform_tool = GIMP_TRANSFORM_TOOL (data);

  cx = gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (widget), 0);
  cy = gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (widget), 1);

  if ((cx != transform_tool->trans_info[CENTER_X]) ||
      (cy != transform_tool->trans_info[CENTER_Y]))
    {
      gimp_draw_tool_pause (GIMP_DRAW_TOOL (tool));

      transform_tool->trans_info[CENTER_X] = cx;
      transform_tool->trans_info[CENTER_Y] = cy;
      transform_tool->cx = cx;
      transform_tool->cy = cy;

      gimp_rotate_tool_recalc (transform_tool, tool->gdisp);

      gimp_draw_tool_resume (GIMP_DRAW_TOOL (tool));
    }
}
