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

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "libgimpcolor/gimpcolor.h"
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "widgets-types.h"

#include "core/gimp.h"
#include "core/gimpbrush.h"
#include "core/gimpbuffer.h"
#include "core/gimpchannel.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimpdrawable.h"
#include "core/gimpgradient.h"
#include "core/gimpimage.h"
#include "core/gimpimagefile.h"
#include "core/gimplayer.h"
#include "core/gimplayermask.h"
#include "core/gimppalette.h"
#include "core/gimppattern.h"
#include "core/gimptoolinfo.h"

#include "gui/file-open-dialog.h"

#include "app_procs.h"

#include "gimpdnd.h"
#include "gimppreview.h"


#define DRAG_PREVIEW_SIZE 32
#define DRAG_ICON_OFFSET  -8


typedef GtkWidget * (* GimpDndGetIconFunc)  (GtkWidget *widget,
					     GCallback  get_data_func,
					     gpointer   get_data_data);
typedef guchar    * (* GimpDndDragDataFunc) (GtkWidget *widget,
					     GCallback  get_data_func,
					     gpointer   get_data_data,
					     gint      *format,
					     gint      *length);
typedef void        (* GimpDndDropDataFunc) (GtkWidget *widget,
					     GCallback  set_data_func,
					     gpointer   set_data_data,
					     guchar    *vals,
					     gint       format,
					     gint       length);


typedef struct _GimpDndDataDef GimpDndDataDef;

struct _GimpDndDataDef
{
  GtkTargetEntry       target_entry;

  gchar               *set_data_func_name;
  gchar               *set_data_data_name;

  GimpDndGetIconFunc   get_icon_func;
  GimpDndDragDataFunc  get_data_func;
  GimpDndDropDataFunc  set_data_func;
};


static GtkWidget * gimp_dnd_get_viewable_icon  (GtkWidget *widget,
					        GCallback  get_viewable_func,
					        gpointer   get_viewable_data);
static GtkWidget * gimp_dnd_get_color_icon     (GtkWidget *widget,
					        GCallback  get_color_func,
					        gpointer   get_color_data);

static void        gimp_dnd_set_file_data      (GtkWidget *widget,
					        GCallback  set_color_func,
					        gpointer   set_color_data,
					        guchar    *vals,
					        gint       format,
					        gint       length);

static guchar    * gimp_dnd_get_color_data     (GtkWidget *widget,
					        GCallback  get_color_func,
					        gpointer   get_color_data,
					        gint      *format,
					        gint      *length);
static void        gimp_dnd_set_color_data     (GtkWidget *widget,
					        GCallback  set_color_func,
					        gpointer   set_color_data,
					        guchar    *vals,
					        gint       format,
					        gint       length);

static guchar    * gimp_dnd_get_image_data     (GtkWidget *widget,
					        GCallback  get_image_func,
					        gpointer   get_image_data,
					        gint      *format,
					        gint      *length);
static void        gimp_dnd_set_image_data     (GtkWidget *widget,
					        GCallback  set_image_func,
					        gpointer   set_image_data,
					        guchar    *vals,
					        gint       format,
					        gint       length);

static guchar    * gimp_dnd_get_drawable_data  (GtkWidget *widget,
					        GCallback  get_drawable_func,
					        gpointer   get_drawable_data,
					        gint      *format,
					        gint      *length);
static void        gimp_dnd_set_drawable_data  (GtkWidget *widget,
					        GCallback  set_drawable_func,
					        gpointer   set_drawable_data,
					        guchar    *vals,
					        gint       format,
					        gint       length);

static guchar    * gimp_dnd_get_data_data      (GtkWidget *widget,
					        GCallback  get_data_func,
					        gpointer   get_data_data,
					        gint      *format,
					        gint      *length);
static void        gimp_dnd_set_brush_data     (GtkWidget *widget,
					        GCallback  set_brush_func,
					        gpointer   set_brush_data,
					        guchar    *vals,
					        gint       format,
					        gint       length);
static void        gimp_dnd_set_pattern_data   (GtkWidget *widget,
					        GCallback  set_pattern_func,
					        gpointer   set_pattern_data,
					        guchar    *vals,
					        gint       format,
					        gint       length);
static void        gimp_dnd_set_gradient_data  (GtkWidget *widget,
					        GCallback  set_gradient_func,
					        gpointer   set_gradient_data,
					        guchar    *vals,
					        gint       format,
					        gint       length);
static void        gimp_dnd_set_palette_data   (GtkWidget *widget,
					        GCallback  set_palette_func,
					        gpointer   set_palette_data,
					        guchar    *vals,
					        gint       format,
					        gint       length);
static void        gimp_dnd_set_buffer_data    (GtkWidget *widget,
					        GCallback  set_buffer_func,
					        gpointer   set_buffer_data,
					        guchar    *vals,
					        gint       format,
					        gint       length);
static void        gimp_dnd_set_imagefile_data (GtkWidget *widget,
					        GCallback  set_imagefile_func,
					        gpointer   set_imagefile_data,
					        guchar    *vals,
					        gint       format,
					        gint       length);
