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

#include "config.h"

#include <stdlib.h>

#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"

#include "gui-types.h"

#include "splash.h"

#include "gimp-intl.h"


typedef struct
{
  GtkWidget      *window;
  GtkWidget      *area;
  gint            width;
  gint            height;
  GtkWidget      *progress;
  GdkPixmap      *pixmap;
  PangoLayout    *upper;
  gint            upper_x, upper_y;
  PangoLayout    *lower;
  gint            lower_x, lower_y;
} GimpSplash;

static GimpSplash *splash = NULL;


static void      splash_map         (void);
static gboolean  splash_area_expose (GtkWidget      *widget,
                                     GdkEventExpose *event,
                                     GimpSplash     *splash);


/*  public functions  */

void
splash_create (void)
{
  GtkWidget      *frame;
  GtkWidget      *vbox;
  GdkPixbuf      *pixbuf;
  PangoAttrList  *attrs;
  PangoAttribute *attr;
  gchar          *filename;

  g_return_if_fail (splash == NULL);

  filename = gimp_personal_rc_file ("gimp-splash.png");
  pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
  g_free (filename);

  if (! pixbuf)
    {
      filename = g_build_filename (gimp_data_directory (),
                                   "images", "gimp-splash.png",
                                   NULL);
      pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
      g_free (filename);
    }

  if (! pixbuf)
    return;

  splash = g_new0 (GimpSplash, 1);

  splash->width  = gdk_pixbuf_get_width (pixbuf);
  splash->height = gdk_pixbuf_get_height (pixbuf);

  splash->window =
    g_object_new (GTK_TYPE_WINDOW,
                  "type",            GTK_WINDOW_TOPLEVEL,
                  "type_hint",       GDK_WINDOW_TYPE_HINT_SPLASHSCREEN,
                  "title",           _("GIMP Startup"),
                  "role",            "gimp-startup",
                  "window_position", GTK_WIN_POS_CENTER,
                  "resizable",       FALSE,
                  NULL);

  g_signal_connect_swapped (splash->window, "delete_event",
                            G_CALLBACK (exit),
                            GINT_TO_POINTER (0));

  /* we don't want the splash screen to send the startup notification */
  gtk_window_set_auto_startup_notification (FALSE);
  g_signal_connect (splash->window, "map",
                    G_CALLBACK (splash_map),
                    NULL);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER (splash->window), frame);
  gtk_widget_show (frame);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);

  splash->area = gtk_drawing_area_new ();
  gtk_box_pack_start_defaults (GTK_BOX (vbox), splash->area);
  gtk_widget_show (splash->area);

  gtk_widget_set_size_request (splash->area, splash->width, splash->height);

  gtk_widget_realize (splash->area);
  splash->pixmap = gdk_pixmap_new (splash->area->window,
                                   splash->width, splash->height, -1);
  gdk_draw_pixbuf (splash->pixmap, splash->area->style->black_gc,
                   pixbuf, 0, 0, 0, 0, splash->width, splash->height,
                   GDK_RGB_DITHER_NORMAL, 0, 0);
  g_object_unref (pixbuf);

  g_signal_connect (splash->area, "expose_event",
                    G_CALLBACK (splash_area_expose),
                    splash);

  splash->upper = gtk_widget_create_pango_layout (splash->area, "");
  splash->lower = gtk_widget_create_pango_layout (splash->area, "");

  attrs = pango_attr_list_new ();
  attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
  attr->start_index = 0;
  attr->end_index   = -1;
  pango_attr_list_insert (attrs, attr);

  pango_layout_set_attributes (splash->upper, attrs);
  pango_attr_list_unref (attrs);

  splash->progress = gtk_progress_bar_new ();
  gtk_box_pack_end (GTK_BOX (vbox), splash->progress, FALSE, FALSE, 0);
  gtk_widget_show (splash->progress);

  gtk_widget_show (splash->window);

  while (gtk_events_pending ())
    gtk_main_iteration ();
}

void
splash_destroy (void)
{
  if (! splash)
    return;

  gtk_widget_destroy (splash->window);

  g_object_unref (splash->pixmap);
  g_object_unref (splash->upper);
  g_object_unref (splash->lower);

  g_free (splash);
  splash = NULL;
}

void
splash_update (const gchar *text1,
	       const gchar *text2,
	       gdouble      percentage)
{
  gint y = 0;

  if (! splash)
    return;

  if (text1)
    {
      PangoRectangle  rect;

      pango_layout_set_text (splash->upper, text1, -1);
      pango_layout_get_pixel_extents (splash->upper, NULL, &rect);

      splash->upper_x = (splash->width - rect.width) / 2;
      splash->upper_y = splash->height - 2 * (rect.height + 8);

      y = splash->upper_y - 2;
    }

  if (text2)
    {
      PangoRectangle  rect;

      pango_layout_set_text (splash->lower, text2, -1);
      pango_layout_get_pixel_extents (splash->lower, NULL, &rect);

      splash->lower_x = (splash->width - rect.width) / 2;
      splash->lower_y = splash->height - (rect.height + 8);

      if (!y)
        y = splash->lower_y - 2;
    }

  /*  The area that needs exposore could be calculated more precisely,
   *  but it would probably make this code an unreadable mess. So we
   *  use a cheap approximation instead.
   */
  if (y)
    gtk_widget_queue_draw_area (splash->area,
                                0, y, splash->width, splash->height - y);

  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (splash->progress),
                                 CLAMP (percentage, 0.0, 1.0));

  while (gtk_events_pending ())
    gtk_main_iteration ();
}


/*  private functions  */


static gboolean
splash_area_expose (GtkWidget      *widget,
                    GdkEventExpose *event,
                    GimpSplash     *splash)
{
  gint x = (widget->allocation.width  - splash->width)  / 2;
  gint y = (widget->allocation.height - splash->height) / 2;

  gdk_gc_set_clip_rectangle (widget->style->black_gc, &event->area);

  gdk_draw_drawable (widget->window,
                     widget->style->black_gc,
                     splash->pixmap, 0, 0,
                     x, y, splash->width, splash->height);

  gdk_draw_layout (widget->window, widget->style->black_gc,
                   x + splash->upper_x, y + splash->upper_y, splash->upper);

  gdk_draw_layout (widget->window, widget->style->black_gc,
                   x + splash->lower_x, y + splash->lower_y, splash->lower);

  gdk_gc_set_clip_rectangle (widget->style->black_gc, NULL);

  return FALSE;
}

static void
splash_map (void)
{
  /*  Reenable startup notification after the splash has been shown
   *  so that the next window that is mapped sends the notification.
   */
   gtk_window_set_auto_startup_notification (TRUE);
}
