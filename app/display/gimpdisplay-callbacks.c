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

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "libgimpcolor/gimpcolor.h"

#include "core/core-types.h"
#include "tools/tools-types.h"

#include "base/pixel-region.h"
#include "base/temp-buf.h"
#include "base/tile-manager.h"

#include "paint-funcs/paint-funcs.h"

#include "core/gimp.h"
#include "core/gimpbuffer.h"
#include "core/gimpcontext.h"
#include "core/gimpedit.h"
#include "core/gimpimage.h"
#include "core/gimplayer.h"
#include "core/gimppattern.h"
#include "core/gimptoolinfo.h"

#include "tools/gimpbucketfilltool.h"
#include "tools/gimpfuzzyselecttool.h"
#include "tools/gimpmovetool.h"
#include "tools/tool_manager.h"

#include "widgets/gimpwidgets-utils.h"

#include "gui/info-window.h"
#include "gui/layer-select.h"

#include "appenv.h"
#include "app_procs.h"
#include "devices.h"
#include "dialog_handler.h"
#include "disp_callbacks.h"
#include "drawable.h"
#include "gdisplay.h"
#include "gimprc.h"
#include "scale.h"
#include "scroll.h"
#include "selection.h"
#include "undo.h"

#include "libgimp/gimpintl.h"


static void
gdisplay_redraw (GDisplay *gdisp,
		 gint      x,
		 gint      y,
		 gint      w,
		 gint      h)
{
  glong x1, y1, x2, y2;    /*  coordinate of rectangle corners  */

  x1 = x;
  y1 = y;
  x2 = (x + w);
  y2 = (y + h);

  x1 = CLAMP (x1, 0, gdisp->disp_width);
  y1 = CLAMP (y1, 0, gdisp->disp_height);
  x2 = CLAMP (x2, 0, gdisp->disp_width);
  y2 = CLAMP (y2, 0, gdisp->disp_height);

  if ((x2 - x1) && (y2 - y1))
    {
      gdisplay_expose_area (gdisp,
			    x1, y1,
			    (x2 - x1), (y2 - y1));
      gdisplay_flush_displays_only (gdisp);
    }
}

static void
gdisplay_check_device_cursor (GDisplay *gdisp)
{
  GList *list;

  /*  gdk_input_list_devices returns an internal list, so we shouldn't
   *  free it afterwards
   */
  for (list = gdk_input_list_devices(); list; list = g_list_next (list))
    {
      GdkDeviceInfo *info = (GdkDeviceInfo *) list->data;

      if (info->deviceid == current_device)
	{
	  gdisp->draw_cursor = !info->has_cursor;
	  break;
	}
    }
}

static int
key_to_state (gint key)
{
  switch (key)
    {
    case GDK_Alt_L:
    case GDK_Alt_R:
      return GDK_MOD1_MASK;
    case GDK_Shift_L:
    case GDK_Shift_R:
      return GDK_SHIFT_MASK;
    case GDK_Control_L:
    case GDK_Control_R:
      return GDK_CONTROL_MASK;
    default:
      return 0;
    }
}

static void
gdisplay_vscrollbar_update (GtkAdjustment *adjustment,
			    GDisplay      *gdisp)
{
  scroll_display (gdisp, 0, (adjustment->value - gdisp->offset_y));
}

static void
gdisplay_hscrollbar_update (GtkAdjustment *adjustment,
			    GDisplay      *gdisp)
{
  scroll_display (gdisp, (adjustment->value - gdisp->offset_x), 0);
}

gint
gdisplay_shell_events (GtkWidget *widget,
		       GdkEvent  *event,
		       GDisplay  *gdisp)
{
  switch (event->type)
    {
    case GDK_KEY_PRESS:
    case GDK_BUTTON_PRESS:
      /*  Setting the context's display automatically sets the image, too  */
      gimp_context_set_display (gimp_get_user_context (gdisp->gimage->gimp),
				gdisp);

      break;
    default:
      break;
    }

  return FALSE;
}