static void        gimp_dnd_set_tool_data      (GtkWidget *widget,
					        GCallback  set_tool_func,
					        gpointer   set_tool_data,
					        guchar    *vals,
					        gint       format,
					        gint       length);



static GimpDndDataDef dnd_data_defs[] =
{
  {
    { NULL, 0, -1 },

    NULL,
    NULL,

    NULL,
    NULL,
    NULL
  },

  {
    GIMP_TARGET_URI_LIST,

    "gimp_dnd_set_file_func",
    "gimp_dnd_set_file_data",

    NULL,
    NULL,
    gimp_dnd_set_file_data
  },

  {
    GIMP_TARGET_TEXT_PLAIN,

    "gimp_dnd_set_file_func",
    "gimp_dnd_set_file_data",

    NULL,
    NULL,
    gimp_dnd_set_file_data
  },

  {
    GIMP_TARGET_NETSCAPE_URL,

    "gimp_dnd_set_file_func",
    "gimp_dnd_set_file_data",

    NULL,
    NULL,
    gimp_dnd_set_file_data
  },

  {
    GIMP_TARGET_COLOR,

    "gimp_dnd_set_color_func",
    "gimp_dnd_set_color_data",

    gimp_dnd_get_color_icon,
    gimp_dnd_get_color_data,
    gimp_dnd_set_color_data
  },

  {
    GIMP_TARGET_IMAGE,

    "gimp_dnd_set_image_func",
    "gimp_dnd_set_image_data",

    gimp_dnd_get_viewable_icon,
    gimp_dnd_get_image_data,
    gimp_dnd_set_image_data,
  },

  {
    GIMP_TARGET_LAYER,

    "gimp_dnd_set_layer_func",
    "gimp_dnd_set_layer_data",

    gimp_dnd_get_viewable_icon,
    gimp_dnd_get_drawable_data,
    gimp_dnd_set_drawable_data,
  },

  {
    GIMP_TARGET_CHANNEL,

    "gimp_dnd_set_channel_func",
    "gimp_dnd_set_channel_data",

    gimp_dnd_get_viewable_icon,
    gimp_dnd_get_drawable_data,
    gimp_dnd_set_drawable_data,
  },

  {
    GIMP_TARGET_LAYER_MASK,

    "gimp_dnd_set_layer_mask_func",
    "gimp_dnd_set_layer_mask_data",

    gimp_dnd_get_viewable_icon,
    gimp_dnd_get_drawable_data,
    gimp_dnd_set_drawable_data,
  },

  {
    GIMP_TARGET_COMPONENT,

    NULL,
    NULL,

    NULL,
    NULL,
    NULL,
  },

  {
    GIMP_TARGET_PATH,

    NULL,
    NULL,

    NULL,
    NULL,
    NULL,
  },

  {
    GIMP_TARGET_BRUSH,

    "gimp_dnd_set_brush_func",
    "gimp_dnd_set_brush_data",

    gimp_dnd_get_viewable_icon,
    gimp_dnd_get_data_data,
    gimp_dnd_set_brush_data
  },

  {
    GIMP_TARGET_PATTERN,

    "gimp_dnd_set_pattern_func",
    "gimp_dnd_set_pattern_data",

    gimp_dnd_get_viewable_icon,
    gimp_dnd_get_data_data,
    gimp_dnd_set_pattern_data
  },

  {
    GIMP_TARGET_GRADIENT,

    "gimp_dnd_set_gradient_func",
    "gimp_dnd_set_gradient_data",

    gimp_dnd_get_viewable_icon,
    gimp_dnd_get_data_data,
    gimp_dnd_set_gradient_data
  },

  {
    GIMP_TARGET_PALETTE,

    "gimp_dnd_set_palette_func",
    "gimp_dnd_set_palette_data",

    gimp_dnd_get_viewable_icon,
    gimp_dnd_get_data_data,
    gimp_dnd_set_palette_data
  },

  {
    GIMP_TARGET_BUFFER,

    "gimp_dnd_set_buffer_func",
    "gimp_dnd_set_buffer_data",

    gimp_dnd_get_viewable_icon,
    gimp_dnd_get_data_data,
    gimp_dnd_set_buffer_data
  },

  {
    GIMP_TARGET_IMAGEFILE,

    "gimp_dnd_set_imagefile_func",
    "gimp_dnd_set_imagefile_data",

    gimp_dnd_get_viewable_icon,
    gimp_dnd_get_data_data,
    gimp_dnd_set_imagefile_data
  },

  {
    GIMP_TARGET_TOOL,

    "gimp_dnd_set_tool_func",
    "gimp_dnd_set_tool_data",

    gimp_dnd_get_viewable_icon,
    gimp_dnd_get_data_data,
    gimp_dnd_set_tool_data
  },

  {
    GIMP_TARGET_DIALOG,

    NULL,
    NULL,

    NULL,
    NULL,
    NULL
  }
};


