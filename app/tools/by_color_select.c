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
#include <gdk/gdkkeysyms.h>

#include "libgimpcolor/gimpcolor.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "apptypes.h"

#include "boundary.h"
#include "context_manager.h"
#include "cursorutil.h"
#include "drawable.h"
#include "draw_core.h"
#include "gimage_mask.h"
#include "gimpchannel.h"
#include "gimpcontainer.h"
#include "gimpdnd.h"
#include "gimpimage.h"
#include "gimprc.h"
#include "gimpui.h"
#include "gdisplay.h"
#include "paint_funcs.h"
#include "pixel_region.h"
#include "temp_buf.h"
#include "tile.h"
#include "tile_manager.h"

#include "by_color_select.h"
#include "selection_options.h"
#include "tool_options.h"
#include "tools.h"

#include "libgimp/gimpintl.h"


#define PREVIEW_WIDTH       256
#define PREVIEW_HEIGHT      256
#define PREVIEW_EVENT_MASK  GDK_EXPOSURE_MASK | \
                            GDK_BUTTON_PRESS_MASK | \
                            GDK_ENTER_NOTIFY_MASK

/*  the by color selection structures  */

typedef struct _ByColorSelect ByColorSelect;

struct _ByColorSelect
{
  gint       x, y;       /*  Point from which to execute seed fill  */
  SelectOps  operation;  /*  add, subtract, normal color selection  */
};

typedef struct _ByColorDialog ByColorDialog;

struct _ByColorDialog
{
  GtkWidget *shell;

  GtkWidget *preview;
  GtkWidget *gimage_name;

  GtkWidget *replace_button;
  GtkObject *threshold_adj;

  gint       threshold;  /*  threshold value for color select             */
  gint       operation;  /*  Add, Subtract, Replace                       */
  GImage    *gimage;     /*  gimage which is currently under examination  */
};


/*  the by color selection tool options  */
static SelectionOptions * by_color_options = NULL;

/*  the by color selection dialog  */
static ByColorDialog    * by_color_dialog = NULL;

/*  dnd stuff  */
static GtkTargetEntry by_color_select_targets[] =
{
  GIMP_TARGET_COLOR
};
static guint n_by_color_select_targets = (sizeof (by_color_select_targets) /
					  sizeof (by_color_select_targets[0]));


static void   by_color_select_color_drop      (GtkWidget      *widget,
					       const GimpRGB  *color,
					       gpointer        data);

/*  by_color select action functions  */

static void   by_color_select_button_press    (Tool           *tool,
					       GdkEventButton *bevent,
					       GDisplay       *gdisp);
static void   by_color_select_button_release  (Tool           *tool,
					       GdkEventButton *bevent,
					       GDisplay       *gdisp);
static void   by_color_select_modifier_update (Tool           *tool,
					       GdkEventKey    *kevent,
					       GDisplay       *gdisp);
static void   by_color_select_cursor_update   (Tool           *tool,
					       GdkEventMotion *mevent,
					       GDisplay       *gdisp);
static void   by_color_select_oper_update     (Tool           *tool,
					       GdkEventMotion *mevent,
					       GDisplay       *gdisp);
static void   by_color_select_control         (Tool           *tool,
					       ToolAction      action,
					       GDisplay       *gdisp);

static ByColorDialog * by_color_select_dialog_new  (void);

static void   by_color_select_render               (ByColorDialog  *,
						    GImage         *);
static void   by_color_select_draw                 (ByColorDialog  *,
						    GImage         *);
static gint   by_color_select_preview_events       (GtkWidget      *,
						    GdkEventButton *,
						    ByColorDialog  *);
static void   by_color_select_invert_callback      (GtkWidget      *,
						    gpointer        );
static void   by_color_select_select_all_callback  (GtkWidget      *,
						    gpointer        );
static void   by_color_select_select_none_callback (GtkWidget      *,
						    gpointer        );
static void   by_color_select_reset_callback       (GtkWidget      *,
						    gpointer        );
static void   by_color_select_close_callback       (GtkWidget      *,
						    gpointer        );
static void   by_color_select_preview_button_press (ByColorDialog  *,
						    GdkEventButton *);

static gint   is_pixel_sufficiently_different      (guchar         *,
						    guchar         *,
						    gint            ,
						    gint            ,
						    gint            ,
						    gint            );
static GimpChannel * by_color_select_color         (GImage         *,
						    GimpDrawable   *,
						    guchar         *,
						    gint            ,
						    gint            ,
						    gint            );


/*  by_color selection machinery  */

