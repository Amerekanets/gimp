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
#include <sys/types.h>

#include <gtk/gtk.h>

#include "libgimpcolor/gimpcolor.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "apptypes.h"

#include "tools/gimptoolinfo.h"
#include "tools/tool_options_dialog.h"
#include "tools/tool.h"
#include "tools/tool_manager.h"

#include "about_dialog.h"
#include "app_procs.h"
#include "brush_select.h"
#include "colormap_dialog.h"
#include "color_area.h"
#include "commands.h"
#include "context_manager.h"
#include "convert.h"
#include "desaturate.h"
#include "devices.h"
#include "docindex.h"
#include "channel_ops.h"
#include "equalize.h"
#include "errorconsole.h"
#include "fileops.h"
#include "floating_sel.h"
#include "gdisplay_ops.h"
#include "gimage_mask.h"
#include "gimpcontainer.h"
#include "gimpcontext.h"
#include "gimpdrawable.h"
#include "gimphelp.h"
#include "gimpimage.h"
#include "gimplayer.h"
#include "gimprc.h"
#include "gimpui.h"
#include "global_edit.h"
#include "gradient_select.h"
#include "image_render.h"
#include "info_dialog.h"
#include "info_window.h"
#include "nav_window.h"
#include "invert.h"
#include "lc_dialog.h"
#include "layer_select.h"
#include "layers_dialogP.h"
#include "module_db.h"
#include "palette.h"
#include "pattern_select.h"
#include "plug_in.h"
#include "preferences_dialog.h"
#include "resize.h"
#include "scale.h"
#include "selection.h"
#include "tips_dialog.h"
#include "toolbox.h"
#include "undo.h"
#include "undo_history.h"

#ifdef DISPLAY_FILTERS
#include "gdisplay_color_ui.h"
#endif /* DISPLAY_FILTERS */

#include "libgimp/gimpintl.h"


#define return_if_no_display(gdisp) \
        gdisp = gdisplay_active (); \
        if (!gdisp) return


/*  local functions  */
static void     image_resize_callback        (GtkWidget *widget,
					      gpointer   data);
static void     image_scale_callback         (GtkWidget *widget,
					      gpointer   data);
static void     gimage_mask_feather_callback (GtkWidget *widget,
					      gdouble    size,
					      GimpUnit   unit,
					      gpointer   data);
static void     gimage_mask_border_callback  (GtkWidget *widget,
					      gdouble    size,
					      GimpUnit   unit,
		  			      gpointer   data);
static void     gimage_mask_grow_callback    (GtkWidget *widget,
					      gdouble    size,
					      GimpUnit   unit,
					      gpointer   data);
static void     gimage_mask_shrink_callback  (GtkWidget *widget,
					      gdouble    size,
					      GimpUnit   unit,
					      gpointer   data);


/*  local variables  */
static gdouble   selection_feather_radius    = 5.0;
static gint      selection_border_radius     = 5;
static gint      selection_grow_pixels       = 1;
static gint      selection_shrink_pixels     = 1;
static gboolean  selection_shrink_edge_lock  = FALSE;


/*****  File  *****/

void
file_open_cmd_callback (GtkWidget *widget,
			gpointer   client_data)
{
  file_open_callback (widget, client_data);
}

void
file_save_cmd_callback (GtkWidget *widget,
			gpointer   client_data)
{
  file_save_callback (widget, client_data);
}

void
file_save_as_cmd_callback (GtkWidget *widget,
			   gpointer   client_data)
{
  file_save_as_callback (widget, client_data);
}

void
file_save_a_copy_as_cmd_callback (GtkWidget *widget,
				  gpointer   client_data)
{
  file_save_a_copy_as_callback (widget, client_data);
}

void
file_revert_cmd_callback (GtkWidget *widget,
			gpointer   client_data)
{
  file_revert_callback (widget, client_data);
}

void
file_close_cmd_callback (GtkWidget *widget,
			 gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  gdisplay_close_window (gdisp, FALSE);
}

void
file_quit_cmd_callback (GtkWidget *widget,
			gpointer   client_data)
{
  app_exit (FALSE);
}

/*****  Edit  *****/

void
edit_undo_cmd_callback (GtkWidget *widget,
			gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  undo_pop (gdisp->gimage);
}

void
edit_redo_cmd_callback (GtkWidget *widget,
			gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  undo_redo (gdisp->gimage);
}

void
edit_cut_cmd_callback (GtkWidget *widget,
		       gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  global_edit_cut (gdisp);
}

void
edit_copy_cmd_callback (GtkWidget *widget,
			gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  global_edit_copy (gdisp);
}

