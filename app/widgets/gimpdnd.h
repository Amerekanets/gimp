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


typedef enum
{
  GIMP_DND_TYPE_NONE         = 0,
  GIMP_DND_TYPE_URI_LIST     = 1,
  GIMP_DND_TYPE_TEXT_PLAIN   = 2,
  GIMP_DND_TYPE_NETSCAPE_URL = 3,
  GIMP_DND_TYPE_COLOR        = 4,
  GIMP_DND_TYPE_IMAGE        = 5,
  GIMP_DND_TYPE_LAYER        = 6,
  GIMP_DND_TYPE_CHANNEL      = 7,
  GIMP_DND_TYPE_LAYER_MASK   = 8,
  GIMP_DND_TYPE_COMPONENT    = 9,
  GIMP_DND_TYPE_VECTORS      = 10,
  GIMP_DND_TYPE_BRUSH        = 11,
  GIMP_DND_TYPE_PATTERN      = 12,
  GIMP_DND_TYPE_GRADIENT     = 13,
  GIMP_DND_TYPE_PALETTE      = 14,
  GIMP_DND_TYPE_BUFFER       = 15,
  GIMP_DND_TYPE_IMAGEFILE    = 16,
  GIMP_DND_TYPE_TOOL         = 17,
  GIMP_DND_TYPE_DIALOG       = 18,

  GIMP_DND_TYPE_LAST         = GIMP_DND_TYPE_DIALOG
} GimpDndType;


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

#define GIMP_TARGET_VECTORS \
        { "GIMP_VECTORS", GTK_TARGET_SAME_APP, GIMP_DND_TYPE_VECTORS }

#define GIMP_TARGET_COLOR \
        { "application/x-color", 0, GIMP_DND_TYPE_COLOR }

#define GIMP_TARGET_BRUSH \
        { "GIMP_BRUSH", 0, GIMP_DND_TYPE_BRUSH }

#define GIMP_TARGET_PATTERN \
        { "GIMP_PATTERN", 0, GIMP_DND_TYPE_PATTERN }

#define GIMP_TARGET_GRADIENT \
        { "GIMP_GRADIENT", 0, GIMP_DND_TYPE_GRADIENT }

#define GIMP_TARGET_PALETTE \
        { "GIMP_PALETTE", 0, GIMP_DND_TYPE_PALETTE }

#define GIMP_TARGET_BUFFER \
        { "GIMP_BUFFER", 0, GIMP_DND_TYPE_BUFFER }

#define GIMP_TARGET_IMAGEFILE \
        { "GIMP_IMAGEFILE", GTK_TARGET_SAME_APP, GIMP_DND_TYPE_IMAGEFILE }

#define GIMP_TARGET_TOOL \
        { "GIMP_TOOL", GTK_TARGET_SAME_APP, GIMP_DND_TYPE_TOOL }

#define GIMP_TARGET_DIALOG \
        { "GIMP_DIALOG", GTK_TARGET_SAME_APP, GIMP_DND_TYPE_DIALOG }


/*  file / url dnd functions  */

typedef void (* GimpDndDropFileFunc) (GtkWidget *widget,
				      GList     *files,
				      gpointer   data);

void  gimp_dnd_file_dest_set   (GtkWidget           *widget,
				GimpDndDropFileFunc  set_file_func,
				gpointer             data);
void  gimp_dnd_file_dest_unset (GtkWidget           *widget);

/*  standard callback  */
void  gimp_dnd_open_files      (GtkWidget           *widget,
				GList               *files,
				gpointer             data);


/*  color dnd functions  */

typedef void (* GimpDndDropColorFunc) (GtkWidget     *widget,
				       const GimpRGB *color,
				       gpointer       data);
typedef void (* GimpDndDragColorFunc) (GtkWidget     *widget,
				       GimpRGB       *color,
				       gpointer       data);

void  gimp_dnd_color_source_set    (GtkWidget            *widget,
				    GimpDndDragColorFunc  get_color_func,
				    gpointer              data);
void  gimp_dnd_color_dest_set      (GtkWidget            *widget,
				    GimpDndDropColorFunc  set_color_func,
				    gpointer              data);
void  gimp_dnd_color_dest_unset    (GtkWidget            *widget);


/*  GimpViewable (by GType) dnd functions  */

typedef void           (* GimpDndDropViewableFunc) (GtkWidget     *widget,
						    GimpViewable  *viewable,
						    gpointer       data);
typedef GimpViewable * (* GimpDndDragViewableFunc) (GtkWidget     *widget,
						    gpointer       data);


void  gimp_gtk_drag_source_set_by_type (GtkWidget               *widget,
					GdkModifierType          start_button_mask,
					GType                    type,
					GdkDragAction            actions);
void  gimp_gtk_drag_dest_set_by_type   (GtkWidget               *widget,
					GtkDestDefaults          flags,
					GType                    type,
					GdkDragAction            actions);

void  gimp_dnd_viewable_source_set     (GtkWidget               *widget,
					GType                    type,
					GimpDndDragViewableFunc  get_viewable_func,
					gpointer                 data);
void  gimp_dnd_viewable_source_unset   (GtkWidget               *widget,
                                        GType                    type);
void  gimp_dnd_viewable_dest_set       (GtkWidget               *widget,
					GType                    type,
					GimpDndDropViewableFunc  set_viewable_func,
					gpointer                 data);
void  gimp_dnd_viewable_dest_unset     (GtkWidget               *widget,
					GType                    type);

GimpViewable * gimp_dnd_get_drag_data  (GtkWidget               *widget);


#endif /* __GIMP_DND_H__ */
