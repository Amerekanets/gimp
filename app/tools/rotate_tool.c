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

#include "apptypes.h"

#include "appenv.h"
#include "draw_core.h"
#include "drawable.h"
#include "gdisplay.h"
#include "gimage_mask.h"
#include "gimpimage.h"
#include "gimpprogress.h"
#include "gimpui.h"
#include "info_dialog.h"
#include "rotate_tool.h"
#include "selection.h"
#include "tile_manager_pvt.h"
#include "tools.h"
#include "tool_options.h"
#include "transform_core.h"
#include "transform_tool.h"
#include "undo.h"

#include "libgimp/gimpsizeentry.h"
#include "libgimp/gimpmath.h"

#include "libgimp/gimpintl.h"


/*  index into trans_info array  */
#define ANGLE        0
#define REAL_ANGLE   1
#define CENTER_X     2
#define CENTER_Y     3

#define EPSILON      0.018  /*  ~ 1 degree  */
#define FIFTEEN_DEG  (G_PI / 12.0)

/*  variables local to this file  */
static gdouble    angle_val;
static gdouble    center_vals[2];

/*  needed for size update  */
static GtkWidget *sizeentry;

/*  forward function declarations  */
static void   rotate_tool_recalc  (Tool *, void *);
static void   rotate_tool_motion  (Tool *, void *);
static void   rotate_info_update  (Tool *);

/*  callback functions for the info dialog sizeentries  */
static void   rotate_angle_changed  (GtkWidget *entry, gpointer data);
static void   rotate_center_changed (GtkWidget *entry, gpointer data);

TileManager *
rotate_tool_transform (Tool           *tool,
		       gpointer        gdisp_ptr,
		       TransformState  state)
{
  TransformCore *transform_core;
  GDisplay      *gdisp;
  GtkWidget     *widget;
  GtkWidget     *spinbutton2;

  transform_core = (TransformCore *) tool->private;
  gdisp = (GDisplay *) gdisp_ptr;

  switch (state)
    {
    case TRANSFORM_INIT:
      angle_val = 0.0;
      center_vals[0] = transform_core->cx;
      center_vals[1] = transform_core->cy;

      if (!transform_info)
	{
	  transform_info = info_dialog_new (_("Rotation Information"),
					    gimp_standard_help_func,
					    "tools/transform_rotate.html");

	  widget =
	    info_dialog_add_spinbutton (transform_info, _("Angle:"),
					&angle_val,
					-180, 180, 1, 15, 1, 1, 2,
					(GtkSignalFunc) rotate_angle_changed,
					tool);
	  gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (widget), TRUE);

	  /*  this looks strange (-180, 181), but it works  */
	  widget = info_dialog_add_scale (transform_info, "", &angle_val,
					  -180, 181, 0.01, 0.1, 1, -1,
					  (GtkSignalFunc) rotate_angle_changed,
					  tool);
	  gtk_widget_set_usize (widget, 180, 0);

	  spinbutton2 =
	    info_dialog_add_spinbutton (transform_info, _("Center X:"), NULL,
					-1, 1, 1, 10, 1, 1, 2, NULL, NULL);
	  sizeentry =
	    info_dialog_add_sizeentry (transform_info, _("Y:"),
				       center_vals, 1,
				       gdisp->gimage->unit, "%a",
				       TRUE, TRUE, FALSE,
				       GIMP_SIZE_ENTRY_UPDATE_SIZE,
				       rotate_center_changed, tool);

	  gimp_size_entry_add_field (GIMP_SIZE_ENTRY (sizeentry),
				     GTK_SPIN_BUTTON (spinbutton2), NULL);

	  gtk_table_set_row_spacing (GTK_TABLE (transform_info->info_table),
				     1, 6);
	  gtk_table_set_row_spacing (GTK_TABLE (transform_info->info_table),
				     2, 0);
	}

      gtk_signal_handler_block_by_data (GTK_OBJECT (sizeentry), tool);

      gimp_size_entry_set_unit (GIMP_SIZE_ENTRY (sizeentry),
				gdisp->gimage->unit);
      if (gdisp->dot_for_dot)
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
				transform_core->x1, transform_core->x2);
      gimp_size_entry_set_size (GIMP_SIZE_ENTRY (sizeentry), 1,
				transform_core->y1, transform_core->y2);

      gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (sizeentry), 0,
				  center_vals[0]);
      gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (sizeentry), 1,
				  center_vals[1]);

      gtk_widget_set_sensitive (transform_info->shell, TRUE);

      gtk_signal_handler_unblock_by_data (GTK_OBJECT (sizeentry), tool);

      transform_core->trans_info[ANGLE] = angle_val;
      transform_core->trans_info[REAL_ANGLE] = angle_val;
      transform_core->trans_info[CENTER_X] = center_vals[0];
      transform_core->trans_info[CENTER_Y] = center_vals[1];

      return NULL;
      break;

    case TRANSFORM_MOTION:
      rotate_tool_motion (tool, gdisp_ptr);
      rotate_tool_recalc (tool, gdisp_ptr);
      break;

    case TRANSFORM_RECALC:
      rotate_tool_recalc (tool, gdisp_ptr);
      break;

    case TRANSFORM_FINISH:
      gtk_widget_set_sensitive (GTK_WIDGET (transform_info->shell), FALSE);
      return rotate_tool_rotate (gdisp->gimage,
				 gimp_image_active_drawable (gdisp->gimage),
				 gdisp,
				 transform_core->trans_info[ANGLE],
				 transform_core->original,
				 transform_tool_smoothing (),
				 transform_core->transform);
      break;
    }

  return NULL;
}