void
edit_paste_cmd_callback (GtkWidget *widget,
			 gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  global_edit_paste (gdisp, 0);
}

void
edit_paste_into_cmd_callback (GtkWidget *widget,
			      gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  global_edit_paste (gdisp, 1);
}

void
edit_paste_as_new_cmd_callback (GtkWidget *widget,
				gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  global_edit_paste_as_new (gdisp);
}

void
edit_named_cut_cmd_callback (GtkWidget *widget,
			     gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  named_edit_cut (gdisp);
}

void
edit_named_copy_cmd_callback (GtkWidget *widget,
			      gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  named_edit_copy (gdisp);
}

void
edit_named_paste_cmd_callback (GtkWidget *widget,
			       gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  named_edit_paste (gdisp);
}

void
edit_clear_cmd_callback (GtkWidget *widget,
			 gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  edit_clear (gdisp->gimage, gimp_image_active_drawable (gdisp->gimage));
  gdisplays_flush ();
}

void
edit_fill_cmd_callback (GtkWidget *widget,
			gpointer   callback_data,
			guint      callback_action)
{
  GimpFillType fill_type;
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  fill_type = (GimpFillType) callback_action;
  edit_fill (gdisp->gimage, gimp_image_active_drawable (gdisp->gimage),
	     fill_type);
  gdisplays_flush ();
}

void
edit_stroke_cmd_callback (GtkWidget *widget,
			  gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  gimage_mask_stroke (gdisp->gimage, gimp_image_active_drawable (gdisp->gimage));
  gdisplays_flush ();
}

/*****  Select  *****/

void
select_invert_cmd_callback (GtkWidget *widget,
			    gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  gimage_mask_invert (gdisp->gimage);
  gdisplays_flush ();
}

void
select_all_cmd_callback (GtkWidget *widget,
			 gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  gimage_mask_all (gdisp->gimage);
  gdisplays_flush ();
}

void
select_none_cmd_callback (GtkWidget *widget,
			  gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  gimage_mask_none (gdisp->gimage);
  gdisplays_flush ();
}

void
select_float_cmd_callback (GtkWidget *widget,
			   gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  gimage_mask_float (gdisp->gimage, gimp_image_active_drawable (gdisp->gimage),
		     0, 0);
  gdisplays_flush ();
}

void
select_feather_cmd_callback (GtkWidget *widget,
			     gpointer   client_data)
{
  GtkWidget *qbox;
  GDisplay  *gdisp;

  return_if_no_display (gdisp);

  qbox = gimp_query_size_box (_("Feather Selection"),
			      gimp_standard_help_func,
			      "dialogs/feather_selection.html",
			      _("Feather Selection by:"),
			      selection_feather_radius, 0, 32767, 3,
			      gdisp->gimage->unit,
			      MIN (gdisp->gimage->xresolution,
				   gdisp->gimage->yresolution),
			      gdisp->dot_for_dot,
			      GTK_OBJECT (gdisp->gimage), "destroy",
			      gimage_mask_feather_callback, gdisp->gimage);
  gtk_widget_show (qbox);
}

void
select_sharpen_cmd_callback (GtkWidget *widget,
			     gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  gimage_mask_sharpen (gdisp->gimage);
  gdisplays_flush ();
}

void
select_shrink_cmd_callback (GtkWidget *widget,
			    gpointer   client_data)
{
  GtkWidget *edge_lock;
  GtkWidget *shrink_dialog;

  GDisplay *gdisp;
  return_if_no_display (gdisp);

  shrink_dialog =
    gimp_query_size_box (N_("Shrink Selection"),
			 gimp_standard_help_func,
			 "dialogs/shrink_selection.html",
			 _("Shrink Selection by:"),
			 selection_shrink_pixels, 1, 32767, 0,
			 gdisp->gimage->unit,
			 MIN (gdisp->gimage->xresolution,
			      gdisp->gimage->yresolution),
			 gdisp->dot_for_dot,
			 GTK_OBJECT (gdisp->gimage), "destroy",
			 gimage_mask_shrink_callback, gdisp->gimage);

  edge_lock = gtk_check_button_new_with_label (_("Shrink from image border"));
  /* eeek */
  gtk_box_pack_start (GTK_BOX (g_list_nth_data (gtk_container_children (GTK_CONTAINER (GTK_DIALOG (shrink_dialog)->vbox)), 0)), edge_lock,
		      FALSE, FALSE, 0);
  gtk_object_set_data (GTK_OBJECT (shrink_dialog), "edge_lock_toggle",
		       edge_lock);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (edge_lock),
				! selection_shrink_edge_lock);
  gtk_widget_show (edge_lock);

  gtk_widget_show (shrink_dialog);
}