/********************************/
/*  general data dnd functions  */
/********************************/

static void
gimp_dnd_data_drag_begin (GtkWidget      *widget,
			  GdkDragContext *context,
			  gpointer        data)
{
  GimpDndType  data_type;
  GCallback    get_data_func;
  gpointer     get_data_data;
  GtkWidget   *icon_widget;

  data_type = (GimpDndType) g_object_get_data (G_OBJECT (widget),
                                               "gimp-dnd-get-data-type");

  if (! data_type)
    return;

  get_data_func = (GCallback) g_object_get_data (G_OBJECT (widget),
                                                 "gimp-dnd-get-data-func");
  get_data_data = (gpointer) g_object_get_data (G_OBJECT (widget),
                                                "gimp-dnd-get-data-data");

  if (! get_data_func)
    return;

  icon_widget = (* dnd_data_defs[data_type].get_icon_func) (widget,
							    get_data_func,
							    get_data_data);

  if (icon_widget)
    {
      GtkWidget *frame;
      GtkWidget *window;

      window = gtk_window_new (GTK_WINDOW_POPUP);
      gtk_widget_realize (window);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
      gtk_container_add (GTK_CONTAINER (window), frame);
      gtk_widget_show (frame);

      gtk_container_add (GTK_CONTAINER (frame), icon_widget);
      gtk_widget_show (icon_widget);

      g_object_set_data_full (G_OBJECT (widget),
                              "gimp-dnd-data-widget",
                              window,
                              (GDestroyNotify) gtk_widget_destroy);

      gtk_drag_set_icon_widget (context, window,
				DRAG_ICON_OFFSET, DRAG_ICON_OFFSET);
    }
}

static void
gimp_dnd_data_drag_end (GtkWidget      *widget,
			GdkDragContext *context)
{
  g_object_set_data (G_OBJECT (widget), "gimp-dnd-data-widget", NULL);
}

static void
gimp_dnd_data_drag_handle (GtkWidget        *widget, 
			   GdkDragContext   *context,
			   GtkSelectionData *selection_data,
			   guint             info,
			   guint             time,
			   gpointer          data)
{
  GimpDndType  data_type;
  GCallback    get_data_func;
  gpointer     get_data_data;
  gint         format;
  guchar      *vals;
  gint         length;

  data_type =
    (GimpDndType) g_object_get_data (G_OBJECT (widget),
                                     "gimp-dnd-get-data-type");

  if (! data_type)
    return;

  get_data_func =
    (GCallback) g_object_get_data (G_OBJECT (widget),
                                       "gimp-dnd-get-data-func");
  get_data_data =
    (gpointer) g_object_get_data (G_OBJECT (widget),
                                  "gimp-dnd-get-data-data");

  if (! get_data_func)
    return;

  vals = (* dnd_data_defs[data_type].get_data_func) (widget,
						     get_data_func,
						     get_data_data,
						     &format,
						     &length);

  if (vals)
    {
      gtk_selection_data_set
	(selection_data,
	 gdk_atom_intern (dnd_data_defs[data_type].target_entry.target, FALSE),
	 format, vals, length);

      g_free (vals);
    }
}

static void
gimp_dnd_data_drop_handle (GtkWidget        *widget, 
			   GdkDragContext   *context,
			   gint              x,
			   gint              y,
			   GtkSelectionData *selection_data,
			   guint             info,
			   guint             time,
			   gpointer          data)
{
  GCallback    set_data_func;
  gpointer     set_data_data;
  GimpDndType  data_type;

  if (selection_data->length < 0)
    return;

  g_print ("gimp_dnd_data_drop_handle(%d)\n", info);

  for (data_type = GIMP_DND_TYPE_NONE + 1;
       data_type <= GIMP_DND_TYPE_LAST;
       data_type++)
    {
      if (dnd_data_defs[data_type].target_entry.info == info)
	{
	  set_data_func = (GCallback)
	    g_object_get_data (G_OBJECT (widget),
                               dnd_data_defs[data_type].set_data_func_name);
	  set_data_data =
	    g_object_get_data (G_OBJECT (widget),
                               dnd_data_defs[data_type].set_data_data_name);

	  if (! set_data_func)
	    return;

	  (* dnd_data_defs[data_type].set_data_func) (widget,
						      set_data_func,
						      set_data_data,
						      selection_data->data,
						      selection_data->format,
						      selection_data->length);

	  return;
	}
    }
}

