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

#ifndef __COLOR_PANEL_H__
#define __COLOR_PANEL_H__


#include "libgimp/gimpcolorbutton.h"


#define GIMP_TYPE_COLOR_PANEL            (gimp_color_panel_get_type ())
#define GIMP_COLOR_PANEL(obj)            (GTK_CHECK_CAST ((obj), GIMP_TYPE_COLOR_PANEL, GimpColorPanel))
#define GIMP_COLOR_PANEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GIMP_TYPE_COLOR_PANEL, GimpColorPanelClass))
#define GIMP_IS_COLOR_PANEL(obj)         (GTK_CHECK_TYPE ((obj), GIMP_TYPE_COLOR_PANEL))
#define GIMP_IS_COLOR_PANEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_COLOR_PANEL))

typedef struct _GimpColorPanel       GimpColorPanel;
typedef struct _GimpColorPanelClass  GimpColorPanelClass;

struct _GimpColorPanelClass
{
  GimpColorButtonClass  parent_class;
};

GtkType     gimp_color_panel_get_type  (void);
GtkWidget * gimp_color_panel_new       (const GimpRGB     *color,
					GimpColorAreaType  type,
					gint               width,
					gint               height);


#endif  /*  __COLOR_PANEL_H__  */
