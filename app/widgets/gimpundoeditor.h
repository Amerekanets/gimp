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

#ifndef __GIMP_UNDO_EDITOR_H__
#define __GIMP_UNDO_EDITOR_H__


#include "gimpeditor.h"


#define GIMP_TYPE_UNDO_EDITOR            (gimp_undo_editor_get_type ())
#define GIMP_UNDO_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_UNDO_EDITOR, GimpUndoEditor))
#define GIMP_UNDO_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_UNDO_EDITOR, GimpUndoEditorClass))
#define GIMP_IS_UNDO_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_UNDO_EDITOR))
#define GIMP_IS_UNDO_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_UNDO_EDITOR))
#define GIMP_UNDO_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_UNDO_EDITOR, GimpUndoEditorClass))


typedef struct _GimpUndoEditorClass GimpUndoEditorClass;

struct _GimpUndoEditor
{
  GimpEditor     parent_instance;

  GimpImage     *gimage;

  GimpContainer *container;
  GtkWidget     *view;

  GimpUndo      *base_item;

  GtkWidget     *undo_button;
  GtkWidget     *redo_button;
};

struct _GimpUndoEditorClass
{
  GimpEditorClass  parent_class;
};


GType       gimp_undo_editor_get_type  (void) G_GNUC_CONST;

GtkWidget * gimp_undo_editor_new       (GimpImage      *gimage);
void        gimp_undo_editor_set_image (GimpUndoEditor *editor,
                                        GimpImage      *gimage);


#endif /* __GIMP_UNDO_EDITOR_H__ */