static gint
is_pixel_sufficiently_different (guchar *col1,
				 guchar *col2,
				 gint    antialias,
				 gint    threshold,
				 gint    bytes,
				 gint    has_alpha)
{
  gint diff;
  gint max;
  gint b;
  gint alpha;

  max = 0;
  alpha = (has_alpha) ? bytes - 1 : bytes;

  /*  if there is an alpha channel, never select transparent regions  */
  if (has_alpha && col2[alpha] == 0)
    return 0;

  for (b = 0; b < alpha; b++)
    {
      diff = col1[b] - col2[b];
      diff = abs (diff);
      if (diff > max)
	max = diff;
    }

  if (antialias && threshold > 0)
    {
      gfloat aa;

      aa = 1.5 - ((gfloat) max / threshold);
      if (aa <= 0)
	return 0;
      else if (aa < 0.5)
	return (guchar) (aa * 512);
      else
	return 255;
    }
  else
    {
      if (max > threshold)
	return 0;
      else
	return 255;
    }
}

static GimpChannel *
by_color_select_color (GImage       *gimage,
		       GimpDrawable *drawable,
		       guchar       *color,
		       gboolean      antialias,
		       gint          threshold,
		       gboolean      sample_merged)
{
  /*  Scan over the gimage's active layer, finding pixels within the specified
   *  threshold from the given R, G, & B values.  If antialiasing is on,
   *  use the same antialiasing scheme as in fuzzy_select.  Modify the gimage's
   *  mask to reflect the additional selection
   */
  GimpChannel *mask;
  PixelRegion  imagePR, maskPR;
  guchar      *image_data;
  guchar      *mask_data;
  guchar      *idata, *mdata;
  guchar       rgb[MAX_CHANNELS];
  gint         has_alpha, indexed;
  gint         width, height;
  gint         bytes, color_bytes, alpha;
  gint         i, j;
  gpointer     pr;
  gint         d_type;

  /*  Get the image information  */
  if (sample_merged)
    {
      bytes  = gimp_image_composite_bytes (gimage);
      d_type = gimp_image_composite_type (gimage);
      has_alpha = (d_type == RGBA_GIMAGE ||
		   d_type == GRAYA_GIMAGE ||
		   d_type == INDEXEDA_GIMAGE);
      indexed = d_type == INDEXEDA_GIMAGE || d_type == INDEXED_GIMAGE;
      width = gimage->width;
      height = gimage->height;
      pixel_region_init (&imagePR, gimp_image_composite (gimage),
			 0, 0, width, height, FALSE);
    }
  else
    {
      bytes     = gimp_drawable_bytes (drawable);
      d_type    = gimp_drawable_type (drawable);
      has_alpha = gimp_drawable_has_alpha (drawable);
      indexed   = gimp_drawable_is_indexed (drawable);
      width     = gimp_drawable_width (drawable);
      height    = gimp_drawable_height (drawable);

      pixel_region_init (&imagePR, gimp_drawable_data (drawable),
			 0, 0, width, height, FALSE);
    }

  if (indexed)
    {
      /* indexed colors are always RGB or RGBA */
      color_bytes = has_alpha ? 4 : 3;
    }
  else
    {
      /* RGB, RGBA, GRAY and GRAYA colors are shaped just like the image */
      color_bytes = bytes;
    }

  alpha = bytes - 1;
  mask = gimp_channel_new_mask (gimage, width, height);
  pixel_region_init (&maskPR, gimp_drawable_data (GIMP_DRAWABLE (mask)), 
		     0, 0, width, height, TRUE);

  /*  iterate over the entire image  */
  for (pr = pixel_regions_register (2, &imagePR, &maskPR);
       pr != NULL;
       pr = pixel_regions_process (pr))
    {
      image_data = imagePR.data;
      mask_data = maskPR.data;

      for (i = 0; i < imagePR.h; i++)
	{
	  idata = image_data;
	  mdata = mask_data;
	  for (j = 0; j < imagePR.w; j++)
	    {
	      /*  Get the rgb values for the color  */
	      gimp_image_get_color (gimage, d_type, rgb, idata);

	      /*  Plug the alpha channel in there  */
	      if (has_alpha)
		rgb[color_bytes - 1] = idata[alpha];

	      /*  Find how closely the colors match  */
	      *mdata++ = is_pixel_sufficiently_different (color,
							  rgb,
							  antialias,
							  threshold,
							  color_bytes,
							  has_alpha);

	      idata += bytes;
	    }

	  image_data += imagePR.rowstride;
	  mask_data += maskPR.rowstride;
	}
    }

  return mask;
}

void
by_color_select (GImage       *gimage,
		 GimpDrawable *drawable,
		 guchar       *color,
		 gint          threshold,
		 SelectOps     op,
		 gboolean      antialias,
		 gboolean      feather,
		 gdouble       feather_radius,
		 gboolean      sample_merged)
{
  GimpChannel *new_mask;
  gint         off_x, off_y;

  if (!drawable) 
    return;

  new_mask = by_color_select_color (gimage, drawable, color,
				    antialias, threshold, sample_merged);

  /*  if applicable, replace the current selection  */
  if (op == SELECTION_REPLACE)
    gimage_mask_clear (gimage);
  else
    gimage_mask_undo (gimage);

  if (sample_merged)
    {
      off_x = 0; off_y = 0;
    }
  else
    {
      gimp_drawable_offsets (drawable, &off_x, &off_y);
    }

  if (feather)
    gimp_channel_feather (new_mask, gimp_image_get_mask (gimage),
			  feather_radius,
			  feather_radius,
			  op, off_x, off_y);
  else
    gimp_channel_combine_mask (gimp_image_get_mask (gimage),
			       new_mask, op, off_x, off_y);

  gtk_object_unref (GTK_OBJECT (new_mask));
}

