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

#include "display/gimpdisplay.h"

#include "gui/info-dialog.h"

#include "gimpperspectivetool.h"
#include "gimptransformoptions.h"

#include "gimp-intl.h"


/*  local function prototypes  */

static void   gimp_perspective_tool_class_init (GimpPerspectiveToolClass *klass);
static void   gimp_perspective_tool_init       (GimpPerspectiveTool      *tool);

static void   gimp_perspective_tool_dialog     (GimpTransformTool *tr_tool);
static void   gimp_perspective_tool_prepare    (GimpTransformTool *tr_tool,
                                                GimpDisplay       *gdisp);
static void   gimp_perspective_tool_motion     (GimpTransformTool *tr_tool,
                                                GimpDisplay       *gdisp);
static void   gimp_perspective_tool_recalc     (GimpTransformTool *tr_tool,
                                                GimpDisplay       *gdisp);

static void   perspective_info_update          (GimpTransformTool *tr_tool);


/*  storage for information dialog fields  */
static gchar  matrix_row_buf [3][MAX_INFO_BUF];

static GimpTransformToolClass *parent_class = NULL;


/*  public functions  */

void 
gimp_perspective_tool_register (GimpToolRegisterCallback  callback,
                                gpointer                  data)
{
  (* callback) (GIMP_TYPE_PERSPECTIVE_TOOL,
                GIMP_TYPE_TRANSFORM_OPTIONS,
                gimp_transform_options_gui,
                FALSE,
                "gimp-perspective-tool",
                _("Perspective"),
                _("Change perspective of the layer or selection"),
                N_("/Tools/Transform Tools/Perspective"), "<shift>P",
                NULL, "tools/perspective.html",
                GIMP_STOCK_TOOL_PERSPECTIVE,
                data);
}

GType
gimp_perspective_tool_get_type (void)
{
  static GType tool_type = 0;

  if (! tool_type)
    {
      static const GTypeInfo tool_info =
      {
        sizeof (GimpPerspectiveToolClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_perspective_tool_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data     */
	sizeof (GimpPerspectiveTool),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_perspective_tool_init,
      };

      tool_type = g_type_register_static (GIMP_TYPE_TRANSFORM_TOOL,
					  "GimpPerspectiveTool", 
                                          &tool_info, 0);
    }

  return tool_type;
}

static void
gimp_perspective_tool_class_init (GimpPerspectiveToolClass *klass)
{
  GimpTransformToolClass *trans_class;

  trans_class = GIMP_TRANSFORM_TOOL_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  trans_class->dialog  = gimp_perspective_tool_dialog;
  trans_class->prepare = gimp_perspective_tool_prepare;
  trans_class->motion  = gimp_perspective_tool_motion;
  trans_class->recalc  = gimp_perspective_tool_recalc;
}

static void
gimp_perspective_tool_init (GimpPerspectiveTool *perspective_tool)
{
  GimpTool          *tool;
  GimpTransformTool *tr_tool;

  tool    = GIMP_TOOL (perspective_tool);
  tr_tool = GIMP_TRANSFORM_TOOL (perspective_tool);

  gimp_tool_control_set_tool_cursor (tool->control,
                                     GIMP_PERSPECTIVE_TOOL_CURSOR);

  tr_tool->shell_desc    = _("Perspective Transform Information");
  tr_tool->progress_text = _("Perspective...");
}

static void
gimp_perspective_tool_dialog (GimpTransformTool *tr_tool)
{
  info_dialog_add_label (tr_tool->info_dialog, _("Matrix:"),
                         matrix_row_buf[0]);
  info_dialog_add_label (tr_tool->info_dialog, "",
                         matrix_row_buf[1]);
  info_dialog_add_label (tr_tool->info_dialog, "",
                         matrix_row_buf[2]);
}

static void
gimp_perspective_tool_prepare (GimpTransformTool  *tr_tool,
                               GimpDisplay        *gdisp)
{
  tr_tool->trans_info[X0] = (gdouble) tr_tool->x1;
  tr_tool->trans_info[Y0] = (gdouble) tr_tool->y1;
  tr_tool->trans_info[X1] = (gdouble) tr_tool->x2;
  tr_tool->trans_info[Y1] = (gdouble) tr_tool->y1;
  tr_tool->trans_info[X2] = (gdouble) tr_tool->x1;
  tr_tool->trans_info[Y2] = (gdouble) tr_tool->y2;
  tr_tool->trans_info[X3] = (gdouble) tr_tool->x2;
  tr_tool->trans_info[Y3] = (gdouble) tr_tool->y2;
}

static void
gimp_perspective_tool_motion (GimpTransformTool *transform_tool,
                              GimpDisplay       *gdisp)
{
  gdouble diff_x, diff_y;

  diff_x = transform_tool->curx - transform_tool->lastx;
  diff_y = transform_tool->cury - transform_tool->lasty;

  switch (transform_tool->function)
    {
    case TRANSFORM_HANDLE_1:
      transform_tool->trans_info[X0] += diff_x;
      transform_tool->trans_info[Y0] += diff_y;
      break;
    case TRANSFORM_HANDLE_2:
      transform_tool->trans_info[X1] += diff_x;
      transform_tool->trans_info[Y1] += diff_y;
      break;
    case TRANSFORM_HANDLE_3:
      transform_tool->trans_info[X2] += diff_x;
      transform_tool->trans_info[Y2] += diff_y;
      break;
    case TRANSFORM_HANDLE_4:
      transform_tool->trans_info[X3] += diff_x;
      transform_tool->trans_info[Y3] += diff_y;
      break;
    case TRANSFORM_HANDLE_CENTER:
      transform_tool->trans_info[X0] += diff_x;
      transform_tool->trans_info[Y0] += diff_y;
      transform_tool->trans_info[X1] += diff_x;
      transform_tool->trans_info[Y1] += diff_y;
      transform_tool->trans_info[X2] += diff_x;
      transform_tool->trans_info[Y2] += diff_y;
      transform_tool->trans_info[X3] += diff_x;
      transform_tool->trans_info[Y3] += diff_y;
      break;
    default:
      break;
    }
}

static void
gimp_perspective_tool_recalc (GimpTransformTool *tr_tool,
                              GimpDisplay       *gdisp)
{
  gimp_drawable_transform_matrix_perspective (tr_tool->x1,
                                              tr_tool->y1,
                                              tr_tool->x2,
                                              tr_tool->y2,
                                              tr_tool->trans_info[X0],
                                              tr_tool->trans_info[Y0],
                                              tr_tool->trans_info[X1],
                                              tr_tool->trans_info[Y1],
                                              tr_tool->trans_info[X2],
                                              tr_tool->trans_info[Y2],
                                              tr_tool->trans_info[X3],
                                              tr_tool->trans_info[Y3],
                                              tr_tool->transform);

  /*  transform the bounding box  */
  gimp_transform_tool_transform_bounding_box (tr_tool);

  /*  update the information dialog  */
  perspective_info_update (tr_tool);
}

static void
perspective_info_update (GimpTransformTool *tr_tool)
{
  gint i;
  
  for (i = 0; i < 3; i++)
    {
      gchar *p = matrix_row_buf[i];
      gint   j;

      for (j = 0; j < 3; j++)
	{
	  p += g_snprintf (p, MAX_INFO_BUF - (p - matrix_row_buf[i]),
			   "%10.3g", tr_tool->transform[i][j]);
	}
    }

  info_dialog_update (tr_tool->info_dialog);
  info_dialog_popup (tr_tool->info_dialog);
}