Tool *
tools_new_rotate_tool (void)
{
  Tool          *tool;
  TransformCore *private;

  tool = transform_core_new (ROTATE, TRUE);

  private = tool->private;

  /*  set the rotation specific transformation attributes  */
  private->trans_func = rotate_tool_transform;
  private->trans_info[ANGLE]      = 0.0;
  private->trans_info[REAL_ANGLE] = 0.0;
  private->trans_info[CENTER_X]   = 0.0;
  private->trans_info[CENTER_Y]   = 0.0;

  /*  assemble the transformation matrix  */
  gimp_matrix3_identity (private->transform);

  return tool;
}

void
tools_free_rotate_tool (Tool *tool)
{
  transform_core_free (tool);
}

static void
rotate_info_update (Tool *tool)
{
  TransformCore *transform_core;

  transform_core = (TransformCore *) tool->private;

  angle_val      = gimp_rad_to_deg (transform_core->trans_info[ANGLE]);
  center_vals[0] = transform_core->cx;
  center_vals[1] = transform_core->cy;

  info_dialog_update (transform_info);
  info_dialog_popup (transform_info);
}

static void
rotate_angle_changed (GtkWidget *widget,
		      gpointer   data)
{
  Tool          *tool;
  GDisplay      *gdisp;
  TransformCore *transform_core;
  gdouble        value;

  tool = (Tool *) data;

  if (tool)
    {
      gdisp = (GDisplay *) tool->gdisp_ptr;
      transform_core = (TransformCore *) tool->private;

      value = gimp_deg_to_rad (GTK_ADJUSTMENT (widget)->value);

      if (value != transform_core->trans_info[ANGLE])
	{
	  draw_core_pause (transform_core->core, tool);      
	  transform_core->trans_info[ANGLE] = value;
	  rotate_tool_recalc (tool, gdisp);
	  draw_core_resume (transform_core->core, tool);
	}
    }
}

static void
rotate_center_changed (GtkWidget *widget,
		       gpointer   data)
{
  Tool          *tool;
  GDisplay      *gdisp;
  TransformCore *transform_core;
  gint           cx;
  gint           cy;

  tool = (Tool *) data;

  if (tool)
    {
      gdisp = (GDisplay *) tool->gdisp_ptr;
      transform_core = (TransformCore *) tool->private;

      cx = RINT (gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (widget), 0));
      cy = RINT (gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (widget), 1));

      if ((cx != transform_core->cx) ||
	  (cy != transform_core->cy))
	{
	  draw_core_pause (transform_core->core, tool);      
	  transform_core->cx = cx;
	  transform_core->cy = cy;
	  rotate_tool_recalc (tool, gdisp);
	  draw_core_resume (transform_core->core, tool);
	}
    }
}