/*  by_color select action functions  */

static void
by_color_select_button_press (Tool           *tool,
			      GdkEventButton *bevent,
			      GDisplay       *gdisp)
{
  ByColorSelect *by_color_sel;

  by_color_sel = (ByColorSelect *) tool->private;

  tool->drawable = gimp_image_active_drawable (gdisp->gimage);

  if (!by_color_dialog)
    return;

  by_color_sel->x = bevent->x;
  by_color_sel->y = bevent->y;

  tool->state = ACTIVE;
  tool->gdisp = gdisp;

  /*  Make sure the "by color" select dialog is visible  */
  if (! GTK_WIDGET_VISIBLE (by_color_dialog->shell))
    gtk_widget_show (by_color_dialog->shell);

  /*  Update the by_color_dialog's active gdisp pointer  */
  if (by_color_dialog->gimage)
    by_color_dialog->gimage->by_color_select = FALSE;

  if (by_color_dialog->gimage != gdisp->gimage)
    {
      gdk_draw_rectangle
	(by_color_dialog->preview->window,
	 by_color_dialog->preview->style->bg_gc[GTK_STATE_NORMAL],
	 TRUE,
	 0, 0,
	 by_color_dialog->preview->allocation.width,
	 by_color_dialog->preview->allocation.width);
    }

  by_color_dialog->gimage = gdisp->gimage;
  gdisp->gimage->by_color_select = TRUE;

  gdk_pointer_grab (gdisp->canvas->window, FALSE,
                    GDK_POINTER_MOTION_HINT_MASK |
		    GDK_BUTTON1_MOTION_MASK |
                    GDK_BUTTON_RELEASE_MASK,
                    NULL, NULL, bevent->time);
}

static void
by_color_select_button_release (Tool           *tool,
				GdkEventButton *bevent,
				GDisplay       *gdisp)
{
  ByColorSelect *by_color_sel;
  gint           x, y;
  GimpDrawable  *drawable;
  guchar        *color;
  gint           use_offsets;

  by_color_sel = (ByColorSelect *) tool->private;
  drawable = gimp_image_active_drawable (gdisp->gimage);

  gdk_pointer_ungrab (bevent->time);

  tool->state = INACTIVE;

  /*  First take care of the case where the user "cancels" the action  */
  if (! (bevent->state & GDK_BUTTON3_MASK))
    {
      use_offsets = (by_color_options->sample_merged) ? FALSE : TRUE;
      gdisplay_untransform_coords (gdisp, by_color_sel->x, by_color_sel->y,
				   &x, &y, FALSE, use_offsets);

      if( x >= 0 && x < gimp_drawable_width (drawable) && 
	  y >= 0 && y < gimp_drawable_height (drawable))
	{
	  /*  Get the start color  */
	  if (by_color_options->sample_merged)
	    {
	      if (!(color = gimp_image_get_color_at (gdisp->gimage, x, y)))
		return;
	    }
	  else
	    {
	      if (!(color = gimp_drawable_get_color_at (drawable, x, y)))
		return;
	    }

	  /*  select the area  */
	  by_color_select (gdisp->gimage, drawable, color,
			   by_color_dialog->threshold,
			   by_color_sel->operation,
			   by_color_options->antialias,
			   by_color_options->feather,
			   by_color_options->feather_radius,
			   by_color_options->sample_merged);

	  g_free (color);

	  /*  show selection on all views  */
	  gdisplays_flush ();

	  /*  update the preview window  */
	  by_color_select_render (by_color_dialog, gdisp->gimage);
	  by_color_select_draw (by_color_dialog, gdisp->gimage);
	}
    }
}

