/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpnavigationview.h
 * Copyright (C) 2002 Michael Natterer <mitch@gimp.org>
 *
 * partly based on app/nav_window
 * Copyright (C) 1999 Andy Thomas <alt@gimp.org>
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

#ifndef __GIMP_NAVIGATION_VIEW_H__
#define __GIMP_NAVIGATION_VIEW_H__


#include "widgets/gimpeditor.h"


#define GIMP_TYPE_NAVIGATION_VIEW            (gimp_navigation_view_get_type ())
#define GIMP_NAVIGATION_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_NAVIGATION_VIEW, GimpNavigationView))
#define GIMP_NAVIGATION_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_NAVIGATION_VIEW, GimpNavigationViewClass))
#define GIMP_IS_NAVIGATION_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_NAVIGATION_VIEW))
#define GIMP_IS_NAVIGATION_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_NAVIGATION_VIEW))
#define GIMP_NAVIGATION_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_NAVIGATION_VIEW, GimpNavigationViewClass))


typedef struct _GimpNavigationViewClass  GimpNavigationViewClass;

struct _GimpNavigationView
{
  GimpEditor        parent_instance;

  GimpDisplayShell *shell;

  GtkWidget        *preview;
  GtkWidget        *zoom_label;
  GtkAdjustment    *zoom_adjustment;

  GtkWidget        *zoom_out_button;
  GtkWidget        *zoom_in_button;
  GtkWidget        *zoom_100_button;
  GtkWidget        *zoom_fit_button;
  GtkWidget        *shrink_wrap_button;
};

struct _GimpNavigationViewClass
{
  GimpEditorClass  parent_class;
};


GType       gimp_navigation_view_get_type  (void) G_GNUC_CONST;

GtkWidget * gimp_navigation_view_new       (GimpDisplayShell   *shell);
void        gimp_navigation_view_set_shell (GimpNavigationView *view,
                                            GimpDisplayShell   *shell);

void        gimp_navigation_view_popup     (GimpDisplayShell   *shell,
                                            GtkWidget          *widget,
                                            gint                click_x,
                                            gint                click_y);


#endif  /*  __GIMP_NAVIGATION_VIEW_H__  */
