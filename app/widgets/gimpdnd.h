/* The GIMP -- an image manipulation program
 * Copyright (C) 1995-1997 Spencer Kimball and Peter Mattis
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
#ifndef __GIMP_DND_H__
#define __GIMP_DND_H__

#include <gtk/gtk.h>

#include "gimpdrawable.h"

enum
{
  GIMP_DND_TYPE_URI_LIST,
  GIMP_DND_TYPE_TEXT_PLAIN,
  GIMP_DND_TYPE_NETSCAPE_URL,
  GIMP_DND_TYPE_IMAGE,
  GIMP_DND_TYPE_LAYER,
  GIMP_DND_TYPE_CHANNEL,
  GIMP_DND_TYPE_LAYER_MASK,
  GIMP_DND_TYPE_COMPONENT,
  GIMP_DND_TYPE_PATH
};

#define GIMP_TARGET_URI_LIST \
        { "text/uri-list", 0, GIMP_DND_TYPE_URI_LIST }

#define GIMP_TARGET_TEXT_PLAIN \
        { "text/plain", 0, GIMP_DND_TYPE_TEXT_PLAIN }

#define GIMP_TARGET_NETSCAPE_URL \
        { "_NETSCAPE_URL", 0, GIMP_DND_TYPE_NETSCAPE_URL }

#define GIMP_TARGET_IMAGE \
        { "GIMP_IMAGE", GTK_TARGET_SAME_APP, GIMP_DND_TYPE_IMAGE }

#define GIMP_TARGET_LAYER \
        { "GIMP_LAYER", GTK_TARGET_SAME_APP, GIMP_DND_TYPE_LAYER }

#define GIMP_TARGET_CHANNEL \
        { "GIMP_CHANNEL", GTK_TARGET_SAME_APP, GIMP_DND_TYPE_CHANNEL }

#define GIMP_TARGET_LAYER_MASK \
        { "GIMP_LAYER_MASK", GTK_TARGET_SAME_APP, GIMP_DND_TYPE_LAYER_MASK }

#define GIMP_TARGET_COMPONENT \
        { "GIMP_COMPONENT", GTK_TARGET_SAME_APP, GIMP_DND_TYPE_COMPONENT }

#define GIMP_TARGET_PATH \
        { "GIMP_PATH", GTK_TARGET_SAME_APP, GIMP_DND_TYPE_PATH }

typedef enum
{
  GIMP_DROP_NONE,
  GIMP_DROP_ABOVE,
  GIMP_DROP_BELOW
} GimpDropType;

void  gimp_dnd_set_drawable_preview_icon (GtkWidget      *widget,
					  GdkDragContext *context,
					  GimpDrawable   *drawable);

#endif /* __GIMP_DND_H__ */