gint
gdisplay_canvas_events (GtkWidget *canvas,
			GdkEvent  *event)
{
  GDisplay        *gdisp;
  GdkEventExpose  *eevent;
  GdkEventMotion  *mevent;
  GdkEventButton  *bevent;
  GdkEventKey     *kevent;
  gdouble          tx            = 0;
  gdouble          ty            = 0;
  guint            state         = 0;
  gint             return_val    = FALSE;
  static gboolean  scrolled      = FALSE;
  static guint     key_signal_id = 0;
  gboolean         update_cursor = FALSE;

  gdisp = (GDisplay *) gtk_object_get_user_data (GTK_OBJECT (canvas));

  if (!canvas->window)
    return FALSE;

  /*  If this is the first event...  */
  if (!gdisp->select)
    {
      /*  create the selection object  */
      gdisp->select = selection_create (gdisp->canvas->window, gdisp,
					gdisp->gimage->height,
					gdisp->gimage->width, 
					gimprc.marching_speed);

      gdisp->disp_width = gdisp->canvas->allocation.width;
      gdisp->disp_height = gdisp->canvas->allocation.height;

      /*  create GC for scrolling  */
      gdisp->scroll_gc = gdk_gc_new (gdisp->canvas->window);
      gdk_gc_set_exposures (gdisp->scroll_gc, TRUE);

      /*  set up the scrollbar observers  */
      gtk_signal_connect (GTK_OBJECT (gdisp->hsbdata), "value_changed",
			  GTK_SIGNAL_FUNC (gdisplay_hscrollbar_update),
			  gdisp);
      gtk_signal_connect (GTK_OBJECT (gdisp->vsbdata), "value_changed",
			  GTK_SIGNAL_FUNC (gdisplay_vscrollbar_update),
			  gdisp);

      /*  setup scale properly  */
      setup_scale (gdisp);
    }

  /*  Find out what device the event occurred upon  */
  if (! gimp_busy && devices_check_change (event))
    gdisplay_check_device_cursor (gdisp);

  switch (event->type)
    {
    case GDK_EXPOSE:
      eevent = (GdkEventExpose *) event;
      /*printf(" EXP:%d,%d(%dx%d) ",eevent->area.x, eevent->area.y,
	eevent->area.width, eevent->area.height);fflush(stdout);*/
      gdisplay_redraw (gdisp,
		       eevent->area.x, eevent->area.y,
		       eevent->area.width, eevent->area.height);
      break;

    case GDK_CONFIGURE:
      /*printf(" CNF ");fflush(stdout);*/
      if ((gdisp->disp_width != gdisp->canvas->allocation.width) ||
	  (gdisp->disp_height != gdisp->canvas->allocation.height))
	{
	  gdisp->disp_width = gdisp->canvas->allocation.width;
	  gdisp->disp_height = gdisp->canvas->allocation.height;
	  resize_display (gdisp, FALSE, FALSE);
	}
      break;

    case GDK_LEAVE_NOTIFY:
      if (((GdkEventCrossing *) event)->mode != GDK_CROSSING_NORMAL)
	return TRUE;
      gdisplay_update_cursor (gdisp, 0, 0);
      gtk_label_set_text (GTK_LABEL (gdisp->cursor_label), "");
      info_window_update_extended (gdisp, -1, -1);

    case GDK_PROXIMITY_OUT:
      gdisp->proximity = FALSE;
      break;

    case GDK_ENTER_NOTIFY:
      if (((GdkEventCrossing *) event)->mode != GDK_CROSSING_NORMAL)
	return TRUE;
      /* Actually, should figure out tx,ty here */
      break;

    case GDK_BUTTON_PRESS:
      bevent = (GdkEventButton *) event;
      state = bevent->state;

      /*  ignore new mouse events  */
      if (gimp_busy)
	return TRUE;

      switch (bevent->button)
	{
	case 1:
	  state |= GDK_BUTTON1_MASK;
	  gtk_grab_add (canvas);

	  /* This is a hack to prevent other stuff being run in the middle of
	   * a tool operation (like changing image types.... brrrr). We just
	   * block all the keypress event. A better solution is to implement
	   * some sort of locking for images.
	   * Note that this is dependent on specific GTK behavior, and isn't
	   * guaranteed to work in future versions of GTK.
	   * -Yosh
	   */

	   
	  if (key_signal_id == 0)
	    key_signal_id = gtk_signal_connect (GTK_OBJECT (canvas),
						"key_press_event",
						GTK_SIGNAL_FUNC (gtk_true),
						NULL);

/* FIXME!!! This code is ugly, and active_tool shouldn't be referenced
 * directly
 */

	  if (active_tool && (GIMP_IS_MOVE_TOOL (active_tool) ||
			      ! gimp_image_is_empty (gdisp->gimage)))
	    {
	      if (active_tool->auto_snap_to)
		{
		  gdisplay_snap_point (gdisp, bevent->x, bevent->y, &tx, &ty);
		  bevent->x = tx;
		  bevent->y = ty;
		  update_cursor = TRUE;
		}

	      /* reset the current tool if ... */
	      if ((/* it has no drawable */
		   ! active_tool->drawable ||

		   /* or a drawable different from the current one */
		   (gimp_image_active_drawable (gdisp->gimage) !=
		    active_tool->drawable)) &&

		  /* and doesn't want to be preserved across drawable changes */
		  ! active_tool->preserve)
		{
		  tool_manager_initialize_tool (gdisp->gimage->gimp,
						active_tool, gdisp);
		}

	      /* otherwise set it's drawable if it has none */
	      else if (! active_tool->drawable)
		{
		  active_tool->drawable =
		    gimp_image_active_drawable (gdisp->gimage);
		}
	      
	      gimp_tool_button_press (active_tool, bevent, gdisp);
	    } 
	  break;

	case 2:
	  state |= GDK_BUTTON2_MASK;
	  scrolled = TRUE;
	  gtk_grab_add (canvas);
	  start_grab_and_scroll (gdisp, bevent);
	  break;

	case 3:
	  state |= GDK_BUTTON3_MASK;
	  {
	    gint x, y;

	    gimp_menu_position (GTK_MENU (gdisp->ifactory->widget), &x, &y);

	    gtk_item_factory_popup_with_data (gdisp->ifactory,
					      gdisp->gimage, NULL,
					      x, y,
					      3, bevent->time);
	  }
	  return_val = TRUE;
	  break;

	  /*  wheelmouse support  */
	case 4:
	  state |= GDK_BUTTON4_MASK;
	  if (state & GDK_SHIFT_MASK)
	    {
	      change_scale (gdisp, GIMP_ZOOM_IN);
	    }
	  else
	    {
	      GtkAdjustment *adj =
		(state & GDK_CONTROL_MASK) ? gdisp->hsbdata : gdisp->vsbdata;
	      gfloat new_value = adj->value - adj->page_increment / 2;
	      new_value =
		CLAMP (new_value, adj->lower, adj->upper - adj->page_size);
	      gtk_adjustment_set_value (adj, new_value);
	    }
	  return_val = TRUE;
	  break;

	case 5:
	  state |= GDK_BUTTON5_MASK;
	  if (state & GDK_SHIFT_MASK)
	    {
	      change_scale (gdisp, GIMP_ZOOM_OUT);
	    }
	  else
	    {
	      GtkAdjustment *adj =
		(state & GDK_CONTROL_MASK) ? gdisp->hsbdata : gdisp->vsbdata;
	      gfloat new_value = adj->value + adj->page_increment / 2;
	      new_value = CLAMP (new_value,
				 adj->lower, adj->upper - adj->page_size);
	      gtk_adjustment_set_value (adj, new_value);
	    }
	  return_val = TRUE;
	  break;

	default:
	  break;
	}
      break;

    case GDK_BUTTON_RELEASE:
      bevent = (GdkEventButton *) event;
      state = bevent->state;

      /*  ugly side condition: all operations which set busy cursors are
       *  invoked on BUTTON_RELEASE, thus no new BUTTON_PRESS events are
       *  accepted while Gimp is busy, thus it should be safe to block
       *  BUTTON_RELEASE.  --Mitch
       *
       *  ugly: fuzzy_select sets busy cursors while ACTIVE.
       */
      if (gimp_busy && ! (GIMP_IS_FUZZY_SELECT_TOOL (active_tool)  &&
	                  active_tool->state == ACTIVE))
	return TRUE;

      switch (bevent->button)
	{
	case 1:
	  state &= ~GDK_BUTTON1_MASK;

	  /* Lame hack. See above */
	  if (key_signal_id)
	    {
	      gtk_signal_disconnect (GTK_OBJECT (canvas), key_signal_id);
	      key_signal_id = 0;
	    }

	  gtk_grab_remove (canvas);
	  gdk_pointer_ungrab (bevent->time);  /* fixes pointer grab bug */

	  if (active_tool && (GIMP_IS_MOVE_TOOL (active_tool) ||
			      ! gimp_image_is_empty (gdisp->gimage)))
	    {
	      if (active_tool->state == ACTIVE)
		{
		  if (active_tool->auto_snap_to)
		    {
		      gdisplay_snap_point (gdisp, bevent->x, bevent->y, &tx, &ty);
		      bevent->x = tx;
		      bevent->y = ty;
		      update_cursor = TRUE;
		    }
		  
		  gimp_tool_button_release (active_tool, bevent, gdisp);
		}
	    }
	  break;

	case 2:
	  state &= ~GDK_BUTTON2_MASK;
	  scrolled = FALSE;
	  gtk_grab_remove (canvas);
	  end_grab_and_scroll (gdisp, bevent);
	  break;

	case 3:
	  state &= ~GDK_BUTTON3_MASK;
	  break;

	  /*  wheelmouse support  */
	case 4:
	  state &= ~GDK_BUTTON4_MASK;
	  return_val = TRUE;
	  break;

	case 5:
	  state &= ~GDK_BUTTON5_MASK;
	  return_val = TRUE;
	  break;

	default:
	  break;
	}
      break;

    case GDK_MOTION_NOTIFY:
      mevent = (GdkEventMotion *) event;
      state = mevent->state;

      /*  for the same reason we block BUTTON_RELEASE,
       *  we block MOTION_NOTIFY.  --Mitch
       *
       *  ugly: fuzzy_select sets busy cursors while ACTIVE.
       */
      if (gimp_busy && ! (GIMP_IS_FUZZY_SELECT_TOOL (active_tool) &&
			  active_tool->state == ACTIVE))
	return TRUE;

      /* Ask for the pointer position, but ignore it except for cursor
       * handling, so motion events sync with the button press/release events 
       */
      if (mevent->is_hint)
	{
	  gdk_input_window_get_pointer (canvas->window, current_device, &tx, &ty,
					NULL, NULL, NULL, NULL);
	}
      else
	{
	  tx = mevent->x;
	  ty = mevent->y;
	}
      update_cursor = TRUE;

      if (!gdisp->proximity)
	{
	  gdisp->proximity = TRUE;
	  gdisplay_check_device_cursor (gdisp);
	}

      if (active_tool && (GIMP_IS_MOVE_TOOL(active_tool) ||
			  ! gimp_image_is_empty (gdisp->gimage)) &&
	  (mevent->state & GDK_BUTTON1_MASK))
	{
	  if (active_tool->state == ACTIVE)
	    {
	      /*  if the first mouse button is down, check for automatic
	       *  scrolling...
	       */
	      if ((mevent->state & GDK_BUTTON1_MASK) &&
		  !active_tool->scroll_lock)
		{
		  if (mevent->x < 0 || mevent->y < 0 ||
		      mevent->x > gdisp->disp_width ||
		      mevent->y > gdisp->disp_height)
		    scroll_to_pointer_position (gdisp, mevent);
		}

	      if (active_tool->auto_snap_to)
		{
		  gdisplay_snap_point (gdisp, mevent->x, mevent->y, &tx, &ty);
		  mevent->x = tx;
		  mevent->y = ty;
		  update_cursor = TRUE;
		}

	      gimp_tool_motion (active_tool, mevent, gdisp);
	    }
	}
      else if ((mevent->state & GDK_BUTTON2_MASK) && scrolled)
	{
	  grab_and_scroll (gdisp, mevent);
	}

      /* Operator update support: Bug #XXXX */

      if (/* Should we have a tool...      */
	  active_tool && 
	  /* and this event is NOT driving */
	  /* button press handlers ...     */
	  !(state & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK)))
        {
          /* ...then preconditions to modify a tool */ 
          /* operator state have been met.          */
          gimp_tool_oper_update (active_tool, mevent, gdisp);
        }

      break;

    case GDK_KEY_PRESS:
      kevent = (GdkEventKey *) event;
      state = kevent->state;

      /*  ignore any key presses  */
      if (gimp_busy)
	return TRUE;

      switch (kevent->keyval)
	{
	case GDK_Left: case GDK_Right:
	case GDK_Up: case GDK_Down:
	  if (active_tool && ! gimp_image_is_empty (gdisp->gimage))
	    gimp_tool_arrow_key (active_tool, kevent, gdisp);

	  return_val = TRUE;
	  break;

	case GDK_Tab:
	  if (kevent->state & GDK_MOD1_MASK &&
	      !gimp_image_is_empty (gdisp->gimage))
	    layer_select_init (gdisp->gimage, 1, kevent->time);

	  if (kevent->state & GDK_CONTROL_MASK &&
	      !gimp_image_is_empty (gdisp->gimage))
	    layer_select_init (gdisp->gimage, -1, kevent->time);

	  /* Hide or show all dialogs */
	  if (!kevent->state)
	    dialog_toggle();

	  return_val = TRUE;
	  break;

	  /*  Update the state based on modifiers being pressed  */
	case GDK_Alt_L: case GDK_Alt_R:
	case GDK_Shift_L: case GDK_Shift_R:
	case GDK_Control_L: case GDK_Control_R:
	  state |= key_to_state (kevent->keyval);
	  /* For all modifier keys: call the tools modifier_key_func */
	  if (active_tool && !gimp_image_is_empty (gdisp->gimage))
	    {
	      gdk_input_window_get_pointer (canvas->window, current_device,
					    &tx, &ty, NULL, NULL, NULL, NULL);
	      gimp_tool_modifier_key (active_tool, kevent, gdisp);
	      return_val = TRUE;
	    }
	  break;
	}
      break;

    case GDK_KEY_RELEASE:
      kevent = (GdkEventKey *) event;
      state = kevent->state;

      /*  ignore any key releases  */
      if (gimp_busy)
	return TRUE;

      switch (kevent->keyval)
	{
	case GDK_Alt_L: case GDK_Alt_R:
	case GDK_Shift_L: case GDK_Shift_R:
	case GDK_Control_L: case GDK_Control_R:
	  state &= ~key_to_state (kevent->keyval);
	  /* For all modifier keys: call the tools modifier_key_func */
	  if (active_tool && !gimp_image_is_empty (gdisp->gimage))
	    {
	      gdk_input_window_get_pointer (canvas->window, current_device,
	                                    &tx, &ty, NULL, NULL, NULL, NULL);
	      gimp_tool_modifier_key (active_tool, kevent, gdisp);
	      return_val = TRUE;
	    }
	  break;
	}

      return_val = TRUE;
      break;

    default:
      break;
    }

  /*  if we reached this point in gimp_busy mode, return now  */
  if (gimp_busy)
    return TRUE;

  /* Cursor update support                               */
  /* no_cursor_updating is TRUE (=1) when                */
  /* <Toolbox>/File/Preferences.../Interface/...         */
  /* Image Windows/Disable Cursor Updating is TOGGLED ON */
 
  if (gimprc.no_cursor_updating == 0)
    {
      if (active_tool && !gimp_image_is_empty (gdisp->gimage) &&
	  !(state & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK)))
	{
	  GdkEventMotion me;
	  me.x = tx;  me.y = ty;
	  me.state = state;
	  gimp_tool_cursor_update (active_tool, &me, gdisp);
	}
      else if (gimp_image_is_empty (gdisp->gimage))
	{
	  gdisplay_install_tool_cursor (gdisp,
					GIMP_BAD_CURSOR,
					GIMP_TOOL_CURSOR_NONE,
					GIMP_CURSOR_MODIFIER_NONE);
	}
    }

  if (update_cursor)
    gdisplay_update_cursor (gdisp, tx, ty);

  return return_val;
}