void
select_grow_cmd_callback (GtkWidget *widget,
			  gpointer   client_data)
{
  GtkWidget *qbox;
  GDisplay  *gdisp;

  return_if_no_display (gdisp);

  qbox = gimp_query_size_box (_("Grow Selection"),
			      gimp_standard_help_func,
			      "dialogs/grow_selection.html",
			      _("Grow Selection by:"),
			      selection_grow_pixels, 1, 32767, 0,
			      gdisp->gimage->unit,
			      MIN (gdisp->gimage->xresolution,
				   gdisp->gimage->yresolution),
			      gdisp->dot_for_dot,
			      GTK_OBJECT (gdisp->gimage), "destroy",
			      gimage_mask_grow_callback, gdisp->gimage);
  gtk_widget_show (qbox);
}

void
select_border_cmd_callback (GtkWidget *widget,
			    gpointer   client_data)
{
  GtkWidget *qbox;
  GDisplay  *gdisp;

  return_if_no_display (gdisp);

  qbox = gimp_query_size_box (_("Border Selection"),
			      gimp_standard_help_func,
			      "dialogs/border_selection.html",
			      _("Border Selection by:"),
			      selection_border_radius, 1, 32767, 0,
			      gdisp->gimage->unit,
			      MIN (gdisp->gimage->xresolution,
				   gdisp->gimage->yresolution),
			      gdisp->dot_for_dot,
			      GTK_OBJECT (gdisp->gimage), "destroy",
			      gimage_mask_border_callback, gdisp->gimage);
  gtk_widget_show (qbox);
}

void
select_save_cmd_callback (GtkWidget *widget,
			  gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  gimage_mask_save (gdisp->gimage);
  gdisplays_flush ();
}

/*****  View  *****/

void
view_zoomin_cmd_callback (GtkWidget *widget,
			  gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  change_scale (gdisp, GIMP_ZOOM_IN);
}

void
view_zoomout_cmd_callback (GtkWidget *widget,
			   gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  change_scale (gdisp, GIMP_ZOOM_OUT);
}

static void
view_zoom_val (GtkWidget *widget,
	       gpointer   client_data,
	       int        val)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  change_scale (gdisp, val);
}

void
view_zoom_16_1_cmd_callback (GtkWidget *widget,
			     gpointer   client_data)
{
  view_zoom_val (widget, client_data, 1601);
}

void
view_zoom_8_1_cmd_callback (GtkWidget *widget,
			    gpointer   client_data)
{
  view_zoom_val (widget, client_data, 801);
}

void
view_zoom_4_1_cmd_callback (GtkWidget *widget,
			    gpointer   client_data)
{
  view_zoom_val (widget, client_data, 401);
}

void
view_zoom_2_1_cmd_callback (GtkWidget *widget,
			    gpointer   client_data)
{
  view_zoom_val (widget, client_data, 201);
}

void
view_zoom_1_1_cmd_callback (GtkWidget *widget,
			    gpointer   client_data)
{
  view_zoom_val (widget, client_data, 101);
}

void
view_zoom_1_2_cmd_callback (GtkWidget *widget,
			    gpointer   client_data)
{
  view_zoom_val (widget, client_data, 102);
}

void
view_zoom_1_4_cmd_callback (GtkWidget *widget,
			    gpointer   client_data)
{
  view_zoom_val (widget, client_data, 104);
}

void
view_zoom_1_8_cmd_callback (GtkWidget *widget,
			    gpointer   client_data)
{
  view_zoom_val (widget, client_data, 108);
}

void
view_zoom_1_16_cmd_callback (GtkWidget *widget,
			     gpointer   client_data)
{
  view_zoom_val (widget, client_data, 116);
}

void
view_dot_for_dot_cmd_callback (GtkWidget *widget,
			       gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  gdisplay_set_dot_for_dot (gdisp, GTK_CHECK_MENU_ITEM (widget)->active);
}

void
view_info_window_cmd_callback (GtkWidget *widget,
			       gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  if (!info_window_follows_mouse) 
    {
      if (! gdisp->window_info_dialog)
	gdisp->window_info_dialog = info_window_create ((void *) gdisp);
      info_window_update(gdisp);
      info_dialog_popup (gdisp->window_info_dialog);
    }
  else
    {
      info_window_follow_auto();
    }

}

void
view_nav_window_cmd_callback (GtkWidget *widget,
			      gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  if (nav_window_per_display) 
    {
      if (! gdisp->window_nav_dialog)
	gdisp->window_nav_dialog = nav_window_create ((void *) gdisp);

      nav_dialog_popup (gdisp->window_nav_dialog);
    }
  else
    {
      nav_window_follow_auto ();
    }
}