static void
by_color_select_cursor_update (Tool           *tool,
			       GdkEventMotion *mevent,
			       GDisplay       *gdisp)
{
  ByColorSelect *by_col_sel;
  GimpLayer     *layer;
  gint           x, y;

  by_col_sel = (ByColorSelect *) tool->private;

  gdisplay_untransform_coords (gdisp, mevent->x, mevent->y,
			       &x, &y, FALSE, FALSE);

  if (by_color_options->sample_merged ||
      ((layer = gimp_image_pick_correlate_layer (gdisp->gimage, x, y)) &&
       layer == gdisp->gimage->active_layer))
    {
      switch (by_col_sel->operation)
	{
	case SELECTION_ADD:
	  gdisplay_install_tool_cursor (gdisp, GIMP_MOUSE_CURSOR,
					TOOL_TYPE_NONE,
					CURSOR_MODIFIER_PLUS,
					FALSE);
	  break;
	case SELECTION_SUB:
	  gdisplay_install_tool_cursor (gdisp, GIMP_MOUSE_CURSOR,
					TOOL_TYPE_NONE,
					CURSOR_MODIFIER_MINUS,
					FALSE);
	  break;
	case SELECTION_INTERSECT: 
	  gdisplay_install_tool_cursor (gdisp, GIMP_MOUSE_CURSOR,
					TOOL_TYPE_NONE,
					CURSOR_MODIFIER_INTERSECT,
					FALSE);
	  break;
	case SELECTION_REPLACE:
	  gdisplay_install_tool_cursor (gdisp, GIMP_MOUSE_CURSOR,
					TOOL_TYPE_NONE,
					CURSOR_MODIFIER_NONE,
					FALSE);
	  break;
	case SELECTION_MOVE_MASK:
	  gdisplay_install_tool_cursor (gdisp, GIMP_MOUSE_CURSOR,
					RECT_SELECT,
					CURSOR_MODIFIER_MOVE,
					FALSE);
	  break;
	case SELECTION_MOVE: 
	  gdisplay_install_tool_cursor (gdisp, GIMP_MOUSE_CURSOR,
					MOVE,
					CURSOR_MODIFIER_NONE,
					FALSE);
	  break;
	case SELECTION_ANCHOR:
	  gdisplay_install_tool_cursor (gdisp, GIMP_MOUSE_CURSOR,
					RECT_SELECT,
					CURSOR_MODIFIER_ANCHOR,
					FALSE);
	}

      return;
    }

  gdisplay_install_tool_cursor (gdisp, GIMP_BAD_CURSOR,
				TOOL_TYPE_NONE,
				CURSOR_MODIFIER_NONE,
				FALSE);
}

static void
by_color_select_update_op_state (ByColorSelect *by_col_sel,
				 gint           state,  
				 GDisplay      *gdisp)
{
  if (active_tool->state == ACTIVE)
    return;

  if ((state & GDK_SHIFT_MASK) &&
      !(state & GDK_CONTROL_MASK))
    by_col_sel->operation = SELECTION_ADD;   /* add to the selection */
  else if ((state & GDK_CONTROL_MASK) &&
           !(state & GDK_SHIFT_MASK))
    by_col_sel->operation = SELECTION_SUB;   /* subtract from the selection */
  else if ((state & GDK_CONTROL_MASK) &&
           (state & GDK_SHIFT_MASK))
    by_col_sel->operation = SELECTION_INTERSECT; /* intersect with selection */
  else
    by_col_sel->operation = by_color_dialog->operation;
}

static void
by_color_select_modifier_update (Tool        *tool,
				 GdkEventKey *kevent,
				 GDisplay    *gdisp)
{
  ByColorSelect *by_col_sel;
  gint state;

  by_col_sel = (ByColorSelect *) tool->private;

  state = kevent->state;

  switch (kevent->keyval)
    {
    case GDK_Alt_L: case GDK_Alt_R:
      if (state & GDK_MOD1_MASK)
        state &= ~GDK_MOD1_MASK;
      else
        state |= GDK_MOD1_MASK;
      break;

    case GDK_Shift_L: case GDK_Shift_R:
      if (state & GDK_SHIFT_MASK)
        state &= ~GDK_SHIFT_MASK;
      else
        state |= GDK_SHIFT_MASK;
      break;

    case GDK_Control_L: case GDK_Control_R:
      if (state & GDK_CONTROL_MASK)
        state &= ~GDK_CONTROL_MASK;
      else
        state |= GDK_CONTROL_MASK;
      break;
    }

  by_color_select_update_op_state (by_col_sel, state, gdisp);
}

static void
by_color_select_oper_update (Tool           *tool,
			     GdkEventMotion *mevent,
			     GDisplay       *gdisp)
{
  ByColorSelect *by_col_sel;

  by_col_sel = (ByColorSelect *) tool->private;

  by_color_select_update_op_state (by_col_sel, mevent->state, gdisp);
}

static void
by_color_select_control (Tool       *tool,
			 ToolAction  action,
			 GDisplay   *gdisp)
{
  switch (action)
    {
    case PAUSE:
      break;

    case RESUME:
      break;

    case HALT:
      if (by_color_dialog)
	by_color_select_close_callback (NULL, (gpointer) by_color_dialog);
      break;

    default:
      break;
    }
}

static void
by_color_select_options_reset (void)
{
  selection_options_reset (by_color_options);
}

Tool *
tools_new_by_color_select (void)
{
  Tool          *tool;
  ByColorSelect *private;

  /*  The tool options  */
  if (!by_color_options)
    {
      by_color_options =
	selection_options_new (BY_COLOR_SELECT, by_color_select_options_reset);
      tools_register (BY_COLOR_SELECT, (ToolOptions *) by_color_options);
    }

  tool = tools_new_tool (BY_COLOR_SELECT);
  private = g_new0 (ByColorSelect, 1);

  private->operation = SELECTION_REPLACE;

  tool->private = (void *) private;

  tool->scroll_lock = TRUE;  /*  Disallow scrolling  */

  tool->button_press_func   = by_color_select_button_press;
  tool->button_release_func = by_color_select_button_release;
  tool->cursor_update_func  = by_color_select_cursor_update;
  tool->modifier_key_func   = by_color_select_modifier_update;
  tool->oper_update_func    = by_color_select_oper_update;
  tool->control_func        = by_color_select_control;

  return tool;
}

