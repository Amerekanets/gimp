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

#ifndef __GIMP_BRUSH_H__
#define __GIMP_BRUSH_H__


#include "gimpobject.h"


#define GIMP_TYPE_BRUSH         (gimp_brush_get_type ())
#define GIMP_BRUSH(obj)         (GTK_CHECK_CAST ((obj), GIMP_TYPE_BRUSH, GimpBrush))
#define GIMP_IS_BRUSH(obj)      (GTK_CHECK_TYPE ((obj), GIMP_TYPE_BRUSH))
#define GIMP_BRUSH_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), gimp_brush_get_type(), GimpBrushClass))


typedef struct _GimpBrushClass GimpBrushClass;

struct _GimpBrush
{
  GimpObject   gobject;

  gchar       *filename;   /*  actual filename--brush's location on disk  */
  gchar       *name;       /*  brush's name--for brush selection dialog   */
  gint         spacing;    /*  brush's spacing                            */
  GimpVector2  x_axis;     /*  for calculating brush spacing              */
  GimpVector2  y_axis;     /*  for calculating brush spacing              */
  TempBuf     *mask;       /*  the actual mask                            */
  TempBuf     *pixmap;     /*  optional pixmap data                       */
};

struct _GimpBrushClass
{
  GimpObjectClass parent_class;

  void        (* dirty)            (GimpBrush *brush);
  void        (* rename)           (GimpBrush *brush);

  /* FIXME: these are no virtual function pointers but bad hacks: */
  GimpBrush * (* select_brush)     (PaintCore *paint_core);
  gboolean    (* want_null_motion) (PaintCore *paint_core);
};


GtkType     gimp_brush_get_type    (void);
GimpBrush * gimp_brush_load        (gchar     *filename);

GimpBrush * gimp_brush_load_brush  (gint       fd,
				    gchar     *filename);

TempBuf   * gimp_brush_get_mask    (GimpBrush *brush);
TempBuf *   gimp_brush_get_pixmap  (GimpBrush *brush);

gchar     * gimp_brush_get_name    (GimpBrush *brush);
void        gimp_brush_set_name    (GimpBrush *brush,
				    gchar     *name);

gint        gimp_brush_get_spacing (GimpBrush *brush);
void        gimp_brush_set_spacing (GimpBrush *brush,
				    gint       spacing);


#endif /* __GIMP_BRUSH_H__ */
