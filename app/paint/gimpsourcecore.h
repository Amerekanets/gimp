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

#ifndef __GIMP_CLONE_H__
#define __GIMP_CLONE_H__


#include "gimppaintcore.h"


typedef enum /*< skip >*/ /*< pdb-skip >*/
{
  ALIGN_NO,
  ALIGN_YES,
  ALIGN_REGISTERED
} AlignType;


#define GIMP_TYPE_CLONE            (gimp_clone_get_type ())
#define GIMP_CLONE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_CLONE, GimpClone))
#define GIMP_CLONE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_CLONE, GimpCloneClass))
#define GIMP_IS_CLONE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_CLONE))
#define GIMP_IS_CLONE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_CLONE))
#define GIMP_CLONE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_CLONE, GimpCloneClass))


typedef struct _GimpClone      GimpClone;
typedef struct _GimpCloneClass GimpCloneClass;

struct _GimpClone
{
  GimpPaintCore parent_instance;

  gboolean      set_source;

  GimpDrawable *src_drawable;
  gint          src_x;
  gint          src_y;

  void (* init_callback)      (GimpClone *clone,
                               gpointer   data);
  void (* finish_callback)    (GimpClone *clone,
                               gpointer   data);
  void (* pretrace_callback)  (GimpClone *clone,
                               gpointer   data);
  void (* posttrace_callback) (GimpClone *clone,
                               gpointer   data);

  gpointer callback_data;
};

struct _GimpCloneClass
{
  GimpPaintCoreClass parent_class;
};


typedef struct _CloneOptions CloneOptions;

struct _CloneOptions
{
  PaintOptions  paint_options;

  CloneType     type;
  CloneType     type_d;
  GtkWidget    *type_w[2];  /* 2 radio buttons */

  AlignType     aligned;
  AlignType     aligned_d;
  GtkWidget    *aligned_w[3];  /* 3 radio buttons */
};


GType   gimp_clone_get_type (void) G_GNUC_CONST;


#endif  /*  __GIMP_CLONE_H__  */