void
tools_free_by_color_select (Tool *tool)
{
  ByColorSelect *by_color_sel;

  by_color_sel = (ByColorSelect *) tool->private;

  /*  Close the color select dialog  */
  if (by_color_dialog)
    by_color_select_close_callback (NULL, (gpointer) by_color_dialog);

  g_free (by_color_sel);
}

void
by_color_select_initialize_by_image (GImage *gimage)
{
  /*  update the preview window  */
  if (by_color_dialog)
    {
      by_color_dialog->gimage = gimage;
      gimage->by_color_select = TRUE;
      by_color_select_render (by_color_dialog, gimage);
      by_color_select_draw (by_color_dialog, gimage);
    }
}

void
by_color_select_initialize (GDisplay *gdisp)
{
  /*  The "by color" dialog  */
  if (!by_color_dialog)
    by_color_dialog = by_color_select_dialog_new ();
  else
    if (!GTK_WIDGET_VISIBLE (by_color_dialog->shell))
      gtk_widget_show (by_color_dialog->shell);

  by_color_select_initialize_by_image (gdisp->gimage);
}

/****************************/
/*  Select by Color dialog  */
/****************************/

static ByColorDialog *
by_color_select_dialog_new (void)
{
  ByColorDialog *bcd;
  GtkWidget *hbox;
  GtkWidget *frame;
  GtkWidget *options_box;
  GtkWidget *label;
  GtkWidget *util_box;
  GtkWidget *slider;
  GtkWidget *table;
  GtkWidget *button;

  bcd = g_new (ByColorDialog, 1);
  bcd->gimage    = NULL;
  bcd->operation = SELECTION_REPLACE;
  bcd->threshold = default_threshold;

  /*  The shell and main vbox  */
  bcd->shell = gimp_dialog_new (_("By Color Selection"), "by_color_selection",
				tools_help_func, NULL,
				GTK_WIN_POS_NONE,
				FALSE, TRUE, FALSE,

				_("Reset"), by_color_select_reset_callback,
				bcd, NULL, NULL, FALSE, FALSE,
				_("Close"), by_color_select_close_callback,
				bcd, NULL, NULL, TRUE, TRUE,

				NULL);

  /*  The main hbox  */
  hbox = gtk_hbox_new (FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (bcd->shell)->vbox), hbox);

  /*  The preview  */
  util_box = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), util_box, FALSE, FALSE, 0);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (util_box), frame, FALSE, FALSE, 0);

  bcd->preview = gtk_preview_new (GTK_PREVIEW_GRAYSCALE);
  gtk_preview_size (GTK_PREVIEW (bcd->preview), PREVIEW_WIDTH, PREVIEW_HEIGHT);
  gtk_widget_set_events (bcd->preview, PREVIEW_EVENT_MASK);
  gtk_container_add (GTK_CONTAINER (frame), bcd->preview);

  gtk_signal_connect (GTK_OBJECT (bcd->preview), "button_press_event",
		      GTK_SIGNAL_FUNC (by_color_select_preview_events),
		      bcd);

  /*  dnd colors to the image window  */
  gtk_drag_dest_set (bcd->preview, 
                     GTK_DEST_DEFAULT_HIGHLIGHT |
                     GTK_DEST_DEFAULT_MOTION |
                     GTK_DEST_DEFAULT_DROP, 
                     by_color_select_targets,
                     n_by_color_select_targets,
                     GDK_ACTION_COPY);
  gimp_dnd_color_dest_set (bcd->preview, by_color_select_color_drop, bcd);

  gtk_widget_show (bcd->preview);
  gtk_widget_show (frame);
  gtk_widget_show (util_box);

  /*  options box  */
  options_box = gtk_vbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (hbox), options_box, FALSE, FALSE, 0);

  /*  Create the active image label  */
  util_box = gtk_hbox_new (FALSE, 2);
  gtk_box_pack_start (GTK_BOX (options_box), util_box, FALSE, FALSE, 0);

  bcd->gimage_name = gtk_label_new (_("Inactive"));
  gtk_box_pack_start (GTK_BOX (util_box), bcd->gimage_name, FALSE, FALSE, 0);

  gtk_widget_show (bcd->gimage_name);
  gtk_widget_show (util_box);

  /*  Create the selection mode radio box  */
  frame = 
    gimp_radio_group_new2 (TRUE, _("Selection Mode"),
			   gimp_radio_button_update,
			   &bcd->operation,
			   (gpointer) bcd->operation,

			   _("Replace"),   (gpointer) SELECTION_REPLACE,
			   &bcd->replace_button,
			   _("Add"),       (gpointer) SELECTION_ADD, NULL,
			   _("Subtract"),  (gpointer) SELECTION_SUB, NULL,
			   _("Intersect"), (gpointer) SELECTION_INTERSECT, NULL,

			   NULL);

  gtk_box_pack_start (GTK_BOX (options_box), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  /*  Create the opacity scale widget  */
  util_box = gtk_vbox_new (FALSE, 2);
  gtk_box_pack_start (GTK_BOX (options_box), util_box, FALSE, FALSE, 0);

  label = gtk_label_new (_("Fuzziness Threshold"));
  gtk_box_pack_start (GTK_BOX (util_box), label, FALSE, FALSE, 2);

  gtk_widget_show (label);
  gtk_widget_show (util_box);

  bcd->threshold_adj =
    gtk_adjustment_new (bcd->threshold, 0.0, 255.0, 1.0, 1.0, 0.0);
  slider = gtk_hscale_new (GTK_ADJUSTMENT (bcd->threshold_adj));
  gtk_box_pack_start (GTK_BOX (util_box), slider, TRUE, TRUE, 0);
  gtk_scale_set_value_pos (GTK_SCALE (slider), GTK_POS_TOP);
  gtk_range_set_update_policy (GTK_RANGE (slider), GTK_UPDATE_DELAYED);

  gtk_signal_connect (GTK_OBJECT (bcd->threshold_adj), "value_changed",
		      GTK_SIGNAL_FUNC (gimp_int_adjustment_update),
		      &bcd->threshold);

  gtk_widget_show (slider);

  frame = gtk_frame_new (_("Selection"));
  gtk_box_pack_end (GTK_BOX (options_box), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  table = gtk_table_new (2, 2, TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (table), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table), 2);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  button = gtk_button_new_with_label (_("Invert"));
  gtk_table_attach_defaults (GTK_TABLE (table), button, 0, 2, 0, 1);
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      GTK_SIGNAL_FUNC (by_color_select_invert_callback),
		      bcd);
  gtk_widget_show (button);

  button = gtk_button_new_with_label (_("All"));
  gtk_table_attach_defaults (GTK_TABLE (table), button, 0, 1, 1, 2);
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      GTK_SIGNAL_FUNC (by_color_select_select_all_callback),
		      bcd);
  gtk_widget_show (button);

  button = gtk_button_new_with_label (_("None"));
  gtk_table_attach_defaults (GTK_TABLE (table), button, 1, 2, 1, 2);
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      GTK_SIGNAL_FUNC (by_color_select_select_none_callback),
		      bcd);
  gtk_widget_show (button);

  gtk_widget_show (options_box);
  gtk_widget_show (hbox);
  gtk_widget_show (bcd->shell);

  gtk_signal_connect_object (GTK_OBJECT (bcd->shell), "unmap_event",
			     GTK_SIGNAL_FUNC (gimp_dialog_hide), 
			     (gpointer) bcd->shell);

  return bcd;
}