void
view_toggle_selection_cmd_callback (GtkWidget *widget,
				    gpointer   client_data)
{
  GDisplay *gdisp;
  gint      new_val;

  return_if_no_display (gdisp);

  new_val = GTK_CHECK_MENU_ITEM (widget)->active;

  /*  hidden == TRUE corresponds to the menu toggle being FALSE  */
  if (new_val == gdisp->select->hidden)
    {
      selection_hide (gdisp->select, (void *) gdisp);
      gdisplays_flush ();
    }
}

void
view_toggle_rulers_cmd_callback (GtkWidget *widget,
				 gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  if (!GTK_CHECK_MENU_ITEM (widget)->active)
    {
      if (GTK_WIDGET_VISIBLE (gdisp->origin))
	{
	  gtk_widget_hide (gdisp->origin);
	  gtk_widget_hide (gdisp->hrule);
	  gtk_widget_hide (gdisp->vrule);

	  gtk_widget_queue_resize (GTK_WIDGET (gdisp->origin->parent));
	}
    }
  else
    {
      if (!GTK_WIDGET_VISIBLE (gdisp->origin))
	{
	  gtk_widget_show (gdisp->origin);
	  gtk_widget_show (gdisp->hrule);
	  gtk_widget_show (gdisp->vrule);

	  gtk_widget_queue_resize (GTK_WIDGET (gdisp->origin->parent));
	}
    }
}

void
view_toggle_statusbar_cmd_callback (GtkWidget *widget,
				    gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  if (!GTK_CHECK_MENU_ITEM (widget)->active)
    {
      if (GTK_WIDGET_VISIBLE (gdisp->statusarea))
	gtk_widget_hide (gdisp->statusarea);
    }
  else
    {
      if (!GTK_WIDGET_VISIBLE (gdisp->statusarea))
	gtk_widget_show (gdisp->statusarea);
    }
}

void
view_toggle_guides_cmd_callback (GtkWidget *widget,
				 gpointer   client_data)
{
  GDisplay *gdisp;
  gint      old_val;

  return_if_no_display (gdisp);

  old_val = gdisp->draw_guides;
  gdisp->draw_guides = GTK_CHECK_MENU_ITEM (widget)->active;

  if ((old_val != gdisp->draw_guides) && gdisp->gimage->guides)
    {
      gdisplay_expose_full (gdisp);
      gdisplays_flush ();
    }
}

void
view_snap_to_guides_cmd_callback (GtkWidget *widget,
				  gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  gdisp->snap_to_guides = GTK_CHECK_MENU_ITEM (widget)->active;
}

void
view_new_view_cmd_callback (GtkWidget *widget,
			    gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  gdisplay_new_view (gdisp);
}

void
view_shrink_wrap_cmd_callback (GtkWidget *widget,
			       gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  shrink_wrap_display (gdisp);
}

/*****  Image  *****/

void
image_convert_rgb_cmd_callback (GtkWidget *widget,
				gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  convert_to_rgb (gdisp->gimage);
}

void
image_convert_grayscale_cmd_callback (GtkWidget *widget,
				      gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  convert_to_grayscale (gdisp->gimage);
}

void
image_convert_indexed_cmd_callback (GtkWidget *widget,
				    gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  convert_to_indexed (gdisp->gimage);
}

void
image_desaturate_cmd_callback (GtkWidget *widget,
			       gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  image_desaturate (gdisp->gimage);
  gdisplays_flush ();
}

void
image_invert_cmd_callback (GtkWidget *widget,
			   gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  image_invert (gdisp->gimage);
  gdisplays_flush ();
}

void
image_equalize_cmd_callback (GtkWidget *widget,
			     gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  image_equalize (gdisp->gimage);
  gdisplays_flush ();
}

void
image_offset_cmd_callback (GtkWidget *widget,
			   gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  channel_ops_offset (gdisp->gimage);
}

void
image_resize_cmd_callback (GtkWidget *widget,
			   gpointer   client_data)
{
  GDisplay    *gdisp;
  GimpImage   *gimage;
  ImageResize *image_resize;

  return_if_no_display (gdisp);

  gimage = gdisp->gimage;

  /*  the ImageResize structure  */
  image_resize = g_new (ImageResize, 1);
  image_resize->gimage = gimage;
  image_resize->resize = resize_widget_new (ResizeWidget,
					    ResizeImage,
					    GTK_OBJECT (gimage),
					    "destroy",
					    gimage->width,
					    gimage->height,
					    gimage->xresolution,
					    gimage->yresolution,
					    gimage->unit,
					    gdisp->dot_for_dot,
					    image_resize_callback,
					    NULL,
					    image_resize);

  gtk_signal_connect_object (GTK_OBJECT (image_resize->resize->resize_shell),
			     "destroy",
			     GTK_SIGNAL_FUNC (g_free),
			     (GtkObject *) image_resize);

  gtk_widget_show (image_resize->resize->resize_shell);
}

