/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpdialog.h
 * Copyright (C) 2000 Michael Natterer <mitch@gimp.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GIMP_DIALOG_H__
#define __GIMP_DIALOG_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* For information look into the C source or the html documentation */


GtkWidget * gimp_dialog_new                 (const gchar        *title,
					     const gchar        *wmclass_name,
					     GimpHelpFunc        help_func,
					     const gchar        *help_data,
					     GtkWindowPosition   position,
					     gint                allow_shrink,
					     gint                allow_grow,
					     gint                auto_shrink,

					     /* specify action area buttons
					      * as va_list:
					      *  const gchar    *label,
					      *  GCallback       callback,
					      *  gpointer        callback_data,
					      *  GObject        *slot_object,
					      *  GtkWidget     **widget_ptr,
					      *  gboolean        default_action,
					      *  gboolean        connect_delete,
					      */

					     ...);

GtkWidget * gimp_dialog_newv                (const gchar        *title,
					     const gchar        *wmclass_name,
					     GimpHelpFunc        help_func,
					     const gchar        *help_data,
					     GtkWindowPosition   position,
					     gint                allow_shrink,
					     gint                allow_grow,
					     gint                auto_shrink,
					     va_list             args);

void        gimp_dialog_set_icon            (GtkWindow          *dialog);

void        gimp_dialog_create_action_area  (GtkDialog          *dialog,

					     /* specify action area buttons
					      * as va_list:
					      *  const gchar    *label,
					      *  GCallback       callback,
					      *  gpointer        callback_data,
					      *  GObject        *slot_object,
					      *  GtkWidget     **widget_ptr,
					      *  gboolean        default_action,
					      *  gboolean        connect_delete,
					      */

					     ...);

void        gimp_dialog_create_action_areav (GtkDialog          *dialog,
					     va_list             args);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GIMP_DIALOG_H__ */