static void
rotate_tool_motion (Tool *tool,
		    void *gdisp_ptr)
{
  TransformCore *transform_core;
  gdouble        angle1, angle2, angle;
  gdouble        cx, cy;
  gdouble        x1, y1, x2, y2;

  transform_core = (TransformCore *) tool->private;

  if (transform_core->function == TRANSFORM_HANDLE_CENTER)
    {
      transform_core->cx = transform_core->curx;
      transform_core->cy = transform_core->cury;

      return;
    }

  cx = transform_core->cx;
  cy = transform_core->cy;

  x1 = transform_core->curx - cx;
  x2 = transform_core->lastx - cx;
  y1 = cy - transform_core->cury;
  y2 = cy - transform_core->lasty;

  /*  find the first angle  */
  angle1 = atan2 (y1, x1);

  /*  find the angle  */
  angle2 = atan2 (y2, x2);

  angle = angle2 - angle1;

  if (angle > G_PI || angle < -G_PI)
    angle = angle2 - ((angle1 < 0) ? 2.0 * G_PI + angle1 : angle1 - 2.0 * G_PI);

  /*  increment the transform tool's angle  */
  transform_core->trans_info[REAL_ANGLE] += angle;

  /*  limit the angle to between 0 and 360 degrees  */
  if (transform_core->trans_info[REAL_ANGLE] < - G_PI)
    transform_core->trans_info[REAL_ANGLE] =
      2.0 * G_PI - transform_core->trans_info[REAL_ANGLE];
  else if (transform_core->trans_info[REAL_ANGLE] > G_PI)
    transform_core->trans_info[REAL_ANGLE] =
      transform_core->trans_info[REAL_ANGLE] - 2.0 * G_PI;

  /*  constrain the angle to 15-degree multiples if ctrl is held down  */
  if (transform_core->state & GDK_CONTROL_MASK)
    transform_core->trans_info[ANGLE] =
      FIFTEEN_DEG * (int) ((transform_core->trans_info[REAL_ANGLE] +
			    FIFTEEN_DEG / 2.0) /
			   FIFTEEN_DEG);
  else
    transform_core->trans_info[ANGLE] = transform_core->trans_info[REAL_ANGLE];
}

static void
rotate_tool_recalc (Tool *tool,
		    void *gdisp_ptr)
{
  TransformCore *transform_core;
  GDisplay      *gdisp;
  gdouble        cx, cy;

  gdisp = (GDisplay *) tool->gdisp_ptr;
  transform_core = (TransformCore *) tool->private;

  cx = transform_core->cx;
  cy = transform_core->cy;

  /*  assemble the transformation matrix  */
  gimp_matrix3_identity  (transform_core->transform);
  gimp_matrix3_translate (transform_core->transform, -cx, -cy);
  gimp_matrix3_rotate    (transform_core->transform,
			  transform_core->trans_info[ANGLE]);
  gimp_matrix3_translate (transform_core->transform, +cx, +cy);

  /*  transform the bounding box  */
  transform_core_transform_bounding_box (tool);

  /*  update the information dialog  */
  rotate_info_update (tool);
}

TileManager *
rotate_tool_rotate (GImage       *gimage,
		    GimpDrawable *drawable,
		    GDisplay     *gdisp,
		    gdouble       angle,
		    TileManager  *float_tiles,
		    gboolean      interpolation,
		    GimpMatrix3   matrix)
{
  gimp_progress *progress;
  TileManager   *ret;

  progress = progress_start (gdisp, _("Rotating..."), FALSE, NULL, NULL);

  ret = transform_core_do (gimage, drawable, float_tiles,
			   interpolation, matrix,
			   progress ? progress_update_and_flush :
			   (progress_func_t) NULL,
			   progress);

  if (progress)
    progress_end (progress);

  return ret;
}