gint
gdisplay_hruler_button_press (GtkWidget      *widget,
			      GdkEventButton *event,
			      gpointer        data)
{
  GDisplay *gdisp;

  if (gimp_busy)
    return TRUE;

  if (event->button == 1)
    {
      gdisp = data;

      gimp_context_set_tool (gimp_get_user_context (gdisp->gimage->gimp),
			     tool_manager_get_info_by_type (gdisp->gimage->gimp, 
							    GIMP_TYPE_MOVE_TOOL));

      gimp_move_tool_start_hguide (active_tool, gdisp);
      gtk_grab_add (gdisp->canvas);
    }

  return FALSE;
}

gint
gdisplay_vruler_button_press (GtkWidget      *widget,
			      GdkEventButton *event,
			      gpointer        data)
{
  GDisplay *gdisp;

  if (gimp_busy)
    return TRUE;

  if (event->button == 1)
    {
      gdisp = data;

      gimp_context_set_tool (gimp_get_user_context (gdisp->gimage->gimp),
			     tool_manager_get_info_by_type (gdisp->gimage->gimp,
							    GIMP_TYPE_MOVE_TOOL));

      gimp_move_tool_start_vguide (active_tool, gdisp);
      gtk_grab_add (gdisp->canvas);
    }

  return FALSE;
}

