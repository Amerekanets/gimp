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

#include <gtk/gtkdialog.h>

G_BEGIN_DECLS

/* For information look into the C source or the html documentation */


#define GIMP_TYPE_DIALOG            (gimp_dialog_get_type ())
#define GIMP_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_DIALOG, GimpDialog))
#define GIMP_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_DIALOG, GimpDialogClass))
#define GIMP_IS_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_DIALOG))
#define GIMP_IS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_DIALOG))
#define GIMP_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_DIALOG, GimpDialogClass))


typedef struct _GimpDialogClass  GimpDialogClass;

struct _GimpDialog
{
  GtkDialog  parent_instance;
};

struct _GimpDialogClass
{
  GtkDialogClass  parent_class;
};


GType       gimp_dialog_get_type            (void) G_GNUC_CONST;

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

void        gimp_dialog_create_action_area  (GimpDialog         *dialog,

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

void        gimp_dialog_create_action_areav (GimpDialog         *dialog,
					     va_list             args);


G_END_DECLS

#endif /* __GIMP_DIALOG_H__ */