static void
gimp_dnd_data_source_set (GimpDndType  data_type,
			  GtkWidget   *widget,
			  GCallback    get_data_func,
			  gpointer     get_data_data)
{
  gboolean drag_connected;

  drag_connected =
    GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget),
                                        "gimp-dnd-drag-connected"));

  if (! drag_connected)
    {
      g_signal_connect (G_OBJECT (widget), "drag_begin",
                        G_CALLBACK (gimp_dnd_data_drag_begin),
                        NULL);
      g_signal_connect (G_OBJECT (widget), "drag_end",
                        G_CALLBACK (gimp_dnd_data_drag_end),
                        NULL);
      g_signal_connect (G_OBJECT (widget), "drag_data_get",
                        G_CALLBACK (gimp_dnd_data_drag_handle),
                        NULL);

      g_object_set_data (G_OBJECT (widget), "gimp-dnd-drag-connected",
                         GINT_TO_POINTER (TRUE));
    }

  g_object_set_data (G_OBJECT (widget), "gimp-dnd-get-data-type",
                     (gpointer) data_type);
  g_object_set_data (G_OBJECT (widget), "gimp-dnd-get-data-func",
                     get_data_func);
  g_object_set_data (G_OBJECT (widget), "gimp-dnd-get-data-data",
                     get_data_data);
}

static void
gimp_dnd_data_source_unset (GtkWidget *widget)
{
  gboolean drag_connected;

  drag_connected =
    GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget),
                                        "gimp-dnd-drag-connected"));

  if (! drag_connected)
    return;

  g_object_set_data (G_OBJECT (widget), "gimp-dnd-get-data-type", NULL);
  g_object_set_data (G_OBJECT (widget), "gimp-dnd-get-data-func", NULL);
  g_object_set_data (G_OBJECT (widget), "gimp-dnd-get-data-data", NULL);
}

static void
gimp_dnd_data_dest_set (GimpDndType  data_type,
			GtkWidget   *widget,
			gpointer     set_data_func,
			gpointer     set_data_data)
{
  gboolean drop_connected;

  drop_connected =
    GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget),
                                        "gimp-dnd-drop-connected"));

  if (! drop_connected)
    {
      g_signal_connect (G_OBJECT (widget), "drag_data_received",
                        G_CALLBACK (gimp_dnd_data_drop_handle),
                        NULL);

      g_object_set_data (G_OBJECT (widget), "gimp-dnd-drop-connected",
                         GINT_TO_POINTER (TRUE));
    }

  g_object_set_data (G_OBJECT (widget),
                     dnd_data_defs[data_type].set_data_func_name,
                     set_data_func);
  g_object_set_data (G_OBJECT (widget),
                     dnd_data_defs[data_type].set_data_data_name,
                     set_data_data);
}

static void
gimp_dnd_data_dest_unset (GimpDndType  data_type,
			  GtkWidget   *widget)
{
  gboolean drop_connected;

  drop_connected =
    GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget),
                                        "gimp-dnd-drop-connected"));

  if (! drop_connected)
    return;

  g_object_set_data (G_OBJECT (widget),
                     dnd_data_defs[data_type].set_data_func_name,
                     NULL);
  g_object_set_data (G_OBJECT (widget),
                     dnd_data_defs[data_type].set_data_data_name,
                     NULL);
}


/************************/
/*  file dnd functions  */
/************************/

static void
gimp_dnd_set_file_data (GtkWidget     *widget,
			GCallback      set_file_func,
			gpointer       set_file_data,
			guchar        *vals,
			gint           format,
			gint           length)
{
  GList *files = NULL;
  gchar *buffer;

  if (format != 8)
    {
      g_warning ("Received invalid file data\n");
      return;
    }

  buffer = (gchar *) vals;

  {
    gchar  name_buffer[1024];
    const gchar *data_type = "file:";
    const gint   sig_len = strlen (data_type);

    while (*buffer)
      {
	gchar *name = name_buffer;
	gint len = 0;

	while ((*buffer != 0) && (*buffer != '\n') && len < 1024)
	  {
	    *name++ = *buffer++;
	    len++;
	  }
	if (len == 0)
	  break;

	if (*(name - 1) == 0xd)   /* gmc uses RETURN+NEWLINE as delimiter */
	  *(name - 1) = '\0';
	else
	  *name = '\0';

	name = name_buffer;

	if ((sig_len < len) && (! strncmp (name, data_type, sig_len)))
	  name += sig_len;

	if (name && strlen (name) > 2)
	  files = g_list_append (files, name);

	if (*buffer)
	  buffer++;
      }
  }

  if (files)
    {
      (* (GimpDndDropFileFunc) set_file_func) (widget, files,
					       set_file_data);

      g_list_free (files);
    }
}

void
gimp_dnd_file_dest_set (GtkWidget           *widget,
			GimpDndDropFileFunc  set_file_func,
			gpointer             data)
{
  gimp_dnd_data_dest_set (GIMP_DND_TYPE_URI_LIST, widget,
			  G_CALLBACK (set_file_func),
			  data);
  gimp_dnd_data_dest_set (GIMP_DND_TYPE_TEXT_PLAIN, widget,
			  G_CALLBACK (set_file_func),
			  data);
  gimp_dnd_data_dest_set (GIMP_DND_TYPE_NETSCAPE_URL, widget,
			  G_CALLBACK (set_file_func),
			  data);
}