static void
gdisplay_origin_menu_position (GtkMenu  *menu,
			       gint     *x,
			       gint     *y,
			       gpointer  data)
{
  GtkWidget *origin;
  gint origin_x;
  gint origin_y;

  origin = (GtkWidget *) data;

  gdk_window_get_origin (origin->window, &origin_x, &origin_y);

  *x = origin_x + origin->allocation.x + origin->allocation.width - 1;
  *y = origin_y + origin->allocation.y + (origin->allocation.height - 1) / 2;

  if (*x + GTK_WIDGET (menu)->allocation.width > gdk_screen_width ())
    *x -= (GTK_WIDGET (menu)->allocation.width + origin->allocation.width);

  if (*y + GTK_WIDGET (menu)->allocation.height > gdk_screen_height ())
    *y -= (GTK_WIDGET (menu)->allocation.height);
}

gint
gdisplay_origin_button_press (GtkWidget      *widget,
			      GdkEventButton *event,
			      gpointer        data)
{
  GDisplay *gdisp;

  gdisp = (GDisplay *) data;

  if (! gimp_busy && event->button == 1)
    {
      gint x, y;

      gdisplay_origin_menu_position (GTK_MENU (gdisp->ifactory->widget),
				     &x, &y, widget);

      gtk_item_factory_popup_with_data (gdisp->ifactory,
					gdisp->gimage, NULL,
					x, y,
					1, event->time);
    }

  /*  Stop the signal emission so the button doesn't grab the
   *  pointer away from us
   */
  gtk_signal_emit_stop_by_name (GTK_OBJECT (widget), "button_press_event");

  return FALSE;
}