static void
by_color_select_render (ByColorDialog *bcd,
			GImage        *gimage)
{
  GimpChannel *mask;
  MaskBuf     *scaled_buf = NULL;
  guchar      *buf;
  PixelRegion  srcPR, destPR;
  guchar      *src;
  gint         subsample;
  gint         width, height;
  gint         srcwidth;
  gint         i;
  gint         scale;

  mask = gimp_image_get_mask (gimage);
  if ((gimp_drawable_width (GIMP_DRAWABLE(mask)) > PREVIEW_WIDTH) ||
      (gimp_drawable_height (GIMP_DRAWABLE(mask)) > PREVIEW_HEIGHT))
    {
      if (((float) gimp_drawable_width (GIMP_DRAWABLE (mask)) / (float) PREVIEW_WIDTH) >
	  ((float) gimp_drawable_height (GIMP_DRAWABLE (mask)) / (float) PREVIEW_HEIGHT))
	{
	  width = PREVIEW_WIDTH;
	  height = ((gimp_drawable_height (GIMP_DRAWABLE (mask)) * PREVIEW_WIDTH) /
		    gimp_drawable_width (GIMP_DRAWABLE (mask)));
	}
      else
	{
	  width = ((gimp_drawable_width (GIMP_DRAWABLE (mask)) * PREVIEW_HEIGHT) /
		   gimp_drawable_height (GIMP_DRAWABLE (mask)));
	  height = PREVIEW_HEIGHT;
	}

      scale = TRUE;
    }
  else
    {
      width  = gimp_drawable_width (GIMP_DRAWABLE (mask));
      height = gimp_drawable_height (GIMP_DRAWABLE (mask));

      scale = FALSE;
    }

  if ((width != bcd->preview->requisition.width) ||
      (height != bcd->preview->requisition.height))
    gtk_preview_size (GTK_PREVIEW (bcd->preview), width, height);

  /*  clear the image buf  */
  buf = g_new0 (guchar, bcd->preview->requisition.width);
  for (i = 0; i < bcd->preview->requisition.height; i++)
    gtk_preview_draw_row (GTK_PREVIEW (bcd->preview), buf,
			  0, i, bcd->preview->requisition.width);
  g_free (buf);

  /*  if the mask is empty, no need to scale and update again  */
  if (gimage_mask_is_empty (gimage))
    return;

  if (scale)
    {
      /*  calculate 'acceptable' subsample  */
      subsample = 1;
      while ((width * (subsample + 1) * 2 < gimp_drawable_width (GIMP_DRAWABLE (mask))) &&
	     (height * (subsample + 1) * 2 < gimp_drawable_height (GIMP_DRAWABLE (mask))))
	subsample = subsample + 1;

      pixel_region_init (&srcPR, gimp_drawable_data (GIMP_DRAWABLE (mask)), 
			 0, 0, 
			 gimp_drawable_width (GIMP_DRAWABLE (mask)), 
			 gimp_drawable_height (GIMP_DRAWABLE (mask)), FALSE);

      scaled_buf = mask_buf_new (width, height);
      destPR.bytes = 1;
      destPR.x = 0;
      destPR.y = 0;
      destPR.w = width;
      destPR.h = height;
      destPR.rowstride = srcPR.bytes * width;
      destPR.data = mask_buf_data (scaled_buf);
      destPR.tiles = NULL;

      subsample_region (&srcPR, &destPR, subsample);
    }
  else
    {
      pixel_region_init (&srcPR, gimp_drawable_data (GIMP_DRAWABLE (mask)), 
			 0, 0, 
			 gimp_drawable_width (GIMP_DRAWABLE (mask)), 
			 gimp_drawable_height (GIMP_DRAWABLE (mask)), FALSE);

      scaled_buf = mask_buf_new (width, height);
      destPR.bytes = 1;
      destPR.x = 0;
      destPR.y = 0;
      destPR.w = width;
      destPR.h = height;
      destPR.rowstride = srcPR.bytes * width;
      destPR.data = mask_buf_data (scaled_buf);
      destPR.tiles = NULL;

      copy_region (&srcPR, &destPR);
    }

  src = mask_buf_data (scaled_buf);
  srcwidth = scaled_buf->width;
  for (i = 0; i < height; i++)
    {
      gtk_preview_draw_row (GTK_PREVIEW (bcd->preview), src, 0, i, width);
      src += srcwidth;
    }

  mask_buf_free (scaled_buf);
}