void
gimp_dnd_file_dest_unset (GtkWidget *widget)
{
  gimp_dnd_data_dest_unset (GIMP_DND_TYPE_URI_LIST, widget);
  gimp_dnd_data_dest_unset (GIMP_DND_TYPE_TEXT_PLAIN, widget);
  gimp_dnd_data_dest_unset (GIMP_DND_TYPE_NETSCAPE_URL, widget);
}

void
gimp_dnd_open_files (GtkWidget *widget,
		     GList     *files,
		     gpointer   data)
{
  GList *list;

  for (list = files; list; list = g_list_next (list))
    {
      gchar *filename = (gchar *) list->data;

      file_open_with_display (the_gimp, filename);
    }
}


/*************************/
/*  color dnd functions  */
/*************************/

static GtkWidget *
gimp_dnd_get_color_icon (GtkWidget *widget,
			 GCallback  get_color_func,
			 gpointer   get_color_data)
{
  GtkWidget *color_area;
  GimpRGB    color;

  (* (GimpDndDragColorFunc) get_color_func) (widget, &color,
					     get_color_data);

  color_area = gimp_color_area_new (&color, TRUE, 0);
  gtk_widget_set_usize (color_area, DRAG_PREVIEW_SIZE, DRAG_PREVIEW_SIZE);

  return color_area;
}

static guchar *
gimp_dnd_get_color_data (GtkWidget *widget,
			 GCallback  get_color_func,
			 gpointer   get_color_data,
			 gint      *format,
			 gint      *length)
{
  guint16 *vals;
  GimpRGB  color;
  guchar   r, g, b, a;

  (* (GimpDndDragColorFunc) get_color_func) (widget, &color,
					     get_color_data);

  vals = g_new (guint16, 4);

  gimp_rgba_get_uchar (&color, &r, &g, &b, &a);

  vals[0] = r + (r << 8);
  vals[1] = g + (g << 8);
  vals[2] = b + (b << 8);
  vals[3] = a + (a << 8);

  *format = 16;
  *length = 8;

  return (guchar *) vals;
}

static void
gimp_dnd_set_color_data (GtkWidget *widget,
			 GCallback  set_color_func,
			 gpointer   set_color_data,
			 guchar    *vals,
			 gint       format,
			 gint       length)
{
  guint16 *color_vals;
  GimpRGB  color;

  if ((format != 16) || (length != 8))
    {
      g_warning ("Received invalid color data\n");
      return;
    }

  color_vals = (guint16 *) vals;

  gimp_rgba_set_uchar (&color,
		       (guchar) (color_vals[0] >> 8),
		       (guchar) (color_vals[1] >> 8),
		       (guchar) (color_vals[2] >> 8),
		       (guchar) (color_vals[3] >> 8));

  (* (GimpDndDropColorFunc) set_color_func) (widget, &color,
					     set_color_data);
}

void
gimp_dnd_color_source_set (GtkWidget            *widget,
			   GimpDndDragColorFunc  get_color_func,
			   gpointer              data)
{
  gimp_dnd_data_source_set (GIMP_DND_TYPE_COLOR, widget,
			    G_CALLBACK (get_color_func),
			    data);
}

void
gimp_dnd_color_dest_set (GtkWidget            *widget,
			 GimpDndDropColorFunc  set_color_func,
			 gpointer              data)
{
  gimp_dnd_data_dest_set (GIMP_DND_TYPE_COLOR, widget,
			  G_CALLBACK (set_color_func),
			  data);
}

void
gimp_dnd_color_dest_unset (GtkWidget *widget)
{
  gimp_dnd_data_dest_unset (GIMP_DND_TYPE_COLOR, widget);
}


/*******************************************/
/*  GimpViewable (by GType) dnd functions  */
/*******************************************/

static GtkWidget *
gimp_dnd_get_viewable_icon (GtkWidget *widget,
			    GCallback  get_viewable_func,
			    gpointer   get_viewable_data)
{
  GtkWidget    *preview;
  GimpViewable *viewable;

  viewable = (* (GimpDndDragViewableFunc) get_viewable_func) (widget,
							      get_viewable_data);

  if (! viewable)
    return NULL;

  preview = gimp_preview_new (viewable, DRAG_PREVIEW_SIZE, 0, TRUE);

  return preview;
}