void
image_scale_cmd_callback (GtkWidget *widget,
			  gpointer   client_data)
{
  GDisplay    *gdisp;
  GimpImage   *gimage;
  ImageResize *image_scale;

  return_if_no_display (gdisp);

  gimage = gdisp->gimage;

  /*  the ImageResize structure  */
  image_scale = g_new (ImageResize, 1);
  image_scale->gimage = gimage;
  image_scale->resize = resize_widget_new (ScaleWidget,
					   ResizeImage,
					   GTK_OBJECT (gimage),
					   "destroy",
					   gimage->width,
					   gimage->height,
					   gimage->xresolution,
					   gimage->yresolution,
					   gimage->unit,
					   gdisp->dot_for_dot,
					   image_scale_callback,
					   NULL,
					   image_scale);

  gtk_signal_connect_object (GTK_OBJECT (image_scale->resize->resize_shell),
			     "destroy",
			     GTK_SIGNAL_FUNC (g_free),
			     (GtkObject *) image_scale);

  gtk_widget_show (image_scale->resize->resize_shell);
}

void
image_duplicate_cmd_callback (GtkWidget *widget,
			      gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  channel_ops_duplicate (gdisp->gimage);
}

/*****  Layers  *****/

void
layers_previous_cmd_callback (GtkWidget *widget,
			      gpointer   client_data)
{
  GDisplay  *gdisp;
  GimpLayer *new_layer;
  gint       current_layer;

  return_if_no_display (gdisp);

  current_layer =
    gimp_image_get_layer_index (gdisp->gimage,
				gimp_image_get_active_layer (gdisp->gimage));

  new_layer = (GimpLayer *)
    gimp_container_get_child_by_index (gdisp->gimage->layers, 
				       current_layer - 1);

  if (new_layer)
    {
      gimp_image_set_active_layer (gdisp->gimage, new_layer);
      gdisplays_flush ();
      layer_select_init (gdisp->gimage, 0, GDK_CURRENT_TIME);
    }
}

void
layers_next_cmd_callback (GtkWidget *widget,
			  gpointer   client_data)
{
  GDisplay  *gdisp;
  GimpLayer *new_layer;
  gint       current_layer;

  return_if_no_display (gdisp);

  current_layer =
    gimp_image_get_layer_index (gdisp->gimage,
				gimp_image_get_active_layer (gdisp->gimage));

  new_layer = (GimpLayer *)
    gimp_container_get_child_by_index (gdisp->gimage->layers, 
				       current_layer + 1);

  if (new_layer)
    {
      gimp_image_set_active_layer (gdisp->gimage, new_layer);
      gdisplays_flush ();
      layer_select_init (gdisp->gimage, 0, GDK_CURRENT_TIME);
    }
}

void
layers_raise_cmd_callback (GtkWidget *widget,
			   gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  gimp_image_raise_layer (gdisp->gimage,
			  gimp_image_get_active_layer (gdisp->gimage));
  gdisplays_flush ();
}

void
layers_lower_cmd_callback (GtkWidget *widget,
			   gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  gimp_image_lower_layer (gdisp->gimage,
			  gimp_image_get_active_layer (gdisp->gimage));
  gdisplays_flush ();
}

void
layers_raise_to_top_cmd_callback (GtkWidget *widget,
				  gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  gimp_image_raise_layer_to_top (gdisp->gimage,
				 gimp_image_get_active_layer (gdisp->gimage));
  gdisplays_flush ();
}

void
layers_lower_to_bottom_cmd_callback (GtkWidget *widget,
				     gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  gimp_image_lower_layer_to_bottom (gdisp->gimage,
				    gimp_image_get_active_layer (gdisp->gimage));
  gdisplays_flush ();
}

void
layers_anchor_cmd_callback (GtkWidget *widget,
			    gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  floating_sel_anchor (gimp_image_get_active_layer (gdisp->gimage));
  gdisplays_flush ();
}

void
layers_merge_cmd_callback (GtkWidget *widget,
			   gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  layers_dialog_layer_merge_query (gdisp->gimage, TRUE);
}

void
layers_flatten_cmd_callback (GtkWidget *widget,
			     gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  gimp_image_flatten (gdisp->gimage);
  gdisplays_flush ();
}