static void
by_color_select_draw (ByColorDialog *bcd,
		      GImage        *gimage)
{
  /*  Draw the image buf to the preview window  */
  gtk_widget_draw (bcd->preview, NULL);

  /*  Update the gimage label to reflect the displayed gimage name  */
  gtk_label_set_text (GTK_LABEL (bcd->gimage_name),
		      g_basename (gimp_image_filename (gimage)));
}

static gint
by_color_select_preview_events (GtkWidget      *widget,
				GdkEventButton *bevent,
				ByColorDialog  *bcd)
{
  switch (bevent->type)
    {
    case GDK_BUTTON_PRESS:
      by_color_select_preview_button_press (bcd, bevent);
      break;

    default:
      break;
    }

  return FALSE;
}

static void
by_color_select_reset_callback (GtkWidget *widget,
				gpointer   data)
{
  ByColorDialog *bcd;

  bcd = (ByColorDialog *) data;

  gtk_widget_activate (bcd->replace_button);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (bcd->threshold_adj),
			    default_threshold);
}

static void
by_color_select_invert_callback (GtkWidget *widget,
				 gpointer   data)
{
  ByColorDialog *bcd;

  bcd = (ByColorDialog *) data;

  if (!bcd->gimage)
    return;

  /*  check if the image associated to the mask still exists  */
  if (! gimp_drawable_gimage (GIMP_DRAWABLE (gimp_image_get_mask (bcd->gimage))))
    return;

  /*  invert the mask  */
  gimage_mask_invert (bcd->gimage);

  /*  show selection on all views  */
  gdisplays_flush ();

  /*  update the preview window  */
  by_color_select_render (bcd, bcd->gimage);
  by_color_select_draw (bcd, bcd->gimage);
}

static void
by_color_select_select_all_callback (GtkWidget *widget,
				     gpointer   data)
{
  ByColorDialog *bcd;

  bcd = (ByColorDialog *) data;

  if (!bcd->gimage)
    return;

  /*  check if the image associated to the mask still exists  */
  if (! gimp_drawable_gimage (GIMP_DRAWABLE (gimp_image_get_mask (bcd->gimage))))
    return;

  /*  fill the mask  */
  gimage_mask_all (bcd->gimage);

  /*  show selection on all views  */
  gdisplays_flush ();

  /*  update the preview window  */
  by_color_select_render (bcd, bcd->gimage);
  by_color_select_draw (bcd, bcd->gimage);
}