static GimpDndType
gimp_dnd_data_type_get_by_g_type (GType type)
{
  GimpDndType dnd_type = GIMP_DND_TYPE_NONE;

  if (g_type_is_a (type, GIMP_TYPE_IMAGE))
    {
      dnd_type = GIMP_DND_TYPE_IMAGE;
    }
  else if (g_type_is_a (type, GIMP_TYPE_LAYER))
    {
      dnd_type = GIMP_DND_TYPE_LAYER;
    }
  else if (g_type_is_a (type, GIMP_TYPE_LAYER_MASK))
    {
      dnd_type = GIMP_DND_TYPE_LAYER_MASK;
    }
  else if (g_type_is_a (type, GIMP_TYPE_CHANNEL))
    {
      dnd_type = GIMP_DND_TYPE_CHANNEL;
    }
  else if (g_type_is_a (type, GIMP_TYPE_BRUSH))
    {
      dnd_type = GIMP_DND_TYPE_BRUSH;
  }
  else if (g_type_is_a (type, GIMP_TYPE_PATTERN))
    {
      dnd_type = GIMP_DND_TYPE_PATTERN;
    }
  else if (g_type_is_a (type, GIMP_TYPE_GRADIENT))
    {
      dnd_type = GIMP_DND_TYPE_GRADIENT;
    }
  else if (g_type_is_a (type, GIMP_TYPE_PALETTE))
    {
      dnd_type = GIMP_DND_TYPE_PALETTE;
    }
  else if (g_type_is_a (type, GIMP_TYPE_BUFFER))
    {
      dnd_type = GIMP_DND_TYPE_BUFFER;
    }
  else if (g_type_is_a (type, GIMP_TYPE_IMAGEFILE))
    {
      dnd_type = GIMP_DND_TYPE_IMAGEFILE;
    }
  else if (g_type_is_a (type, GIMP_TYPE_TOOL_INFO))
    {
      dnd_type = GIMP_DND_TYPE_TOOL;
    }
  else
    {
      g_warning ("%s(): unsupported GType \"%s\"",
		 G_GNUC_FUNCTION, g_type_name (type));
    }

  return dnd_type;
}

void
gimp_gtk_drag_source_set_by_type (GtkWidget       *widget,
				  GdkModifierType  start_button_mask,
				  GType            type,
				  GdkDragAction    actions)
{
  GimpDndType dnd_type;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  dnd_type = gimp_dnd_data_type_get_by_g_type (type);

  if (dnd_type == GIMP_DND_TYPE_NONE)
    return;

  gtk_drag_source_set (widget, start_button_mask,
		       &dnd_data_defs[dnd_type].target_entry,
		       1,
		       actions);
}

void
gimp_gtk_drag_dest_set_by_type (GtkWidget       *widget,
				GtkDestDefaults  flags,
				GType            type,
				GdkDragAction    actions)
{
  GimpDndType dnd_type;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  dnd_type = gimp_dnd_data_type_get_by_g_type (type);

  if (dnd_type == GIMP_DND_TYPE_NONE)
    return;

  gtk_drag_dest_set (widget, flags,
		     &dnd_data_defs[dnd_type].target_entry,
		     1,
		     actions);
}

void
gimp_dnd_viewable_source_set (GtkWidget               *widget,
			      GType                    type,
			      GimpDndDragViewableFunc  get_viewable_func,
			      gpointer                 data)
{
  GimpDndType dnd_type;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (get_viewable_func != NULL);

  dnd_type = gimp_dnd_data_type_get_by_g_type (type);

  if (dnd_type == GIMP_DND_TYPE_NONE)
    return;

  gimp_dnd_data_source_set (dnd_type, widget,
			    G_CALLBACK (get_viewable_func),
			    data);
}

void
gimp_dnd_viewable_source_unset (GtkWidget *widget,
				GType      type)
{
  GimpDndType dnd_type;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  dnd_type = gimp_dnd_data_type_get_by_g_type (type);

  if (dnd_type == GIMP_DND_TYPE_NONE)
    return;

  gimp_dnd_data_source_unset (widget);
}

void
gimp_dnd_viewable_dest_set (GtkWidget               *widget,
			    GType                    type,
			    GimpDndDropViewableFunc  set_viewable_func,
			    gpointer                 data)
{
  GimpDndType dnd_type;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (set_viewable_func != NULL);

  dnd_type = gimp_dnd_data_type_get_by_g_type (type);

  if (dnd_type == GIMP_DND_TYPE_NONE)
    return;

  gimp_dnd_data_dest_set (dnd_type, widget,
			  G_CALLBACK (set_viewable_func),
			  data);  
}

void
gimp_dnd_viewable_dest_unset (GtkWidget *widget,
			      GType      type)
{
  GimpDndType dnd_type;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  dnd_type = gimp_dnd_data_type_get_by_g_type (type);

  if (dnd_type == GIMP_DND_TYPE_NONE)
    return;

  gimp_dnd_data_dest_unset (dnd_type, widget);
}

GimpViewable *
gimp_dnd_get_drag_data (GtkWidget *widget)
{
  GimpDndType              data_type;
  GimpDndDragViewableFunc  get_data_func;
  gpointer                 get_data_data;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  data_type =
    (GimpDndType) g_object_get_data (G_OBJECT (widget),
                                     "gimp-dnd-get-data-type");

  if (! data_type)
    return NULL;

  get_data_func = 
    (GimpDndDragViewableFunc) g_object_get_data (G_OBJECT (widget),
                                                 "gimp-dnd-get-data-func");
  get_data_data = 
    (gpointer) g_object_get_data (G_OBJECT (widget),
                                  "gimp-dnd-get-data-data");

  if (! get_data_func)
    return NULL;

  return (GimpViewable *) (* get_data_func) (widget, get_data_data);
 
}