void
layers_mask_select_cmd_callback (GtkWidget *widget,
				 gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  gimage_mask_layer_mask (gdisp->gimage,
			  gimp_image_get_active_layer (gdisp->gimage));
  gdisplays_flush ();
}

void
layers_add_alpha_channel_cmd_callback (GtkWidget *widget,
				       gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  gimp_layer_add_alpha (gimp_image_get_active_layer (gdisp->gimage));
  gdisplays_flush ();
}

void
layers_alpha_select_cmd_callback (GtkWidget *widget,
				  gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  gimage_mask_layer_alpha (gdisp->gimage,
			   gimp_image_get_active_layer (gdisp->gimage));
  gdisplays_flush ();
}

void
layers_resize_to_image_cmd_callback (GtkWidget *widget,
				     gpointer   client_data)
{
  GDisplay *gdisp;
  return_if_no_display (gdisp);

  gimp_layer_resize_to_image (gimp_image_get_active_layer (gdisp->gimage));
  gdisplays_flush ();
}

/*****  Tools  *****/

void
tools_toolbox_raise_cmd_callback (GtkWidget *widget,
				  gpointer   client_data)
{
  toolbox_raise ();
}

void
tools_default_colors_cmd_callback (GtkWidget *widget,
				   gpointer   client_data)
{
  gimp_context_set_default_colors (gimp_context_get_user ());
}

void
tools_swap_colors_cmd_callback (GtkWidget *widget,
				gpointer   client_data)
{
  gimp_context_swap_colors (gimp_context_get_user ());
}

void
tools_swap_contexts_cmd_callback (GtkWidget *widget,
				  gpointer   client_data)
{
  static GimpContext *swap_context = NULL;
  static GimpContext *temp_context = NULL;

  if (! swap_context)
    {
      swap_context = gimp_context_new ("Swap Context",
				       gimp_context_get_user ());
      temp_context = gimp_context_new ("Temp Context", NULL);
    }

  gimp_context_copy_args (gimp_context_get_user (),
			  temp_context,
			  GIMP_CONTEXT_ALL_ARGS_MASK);
  gimp_context_copy_args (swap_context,
			  gimp_context_get_user (),
			  GIMP_CONTEXT_ALL_ARGS_MASK);
  gimp_context_copy_args (temp_context,
			  swap_context,
			  GIMP_CONTEXT_ALL_ARGS_MASK);
}

void
tools_select_cmd_callback (GtkWidget *widget,
			   gpointer   callback_data,
			   guint      callback_action)
{
  GtkType       tool_type;
  GimpToolInfo *tool_info;
  GDisplay     *gdisp;

  tool_type = callback_action;

  tool_info = tool_manager_get_info_by_type (tool_type);
  gdisp     = gdisplay_active ();

  gimp_context_set_tool (gimp_context_get_user (), tool_info);

#warning FIXME (let the tool manager to this stuff)

  /*  Paranoia  */
  active_tool->drawable = NULL;

  /*  Complete the initialisation by doing the same stuff
   *  tools_initialize() does after it did what tools_select() does
   */
  if (GIMP_TOOL_CLASS (GTK_OBJECT (active_tool)->klass)->initialize)
    {
      gimp_tool_initialize (active_tool, gdisp);

      active_tool->drawable = gimp_image_active_drawable (gdisp->gimage);
    }

  /*  setting the tool->gdisp here is a HACK to allow the tools'
   *  dialog windows being hidden if the tool was selected from
   *  a tear-off-menu and there was no mouse click in the display
   *  before deleting it
   */
  active_tool->gdisp = gdisp;
}

/*****  Filters  *****/

void
filters_repeat_cmd_callback (GtkWidget *widget,
			     gpointer   callback_data,
			     guint      callback_action)
{
  plug_in_repeat (callback_action);
}

/*****  Dialogs  ******/

void
dialogs_preferences_cmd_callback (GtkWidget *widget,
				  gpointer   client_data)
{
  preferences_dialog_create ();
}

void
dialogs_lc_cmd_callback (GtkWidget *widget,
			 gpointer   client_data)
{
  GDisplay *gdisp;
  gdisp = gdisplay_active ();

  lc_dialog_create (gdisp ? gdisp->gimage : NULL);
}

void
dialogs_tool_options_cmd_callback (GtkWidget *widget,
				   gpointer   client_data)
{
  tool_options_dialog_show ();
}

void
dialogs_brushes_cmd_callback (GtkWidget *widget,
			      gpointer   client_data)
{
  brush_dialog_create ();
}

void
dialogs_patterns_cmd_callback (GtkWidget *widget,
			       gpointer   client_data)
{
  pattern_dialog_create ();
}