void
gdisplay_drop_drawable (GtkWidget    *widget,
			GimpViewable *viewable,
			gpointer      data)
{
  GimpDrawable     *drawable;
  GDisplay         *gdisp;
  GimpImage        *src_gimage;
  GimpLayer        *new_layer;
  GimpImage        *dest_gimage;
  gint              src_width, src_height;
  gint              dest_width, dest_height;
  gint              off_x, off_y;
  TileManager      *tiles;
  PixelRegion       srcPR, destPR;
  guchar            bg[MAX_CHANNELS];
  gint              bytes; 
  GimpImageBaseType type;

  if (gimp_busy)
    return;

  drawable = GIMP_DRAWABLE (viewable);
  gdisp    = (GDisplay *) data;

  src_gimage = gimp_drawable_gimage (drawable);
  src_width  = gimp_drawable_width  (drawable);
  src_height = gimp_drawable_height (drawable);

  switch (gimp_drawable_type (drawable))
    {
    case RGB_GIMAGE: case RGBA_GIMAGE:
      bytes = 4; type = RGB;
      break;
    case GRAY_GIMAGE: case GRAYA_GIMAGE:
      bytes = 2; type = GRAY;
      break;
    case INDEXED_GIMAGE: case INDEXEDA_GIMAGE:
      bytes = 4; type = INDEXED;
      break;
    default:
      bytes = 3; type = RGB;
      break;
    }

  gimp_image_get_background (src_gimage, drawable, bg);

  tiles = tile_manager_new (src_width, src_height, bytes);

  pixel_region_init (&srcPR, gimp_drawable_data (drawable),
		     0, 0, src_width, src_height, FALSE);
  pixel_region_init (&destPR, tiles,
		     0, 0, src_width, src_height, TRUE);

  if (type == INDEXED)
    {
      /*  If the layer is indexed...we need to extract pixels  */
      extract_from_region (&srcPR, &destPR, NULL,
			   gimp_drawable_cmap (drawable), bg, type,
			   gimp_drawable_has_alpha (drawable), FALSE);
    }
  else if (bytes > srcPR.bytes)
    {
      /*  If the layer doesn't have an alpha channel, add one  */
      add_alpha_region (&srcPR, &destPR);
    }
  else
    {
      /*  Otherwise, do a straight copy  */
      copy_region (&srcPR, &destPR);
    }

  dest_gimage = gdisp->gimage;
  dest_width  = dest_gimage->width;
  dest_height = dest_gimage->height;

  undo_push_group_start (dest_gimage, EDIT_PASTE_UNDO);

  new_layer =
    gimp_layer_new_from_tiles (dest_gimage,
			       gimp_image_base_type_with_alpha (dest_gimage),
			       tiles, 
			       _("Pasted Layer"),
			       OPAQUE_OPACITY, NORMAL_MODE);

  tile_manager_destroy (tiles);

  if (new_layer)
    {
      gimp_drawable_set_gimage (GIMP_DRAWABLE (new_layer), dest_gimage);

      off_x = (dest_gimage->width - src_width) / 2;
      off_y = (dest_gimage->height - src_height) / 2;

      gimp_layer_translate (new_layer, off_x, off_y);

      gimp_image_add_layer (dest_gimage, new_layer, -1);

      undo_push_group_end (dest_gimage);

      gdisplays_flush ();

      gimp_context_set_display (gimp_get_user_context (gdisp->gimage->gimp),
				gdisp);
    }
}

