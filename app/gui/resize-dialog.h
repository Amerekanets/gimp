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

#ifndef __RESIZE_DIALOG_H__
#define __RESIZE_DIALOG_H__


typedef enum
{
  ScaleWidget,
  ResizeWidget
} ResizeType;

typedef enum
{
  ResizeImage,
  ResizeLayer
} ResizeTarget;


typedef struct _Resize Resize;

struct _Resize
{
  GtkWidget             *resize_shell;

  GimpImage             *gimage;

  ResizeType             type;
  ResizeTarget           target;

  gint                   width;
  gint                   height;

  gdouble                resolution_x;
  gdouble                resolution_y;
  GimpUnit               unit;

  gdouble                ratio_x;
  gdouble                ratio_y;

  gint                   offset_x;
  gint                   offset_y;

  GimpInterpolationType  interpolation;
};


/*  If resolution_x is zero, then don't show resolution modification
 *  parts of the dialog.
 *
 *  If object and signal are non-NULL, then attach the cancel callback to signal.
 *
 *  If cancel_callback is NULL, then the dialog will be destroyed on "Cancel".
 */

Resize * resize_widget_new (GimpImage    *gimage,
                            ResizeType    type,
			    ResizeTarget  target,
			    GObject      *object,
			    const gchar  *signal,
			    gint          width,
			    gint          height,
			    gdouble       resolution_x,
			    gdouble       resolution_y,
			    GimpUnit      unit,
			    gboolean      dot_for_dot,
			    GCallback     ok_cb,
			    GCallback     cancel_cb,
			    gpointer      user_data);


#endif  /*  __RESIZE_DIALOG_H__  */
