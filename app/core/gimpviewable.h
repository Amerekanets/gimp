/* The GIMP -- an image manipulation program
 * Copyright (C) 1995-1997 Spencer Kimball and Peter Mattis
 *
 * gimpviewable.h
 * Copyright (C) 2001 Michael Natterer <mitch@gimp.org>
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

#ifndef __GIMP_VIEWABLE_H__
#define __GIMP_VIEWABLE_H__


#include "gimpobject.h"

#include <gdk-pixbuf/gdk-pixbuf.h>


#define GIMP_TYPE_VIEWABLE            (gimp_viewable_get_type ())
#define GIMP_VIEWABLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_VIEWABLE, GimpViewable))
#define GIMP_VIEWABLE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_VIEWABLE, GimpViewableClass))
#define GIMP_IS_VIEWABLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_VIEWABLE))
#define GIMP_IS_VIEWABLE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_VIEWABLE))
#define GIMP_VIEWABLE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_VIEWABLE, GimpViewableClass))


typedef struct _GimpViewableClass GimpViewableClass;

struct _GimpViewable
{
  GimpObject  parent_instance;
};

struct _GimpViewableClass
{
  GimpObjectClass  parent_class;

  const gchar     *name_changed_signal;

  /*  signals  */
  void      (* invalidate_preview) (GimpViewable *viewable);
  void      (* size_changed)       (GimpViewable *viewable);

  /*  virtual functions  */
  void      (* get_preview_size)   (GimpViewable *viewable,
				    gint          size,
                                    gboolean      is_popup,
                                    gboolean      dot_for_dot,
				    gint         *width,
				    gint         *height);
  TempBuf * (* get_preview)        (GimpViewable *viewable,
				    gint          width,
				    gint          height);
  TempBuf * (* get_new_preview)    (GimpViewable *viewable,
				    gint          width,
				    gint          height);
};


GType       gimp_viewable_get_type               (void) G_GNUC_CONST;

void        gimp_viewable_invalidate_preview     (GimpViewable *viewable);
void        gimp_viewable_size_changed           (GimpViewable *viewable);

void        gimp_viewable_calc_preview_size      (GimpViewable *viewable,
                                                  gint          aspect_width,
                                                  gint          aspect_height,
                                                  gint          width,
                                                  gint          height,
                                                  gboolean      dot_for_dot,
                                                  gdouble       xresolution,
                                                  gdouble       yresolution,
                                                  gint         *return_width,
                                                  gint         *return_height,
                                                  gboolean     *scaling_up);

void        gimp_viewable_get_preview_size       (GimpViewable *viewable,
                                                  gint          size,
                                                  gboolean      popup,
                                                  gboolean      dot_for_dot,
                                                  gint         *width,
                                                  gint         *height);
TempBuf   * gimp_viewable_get_preview            (GimpViewable *viewable,
                                                  gint          width,
                                                  gint          height);
TempBuf   * gimp_viewable_get_new_preview        (GimpViewable *viewable,
                                                  gint          width,
                                                  gint          height);

GdkPixbuf * gimp_viewable_get_preview_pixbuf     (GimpViewable *viewable,
                                                  gint          width,
                                                  gint          height);
GdkPixbuf * gimp_viewable_get_new_preview_pixbuf (GimpViewable *viewable,
                                                  gint          width,
                                                  gint          height);


#endif  /* __GIMP_VIEWABLE_H__ */
