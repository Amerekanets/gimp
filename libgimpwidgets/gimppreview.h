/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * gimppreview.h
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

#ifndef __GIMP_PREVIEW_H__
#define __GIMP_PREVIEW_H__

#include <gtk/gtktable.h>

G_BEGIN_DECLS


/* For information look into the C source or the html documentation */


#define GIMP_TYPE_PREVIEW            (gimp_preview_get_type ())
#define GIMP_PREVIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_PREVIEW, GimpPreview))
#define GIMP_PREVIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_PREVIEW, GimpPreviewClass))
#define GIMP_IS_PREVIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_PREVIEW))
#define GIMP_IS_PREVIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_PREVIEW))
#define GIMP_PREVIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_PREVIEW, GimpPreviewClass))


typedef struct _GimpPreviewClass  GimpPreviewClass;

struct _GimpPreview
{
  GtkTable      parent_instance;

  gint          xoff, yoff;
  gint          xmin, xmax, ymin, ymax;
  gint          drag_x, drag_y;
  gint          drag_xoff, drag_yoff;

  gint          width, height;
  GtkObject    *hadj, *vadj;
  GtkWidget    *hscr, *vscr;
  gboolean      in_drag;
  GtkWidget    *area;
  GtkWidget    *toggle_update;
  gboolean      update_preview;
  guint         timeout_id;
};

struct _GimpPreviewClass
{
  GtkTableClass parent_class;

  /* virtuals */
  void (* draw) (GimpPreview *preview);

  /* signal */
  void (* invalidated) (GimpPreview *preview);
};


GType   gimp_preview_get_type           (void) G_GNUC_CONST;

void    gimp_preview_get_size           (GimpPreview *preview,
                                         gint        *width,
                                         gint        *height);

void    gimp_preview_get_position       (GimpPreview *preview,
                                         gint        *x,
                                         gint        *y);

void    gimp_preview_show_update_toggle (GimpPreview *preview,
                                         gboolean     show_update);

void    gimp_preview_invalidate         (GimpPreview *preview);

G_END_DECLS

#endif /* __GIMP_PREVIEW_H__ */