static void
by_color_select_select_none_callback (GtkWidget *widget,
				      gpointer   data)
{
  ByColorDialog *bcd;

  bcd = (ByColorDialog *) data;

  if (!bcd->gimage)
    return;

  /*  check if the image associated to the mask still exists  */
  if (! gimp_drawable_gimage (GIMP_DRAWABLE (gimp_image_get_mask (bcd->gimage))))
    return;

  /*  reset the mask  */
  gimage_mask_clear (bcd->gimage);

  /*  show selection on all views  */
  gdisplays_flush ();

  /*  update the preview window  */
  by_color_select_render (bcd, bcd->gimage);
  by_color_select_draw (bcd, bcd->gimage);
}

static void
by_color_select_close_callback (GtkWidget *widget,
				gpointer   data)
{
  ByColorDialog *bcd;

  bcd = (ByColorDialog *) data;
  
  gimp_dialog_hide (bcd->shell);

  if (bcd->gimage && gimp_container_lookup (image_context,
					    GIMP_OBJECT (bcd->gimage)))
    {
      bcd->gimage->by_color_select = FALSE;
    }

  bcd->gimage = NULL;
}

static void
by_color_select_preview_button_press (ByColorDialog  *bcd,
				      GdkEventButton *bevent)
{
  gint          x, y;
  gboolean      replace;
  SelectOps     operation;
  GimpDrawable *drawable;
  Tile         *tile;
  guchar       *col;

  if (!bcd->gimage)
    return;

  drawable = gimp_image_active_drawable (bcd->gimage);

  /*  check if the gimage associated to the drawable still exists  */
  if (! gimp_drawable_gimage (drawable))
    return;

  /*  Defaults  */
  replace = FALSE;
  operation = SELECTION_REPLACE;

  /*  Based on modifiers, and the "by color" dialog's selection mode  */
  if ((bevent->state & GDK_SHIFT_MASK) &&
      !(bevent->state & GDK_CONTROL_MASK))
    operation = SELECTION_ADD;
  else if ((bevent->state & GDK_CONTROL_MASK) &&
	   !(bevent->state & GDK_SHIFT_MASK))
    operation = SELECTION_SUB;
  else if ((bevent->state & GDK_CONTROL_MASK) &&
	   (bevent->state & GDK_SHIFT_MASK))
    operation = SELECTION_INTERSECT;
  else
    operation = by_color_dialog->operation;

  /*  Find x, y and modify selection  */

  /*  Get the start color  */
  if (by_color_options->sample_merged)
    {
      x = bcd->gimage->width * bevent->x / bcd->preview->requisition.width;
      y = bcd->gimage->height * bevent->y / bcd->preview->requisition.height;
      if (x < 0 || y < 0 || x >= bcd->gimage->width || y >= bcd->gimage->height)
	return;
      tile = tile_manager_get_tile (gimp_image_composite (bcd->gimage),
				    x, y, TRUE, FALSE);
      col = tile_data_pointer (tile, x % TILE_WIDTH, y % TILE_HEIGHT);
    }
  else
    {
      gint offx, offy;

      gimp_drawable_offsets (drawable, &offx, &offy);
      x = gimp_drawable_width (drawable) * bevent->x / bcd->preview->requisition.width - offx;
      y = gimp_drawable_height (drawable) * bevent->y / bcd->preview->requisition.height - offy;
      if (x < 0 || y < 0 ||
	  x >= gimp_drawable_width (drawable) || y >= gimp_drawable_height (drawable))
	return;
      tile = tile_manager_get_tile (gimp_drawable_data (drawable),
				    x, y, TRUE, FALSE);
      col = tile_data_pointer (tile, x % TILE_WIDTH, y % TILE_HEIGHT);
    }

  by_color_select (bcd->gimage, drawable, col,
		   bcd->threshold,
		   operation,
		   by_color_options->antialias,
		   by_color_options->feather,
		   by_color_options->feather_radius,
		   by_color_options->sample_merged);

  tile_release (tile, FALSE);

  /*  show selection on all views  */
  gdisplays_flush ();

  /*  update the preview window  */
  by_color_select_render (bcd, bcd->gimage);
  by_color_select_draw (bcd, bcd->gimage);
}

static void
by_color_select_color_drop (GtkWidget     *widget,
                            const GimpRGB *color,
                            gpointer       data)

{
  GimpDrawable  *drawable;
  ByColorDialog *bcd;
  guchar         col[3];

  bcd = (ByColorDialog*) data;
  drawable = gimp_image_active_drawable (bcd->gimage);

  gimp_rgb_get_uchar (color, &col[0], &col[1], &col[2]);

  by_color_select (bcd->gimage, 
                   drawable, 
                   col, 
                   bcd->threshold,
                   bcd->operation,
                   by_color_options->antialias,
                   by_color_options->feather,
                   by_color_options->feather_radius,
                   by_color_options->sample_merged);     

  /*  show selection on all views  */
  gdisplays_flush ();

  /*  update the preview window  */
  by_color_select_render (bcd, bcd->gimage);
  by_color_select_draw (bcd, bcd->gimage);
}