/*************************/
/*  image dnd functions  */
/*************************/

static guchar *
gimp_dnd_get_image_data (GtkWidget *widget,
			 GCallback  get_image_func,
			 gpointer   get_image_data,
			 gint      *format,
			 gint      *length)
{
  GimpImage *gimage;
  gchar     *id;

  gimage = (GimpImage *)
    (* (GimpDndDragViewableFunc) get_image_func) (widget, get_image_data);

  if (! gimage)
    return NULL;

  id = g_strdup_printf ("%d", gimp_image_get_ID (gimage));

  *format = 8;
  *length = strlen (id) + 1;

  return (guchar *) id;
}

static void
gimp_dnd_set_image_data (GtkWidget *widget,
			 GCallback  set_image_func,
			 gpointer   set_image_data,
			 guchar    *vals,
			 gint       format,
			 gint       length)
{
  GimpImage *gimage;
  gchar     *id;
  gint       ID;

  if ((format != 8) || (length < 1))
    {
      g_warning ("%s(): received invalid image ID data", G_GNUC_FUNCTION);
      return;
    }

  id = (gchar *) vals;

  ID = atoi (id);

  if (! ID)
    return;

  gimage = gimp_image_get_by_ID (the_gimp, ID);

  if (gimage)
    (* (GimpDndDropViewableFunc) set_image_func) (widget,
						  GIMP_VIEWABLE (gimage),
						  set_image_data);
}


/****************************/
/*  drawable dnd functions  */
/****************************/

static guchar *
gimp_dnd_get_drawable_data (GtkWidget *widget,
			    GCallback  get_drawable_func,
			    gpointer   get_drawable_data,
			    gint      *format,
			    gint      *length)
{
  GimpDrawable *drawable;
  gchar        *id;

  drawable = (GimpDrawable *)
    (* (GimpDndDragViewableFunc) get_drawable_func) (widget, get_drawable_data);

  if (! drawable)
    return NULL;

  id = g_strdup_printf ("%d", gimp_drawable_get_ID (drawable));

  *format = 8;
  *length = strlen (id) + 1;

  return (guchar *) id;
}

static void
gimp_dnd_set_drawable_data (GtkWidget *widget,
			    GCallback  set_drawable_func,
			    gpointer   set_drawable_data,
			    guchar    *vals,
			    gint       format,
			    gint       length)
{
  GimpDrawable *drawable;
  gchar        *id;
  gint          ID;

  if ((format != 8) || (length < 1))
    {
      g_warning ("Received invalid drawable ID data");
      return;
    }

  id = (gchar *) vals;

  ID = atoi (id);

  if (! ID)
    return;

  drawable = gimp_drawable_get_by_ID (the_gimp, ID);

  if (drawable)
    (* (GimpDndDropViewableFunc) set_drawable_func) (widget,
						     GIMP_VIEWABLE (drawable),
						     set_drawable_data);
}


/****************************/
/*  GimpData dnd functions  */
/****************************/

static guchar *
gimp_dnd_get_data_data (GtkWidget *widget,
			GCallback  get_data_func,
			gpointer   get_data_data,
			gint      *format,
			gint      *length)
{
  GimpData *data;
  gchar    *name;

  data = (GimpData *)
    (* (GimpDndDragViewableFunc) get_data_func) (widget, get_data_data);

  if (! data)
    return NULL;

  name = g_strdup (gimp_object_get_name (GIMP_OBJECT (data)));

  if (! name)
    return NULL;

  *format = 8;
  *length = strlen (name) + 1;

  return (guchar *) name;
}


/*************************/
/*  brush dnd functions  */
/*************************/

static void
gimp_dnd_set_brush_data (GtkWidget *widget,
			 GCallback  set_brush_func,
			 gpointer   set_brush_data,
			 guchar    *vals,
			 gint       format,
			 gint       length)
{
  GimpBrush *brush;
  gchar     *name;

  if ((format != 8) || (length < 1))
    {
      g_warning ("Received invalid brush data\n");
      return;
    }

  name = (gchar *) vals;

  g_print ("gimp_dnd_set_brush_data() got >>%s<<\n", name);

  if (strcmp (name, "Standard") == 0)
    brush = GIMP_BRUSH (gimp_brush_get_standard ());
  else
    brush = (GimpBrush *)
      gimp_container_get_child_by_name (the_gimp->brush_factory->container,
					name);

  if (brush)
    (* (GimpDndDropViewableFunc) set_brush_func) (widget,
						  GIMP_VIEWABLE (brush),
						  set_brush_data);
}


/***************************/
/*  pattern dnd functions  */
/***************************/

