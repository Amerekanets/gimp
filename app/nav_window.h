/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 * Copyright (C) 1999 Andy Thomas alt@gimp.org
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

#ifndef __NAV_WINDOW_H__
#define __NAV_WINDOW_H__


InfoDialog * nav_window_create                (gpointer        );
void         nav_window_free                  (GDisplay       *gdisp,
					       InfoDialog     *idialog);
void         nav_window_update_window_marker  (InfoDialog     *idialog);
void         nav_dialog_popup                 (InfoDialog     *idialog);
void         nav_window_preview_resized       (InfoDialog     *idialog);
void         nav_window_popup_preview_resized (GtkWidget     **widget);
void         nav_window_follow_auto           (void);

/* popup functions */
void         nav_popup_click_handler          (GtkWidget      *widget,
					       GdkEventButton *event,
					       gpointer        data);
void         nav_popup_free                   (GtkWidget      *widget);


#endif /*  __NAV_WINDOW_H__  */