void
dialogs_gradients_cmd_callback (GtkWidget *widget,
				gpointer   client_data)
{
  gradient_dialog_create ();
}

void
dialogs_palette_cmd_callback (GtkWidget *widget,
			      gpointer   client_data)
{
  palette_dialog_create ();
}

static void
dialogs_indexed_palette_select_callback (GimpColormapDialog *dialog,
					 gpointer            data)
{
  GimpImage *image;
  GimpRGB    color;
  gint       index;

  image = gimp_colormap_dialog_image (dialog);
  index = gimp_colormap_dialog_col_index (dialog);

  gimp_rgba_set_uchar (&color,
		       image->cmap[index * 3],
		       image->cmap[index * 3 + 1],
		       image->cmap[index * 3 + 2],
		       255);

  if (active_color == FOREGROUND)
    gimp_context_set_foreground (gimp_context_get_user (), &color);
  else if (active_color == BACKGROUND)
    gimp_context_set_background (gimp_context_get_user (), &color);
}

void
dialogs_indexed_palette_cmd_callback (GtkWidget *widget,
				      gpointer   client_data)
{
  static GimpColormapDialog *cmap_dlg;

  if (!cmap_dlg)
    {
      cmap_dlg = gimp_colormap_dialog_create (image_context);

      gtk_signal_connect
	(GTK_OBJECT (cmap_dlg), "selected",
	 GTK_SIGNAL_FUNC (dialogs_indexed_palette_select_callback),
	 NULL);
    }

  if (! GTK_WIDGET_VISIBLE (cmap_dlg))
    gtk_widget_show (GTK_WIDGET (cmap_dlg));
  else
    gdk_window_raise (GTK_WIDGET (cmap_dlg)->window);
}

void
dialogs_input_devices_cmd_callback (GtkWidget *widget,
				    gpointer   client_data)
{
  input_dialog_create ();
}

void
dialogs_device_status_cmd_callback (GtkWidget *widget,
				    gpointer   client_data)
{
  device_status_create ();
}

void
dialogs_document_index_cmd_callback (GtkWidget *widget,
				     gpointer   client_data) 
{
  document_index_create ();
}

void
dialogs_error_console_cmd_callback (GtkWidget *widget,
				    gpointer   client_data) 
{
  error_console_add (NULL);
}

#ifdef DISPLAY_FILTERS
void
dialogs_display_filters_cmd_callback (GtkWidget *widget,
				      gpointer   client_data)
{
  GDisplay *gdisp;

  gdisp = gdisplay_active ();
  if (!gdisp)
    gdisp = color_area_gdisp;

  if (!gdisp->cd_ui)
    gdisplay_color_ui_new (gdisp);

  if (!GTK_WIDGET_VISIBLE (gdisp->cd_ui))
    gtk_widget_show (gdisp->cd_ui);
  else
    gdk_window_raise (gdisp->cd_ui->window);
}
#endif /* DISPLAY_FILTERS */

void
dialogs_undo_history_cmd_callback (GtkWidget *widget,
				   gpointer   client_data)
{
  GDisplay *gdisp;
  GImage   *gimage;
  return_if_no_display (gdisp);

  gimage = gdisp->gimage;

  if (!gimage->undo_history)
    gimage->undo_history = undo_history_new (gimage);

  if (!GTK_WIDGET_VISIBLE (gimage->undo_history))
    gtk_widget_show (gimage->undo_history);
  else
    gdk_window_raise (gimage->undo_history->window);
}

void
dialogs_module_browser_cmd_callback (GtkWidget *widget,
				     gpointer   client_data)
{
  GtkWidget *module_browser;

  module_browser = module_db_browser_new ();
  gtk_widget_show (module_browser);
}


/*****  Help  *****/

void
help_help_cmd_callback (GtkWidget *widget,
			gpointer   client_data)
{
  gimp_standard_help_func (NULL);
}

void
help_context_help_cmd_callback (GtkWidget *widget,
				gpointer   client_data)
{
  gimp_context_help ();
}

void
help_tips_cmd_callback (GtkWidget *widget,
			gpointer   client_data)
{
  tips_dialog_create ();
}

void
help_about_cmd_callback (GtkWidget *widget,
			 gpointer   client_data)
{
  about_dialog_create ();
}


/*****************************/
/*****  Local functions  *****/
/*****************************/