static void
gimp_dnd_set_pattern_data (GtkWidget *widget,
			   GCallback  set_pattern_func,
			   gpointer   set_pattern_data,
			   guchar    *vals,
			   gint       format,
			   gint       length)
{
  GimpPattern *pattern;
  gchar       *name;

  if ((format != 8) || (length < 1))
    {
      g_warning ("Received invalid pattern data\n");
      return;
    }

  name = (gchar *) vals;

  if (strcmp (name, "Standard") == 0)
    pattern = GIMP_PATTERN (gimp_pattern_get_standard ());
  else
    pattern = (GimpPattern *)
      gimp_container_get_child_by_name (the_gimp->pattern_factory->container,
					name);

  if (pattern)
    (* (GimpDndDropViewableFunc) set_pattern_func) (widget,
						    GIMP_VIEWABLE (pattern),
						    set_pattern_data);
}


/****************************/
/*  gradient dnd functions  */
/****************************/

static void
gimp_dnd_set_gradient_data (GtkWidget *widget,
			    GCallback  set_gradient_func,
			    gpointer   set_gradient_data,
			    guchar    *vals,
			    gint       format,
			    gint       length)
{
  GimpGradient *gradient;
  gchar        *name;

  if ((format != 8) || (length < 1))
    {
      g_warning ("Received invalid gradient data\n");
      return;
    }

  name = (gchar *) vals;

  if (strcmp (name, "Standard") == 0)
    gradient = GIMP_GRADIENT (gimp_gradient_get_standard ());
  else
    gradient = (GimpGradient *)
      gimp_container_get_child_by_name (the_gimp->gradient_factory->container,
					name);

  if (gradient)
    (* (GimpDndDropViewableFunc) set_gradient_func) (widget,
						     GIMP_VIEWABLE (gradient),
						     set_gradient_data);
}


/***************************/
/*  palette dnd functions  */
/***************************/

static void
gimp_dnd_set_palette_data (GtkWidget *widget,
			   GCallback  set_palette_func,
			   gpointer   set_palette_data,
			   guchar    *vals,
			   gint       format,
			   gint       length)
{
  GimpPalette *palette;
  gchar       *name;

  if ((format != 8) || (length < 1))
    {
      g_warning ("Received invalid palette data\n");
      return;
    }

  name = (gchar *) vals;

  if (strcmp (name, "Standard") == 0)
    palette = GIMP_PALETTE (gimp_palette_get_standard ());
  else
    palette = (GimpPalette *)
      gimp_container_get_child_by_name (the_gimp->palette_factory->container,
					name);

  if (palette)
    (* (GimpDndDropViewableFunc) set_palette_func) (widget,
						    GIMP_VIEWABLE (palette),
						    set_palette_data);
}


/**************************/
/*  buffer dnd functions  */
/**************************/

static void
gimp_dnd_set_buffer_data (GtkWidget *widget,
			  GCallback  set_buffer_func,
			  gpointer   set_buffer_data,
			  guchar    *vals,
			  gint       format,
			  gint       length)
{
  GimpBuffer *buffer;
  gchar      *name;

  if ((format != 8) || (length < 1))
    {
      g_warning ("Received invalid buffer data\n");
      return;
    }

  name = (gchar *) vals;

  buffer = (GimpBuffer *)
    gimp_container_get_child_by_name (the_gimp->named_buffers, name);

  if (buffer)
    (* (GimpDndDropViewableFunc) set_buffer_func) (widget,
						   GIMP_VIEWABLE (buffer),
						   set_buffer_data);
}


/*****************************/
/*  imagefile dnd functions  */
/*****************************/

static void
gimp_dnd_set_imagefile_data (GtkWidget *widget,
			     GCallback  set_imagefile_func,
			     gpointer   set_imagefile_data,
			     guchar    *vals,
			     gint       format,
			     gint       length)
{
  GimpImagefile *imagefile;
  gchar         *name;

  if ((format != 8) || (length < 1))
    {
      g_warning ("Received invalid buffer data\n");
      return;
    }

  name = (gchar *) vals;

  imagefile = (GimpImagefile *)
    gimp_container_get_child_by_name (the_gimp->documents, name);

  if (imagefile)
    (* (GimpDndDropViewableFunc) set_imagefile_func) (widget,
						      GIMP_VIEWABLE (imagefile),
						      set_imagefile_data);
}


/************************/
/*  tool dnd functions  */
/************************/

static void
gimp_dnd_set_tool_data (GtkWidget *widget,
			GCallback  set_tool_func,
			gpointer   set_tool_data,
			guchar    *vals,
			gint       format,
			gint       length)
{
  GimpToolInfo *tool_info;
  gchar        *name;

  if ((format != 8) || (length < 1))
    {
      g_warning ("Received invalid tool data\n");
      return;
    }

  name = (gchar *) vals;

  g_print ("gimp_dnd_set_tool_data() got >>%s<<\n", name);

  if (strcmp (name, "gimp:standard_tool") == 0)
    tool_info = gimp_tool_info_get_standard (the_gimp);
  else
    tool_info = (GimpToolInfo *)
      gimp_container_get_child_by_name (the_gimp->tool_info_list,
					name);

  if (tool_info)
    (* (GimpDndDropViewableFunc) set_tool_func) (widget,
						 GIMP_VIEWABLE (tool_info),
						 set_tool_data);
}
