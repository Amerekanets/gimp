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

#ifndef __DISP_CALLBACKS_H__
#define __DISP_CALLBACKS_H__


#define CANVAS_EVENT_MASK (GDK_EXPOSURE_MASK            | \
			   GDK_POINTER_MOTION_MASK      | \
			   GDK_POINTER_MOTION_HINT_MASK | \
			   GDK_BUTTON_PRESS_MASK        | \
			   GDK_BUTTON_RELEASE_MASK      | \
			   GDK_STRUCTURE_MASK           | \
			   GDK_ENTER_NOTIFY_MASK        | \
			   GDK_LEAVE_NOTIFY_MASK        | \
			   GDK_KEY_PRESS_MASK           | \
			   GDK_KEY_RELEASE_MASK         | \
			   GDK_PROXIMITY_OUT_MASK)


gboolean   gdisplay_shell_events        (GtkWidget      *widget,
					 GdkEvent       *event,
					 GDisplay       *gdisp);
gboolean   gdisplay_canvas_events       (GtkWidget      *widget,
					 GdkEvent       *event);

gboolean   gdisplay_hruler_button_press (GtkWidget      *widget,
					 GdkEventButton *bevent,
					 gpointer        dtata);
gboolean   gdisplay_vruler_button_press (GtkWidget      *widget,
					 GdkEventButton *bevent,
					 gpointer        data);
gboolean   gdisplay_origin_button_press (GtkWidget      *widget,
					 GdkEventButton *bevent,
					 gpointer        data);

void       gdisplay_drop_drawable       (GtkWidget      *widget,
					 GimpViewable   *viewable,
					 gpointer        data);
void       gdisplay_drop_pattern        (GtkWidget      *widget,
					 GimpViewable   *viewable,
					 gpointer        data);
void       gdisplay_drop_color          (GtkWidget      *widget,
					 const GimpRGB  *color,
					 gpointer        data);
void       gdisplay_drop_buffer         (GtkWidget      *widget,
					 GimpViewable   *viewable,
					 gpointer        data);


#endif /* __DISP_CALLBACKS_H__ */