static void
image_resize_callback (GtkWidget *widget,
		       gpointer   client_data)
{
  ImageResize *image_resize;
  GImage      *gimage;

  image_resize = (ImageResize *) client_data;

  gtk_widget_set_sensitive (image_resize->resize->resize_shell, FALSE);

  if ((gimage = image_resize->gimage) != NULL)
    {
      if (image_resize->resize->width > 0 &&
	  image_resize->resize->height > 0) 
	{
	  gimp_image_resize (gimage,
			     image_resize->resize->width,
			     image_resize->resize->height,
			     image_resize->resize->offset_x,
			     image_resize->resize->offset_y);
	  gdisplays_flush ();
	  lc_dialog_update_image_list ();
	}
      else 
	{
	  g_message (_("Resize Error: Both width and height must be "
		       "greater than zero."));
	  return;
	}
    }

  gtk_widget_destroy (image_resize->resize->resize_shell);
}

static void
image_scale_callback (GtkWidget *widget,
		      gpointer   client_data)
{
  ImageResize *image_scale = (ImageResize *) client_data;

  g_assert (image_scale != NULL);
  g_assert (image_scale->gimage != NULL);

  gtk_widget_set_sensitive (image_scale->resize->resize_shell, FALSE);

  if (resize_check_layer_scaling (image_scale))
    {
      resize_scale_implement (image_scale);
      gtk_widget_destroy (image_scale->resize->resize_shell);
    }
}

static void
gimage_mask_feather_callback (GtkWidget *widget,
			      gdouble    size,
			      GimpUnit   unit,
			      gpointer   data)
{
  GImage  *gimage;
  gdouble  radius_x;
  gdouble  radius_y;

  gimage = GIMP_IMAGE (data);

  selection_feather_radius = size;

  radius_x = radius_y = selection_feather_radius;

  if (unit != GIMP_UNIT_PIXEL)
    {
      gdouble factor;

      factor = (MAX (gimage->xresolution, gimage->yresolution) /
		MIN (gimage->xresolution, gimage->yresolution));

      if (gimage->xresolution == MIN (gimage->xresolution, gimage->yresolution))
	radius_y *= factor;
      else
	radius_x *= factor;
    }

  gimage_mask_feather (gimage, radius_x, radius_y);
  gdisplays_flush ();
}

static void
gimage_mask_border_callback (GtkWidget *widget,
			     gdouble    size,
			     GimpUnit   unit,
			     gpointer   data)
{
  GImage  *gimage;
  gdouble  radius_x;
  gdouble  radius_y;

  gimage = GIMP_IMAGE (data);

  selection_border_radius = ROUND (size);

  radius_x = radius_y = selection_border_radius;

  if (unit != GIMP_UNIT_PIXEL)
    {
      gdouble factor;

      factor = (MAX (gimage->xresolution, gimage->yresolution) /
		MIN (gimage->xresolution, gimage->yresolution));

      if (gimage->xresolution == MIN (gimage->xresolution, gimage->yresolution))
	radius_y *= factor;
      else
	radius_x *= factor;
    }

  gimage_mask_border (gimage, radius_x, radius_y);
  gdisplays_flush ();
}

static void
gimage_mask_grow_callback (GtkWidget *widget,
			   gdouble    size,
			   GimpUnit   unit,
			   gpointer   data)
{
  GImage  *gimage;
  gdouble  radius_x;
  gdouble  radius_y;

  gimage = GIMP_IMAGE (data);

  selection_grow_pixels = ROUND (size);

  radius_x = radius_y = selection_grow_pixels;

  if (unit != GIMP_UNIT_PIXEL)
    {
      gdouble factor;

      factor = (MAX (gimage->xresolution, gimage->yresolution) /
		MIN (gimage->xresolution, gimage->yresolution));

      if (gimage->xresolution == MIN (gimage->xresolution, gimage->yresolution))
	radius_y *= factor;
      else
	radius_x *= factor;
    }

  gimage_mask_grow (gimage, radius_x, radius_y);
  gdisplays_flush ();
}

static void
gimage_mask_shrink_callback (GtkWidget *widget,
			     gdouble    size,
			     GimpUnit   unit,
			     gpointer   data)
{
  GImage *gimage;
  gint    radius_x;
  gint    radius_y;

  gimage = GIMP_IMAGE (data);

  selection_shrink_pixels = ROUND (size);

  radius_x = radius_y = selection_shrink_pixels;

  selection_shrink_edge_lock =
    ! GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (widget),
					      "edge_lock_toggle"))->active;

  if (unit != GIMP_UNIT_PIXEL)
    {
      gdouble factor;

      factor = (MAX (gimage->xresolution, gimage->yresolution) /
		MIN (gimage->xresolution, gimage->yresolution));

      if (gimage->xresolution == MIN (gimage->xresolution, gimage->yresolution))
	radius_y *= factor;
      else
	radius_x *= factor;
    }

  gimage_mask_shrink (gimage, radius_x, radius_y, selection_shrink_edge_lock);
  gdisplays_flush ();
}