static void
gdisplay_bucket_fill (GtkWidget      *widget,
		      BucketFillMode  fill_mode,
		      guchar          orig_color[],
		      TempBuf        *orig_pat_buf,
		      gpointer        data)
{
  GimpImage    *gimage;
  GimpDrawable *drawable;
  TileManager  *buf_tiles;
  PixelRegion   bufPR;
  GimpToolInfo *tool_info;
  GimpContext  *context;
  gint          x1, x2, y1, y2;
  gint          bytes;
  gboolean      has_alpha;

  guchar    color[3];
  TempBuf  *pat_buf = NULL;
  gboolean  new_buf = FALSE;

  if (gimp_busy)
    return;

  gimage = ((GDisplay *) data)->gimage;
  drawable = gimp_image_active_drawable (gimage);
  if (!drawable)
    return;

  gimp_set_busy ();

  /*  Get the bucket fill context  */
  tool_info = tool_manager_get_info_by_type (gimage->gimp,
					     GIMP_TYPE_BUCKET_FILL_TOOL);

  if (tool_info && tool_info->context)
    {
      context = tool_info->context;
    }
  else
    {
      context = gimp_get_user_context (gimage->gimp);
    }

  /*  Transform the passed data for the dest image  */
  if (fill_mode == FG_BUCKET_FILL)
    {
      gimp_image_transform_color (gimage, drawable, orig_color, color, RGB);
    }
  else
    {
      if (((orig_pat_buf->bytes == 3) && ! gimp_drawable_is_rgb (drawable)) ||
	  ((orig_pat_buf->bytes == 1) && ! gimp_drawable_is_gray (drawable)))
	{
	  guchar *d1, *d2;
	  gint size;

	  if ((orig_pat_buf->bytes == 1) && gimp_drawable_is_rgb (drawable))
	    pat_buf = temp_buf_new (orig_pat_buf->width, orig_pat_buf->height,
				    3, 0, 0, NULL);
	  else
	    pat_buf = temp_buf_new (orig_pat_buf->width, orig_pat_buf->height,
				    1, 0, 0, NULL);

          d1 = temp_buf_data (orig_pat_buf);
          d2 = temp_buf_data (pat_buf);

          size = orig_pat_buf->width * orig_pat_buf->height;
          while (size--)
            {
              gimp_image_transform_color (gimage, drawable, d1, d2,
					  (orig_pat_buf->bytes == 3) ? RGB : GRAY);
              d1 += orig_pat_buf->bytes;
              d2 += pat_buf->bytes;
            }

          new_buf = TRUE;
	}
      else
	{
	  pat_buf = orig_pat_buf;
	}
    }

  gimp_drawable_mask_bounds (drawable, &x1, &y1, &x2, &y2);
  bytes     = gimp_drawable_bytes (drawable);
  has_alpha = gimp_drawable_has_alpha (drawable);

  /*  Fill the region  */
  buf_tiles = tile_manager_new ((x2 - x1), (y2 - y1), bytes);
  pixel_region_init (&bufPR, buf_tiles, 0, 0, (x2 - x1), (y2 - y1), TRUE);
  bucket_fill_region (fill_mode, &bufPR, NULL,
		      color, pat_buf, x1, y1, has_alpha);

  /*  Apply it to the image  */
  pixel_region_init (&bufPR, buf_tiles, 0, 0, (x2 - x1), (y2 - y1), FALSE);
  gimp_image_apply_image (gimage, drawable, &bufPR, TRUE,
			  gimp_context_get_opacity (context) * 255,
			  gimp_context_get_paint_mode (context),
			  NULL, x1, y1);
  tile_manager_destroy (buf_tiles);

  /*  Update the displays  */
  drawable_update (drawable,
		   x1, y1,
		   (x2 - x1), (y2 - y1));
  gdisplays_flush ();

  if (new_buf)
    temp_buf_free (pat_buf);

  gimp_unset_busy ();
}

