/* LIBGIMP - The GIMP Library 
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball                
 *
 * gimpcolorbutton.h
 * Copyright (C) 1999-2001 Sven Neumann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU  
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* This provides a button with a color preview. The preview
 * can handle transparency by showing the checkerboard.
 * On click, a color selector is opened, which is already
 * fully functional wired to the preview button.
 */

#ifndef __GIMP_COLOR_BUTTON_H__
#define __GIMP_COLOR_BUTTON_H__


#include "libgimpwidgets/gimpcolorarea.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GIMP_TYPE_COLOR_BUTTON            (gimp_color_button_get_type ())
#define GIMP_COLOR_BUTTON(obj)            (GTK_CHECK_CAST ((obj), GIMP_TYPE_COLOR_BUTTON, GimpColorButton))
#define GIMP_COLOR_BUTTON_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GIMP_TYPE_COLOR_BUTTON, GimpColorButtonClass))
#define GIMP_IS_COLOR_BUTTON(obj)         (GTK_CHECK_TYPE ((obj), GIMP_TYPE_COLOR_BUTTON))
#define GIMP_IS_COLOR_BUTTON_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_COLOR_BUTTON))


typedef struct _GimpColorButtonClass  GimpColorButtonClass;

struct _GimpColorButton
{
  GtkButton       button;

  gchar          *title;

  GtkWidget      *color_area;
  GtkWidget      *dialog;
  GtkItemFactory *item_factory;
};

struct _GimpColorButtonClass
{
  GtkButtonClass parent_class;

  void (* color_changed) (GimpColorButton *gcb);
};


GtkType     gimp_color_button_get_type   (void);
GtkWidget * gimp_color_button_new        (const gchar       *title,
					  gint               width,
					  gint               height,
					  const GimpRGB     *color,
					  GimpColorAreaType  type);
void        gimp_color_button_set_color  (GimpColorButton   *gcb,
					  const GimpRGB     *color);
void        gimp_color_button_get_color  (GimpColorButton   *gcb,
					  GimpRGB           *color);
gboolean    gimp_color_button_has_alpha  (GimpColorButton   *gcb);
void        gimp_color_button_set_type   (GimpColorButton   *gcb,
					  GimpColorAreaType  alpha);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GIMP_COLOR_BUTTON_H__ */