void
gdisplay_drop_pattern (GtkWidget    *widget,
		       GimpViewable *viewable,
		       gpointer      data)
{
  if (GIMP_IS_PATTERN (viewable))
    {
      gdisplay_bucket_fill (widget, PATTERN_BUCKET_FILL, NULL,
			    GIMP_PATTERN (viewable)->mask, data);
    }
}

void
gdisplay_drop_color (GtkWidget     *widget,
		     const GimpRGB *drop_color,
		     gpointer       data)
{
  guchar color[4];

  gimp_rgba_get_uchar (drop_color,
		       &color[0],
		       &color[1],
		       &color[2],
		       &color[3]);

  gdisplay_bucket_fill (widget, FG_BUCKET_FILL, color, NULL, data);
}

void
gdisplay_drop_buffer (GtkWidget    *widget,
		      GimpViewable *viewable,
		      gpointer      data)
{
  GimpBuffer *buffer;
  GDisplay   *gdisp;

  if (gimp_busy)
    return;

  buffer = GIMP_BUFFER (viewable);
  gdisp  = (GDisplay *) data;

  /* FIXME: popup a menu for selecting "Paste Into" */

  gimp_edit_paste (gdisp->gimage,
		   gimp_image_active_drawable (gdisp->gimage),
		   buffer->tiles,
		   FALSE);

  gdisplays_flush ();
}
