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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "libgimpmath/gimpmath.h"
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "gui-types.h"

#include "core/gimptoolinfo.h"

#include "widgets/gimphelp-ids.h"
#include "widgets/gimpdnd.h"

#include "about-dialog.h"
#include "authors.h"

#include "gimp-intl.h"

static gchar *founders[] =
{
  N_("Version %s brought to you by"),
  "Spencer Kimball & Peter Mattis"
};

static gchar *translators[] =
{
  N_("Translation by"),
  /* Translators: insert your names here, separated by newline */
  /* we'd prefer just the names, please no email adresses.     */
  N_("translator-credits"),
};

static gchar *contri_intro[] =
{
  N_("Contributions by")
};

static gchar **translator_names = NULL;

typedef struct
{
  GtkWidget    *about_dialog;
  GtkWidget    *logo_area;
  GdkPixmap    *logo_pixmap;
  GdkRectangle  pixmaparea;

  GdkBitmap    *shape_bitmap;
  GdkGC        *trans_gc;
  GdkGC        *opaque_gc;

  GdkRectangle  textarea;
  gdouble       text_size;
  gdouble       min_text_size;
  PangoLayout  *layout;

  gint          timer;

  gint          index;
  gint          animstep;
  gboolean      visible;
  gint          textrange[2];
  gint          state;

} GimpAboutInfo;

PangoColor gradient[] =
{
  { 50372, 50372, 50115 },
  { 65535, 65535, 65535 },
  { 10000, 10000, 10000 },
};

PangoColor foreground = { 10000, 10000, 10000 };
PangoColor background = { 50372, 50372, 50115 };

/* backup values */

PangoColor grad1ent[] =
{
  { 0xff * 257, 0xba * 257, 0x00 * 257 },
  { 37522, 51914, 57568 },
};

PangoColor foregr0und = { 37522, 51914, 57568 };
PangoColor backgr0und = { 0, 0, 0 };

static GimpAboutInfo about_info = { 0 };
static gboolean pp = FALSE;

static gboolean  about_dialog_load_logo   (GtkWidget      *window);
static void      about_dialog_destroy     (GtkObject      *object,
                                           gpointer        data);
static void      about_dialog_unmap       (GtkWidget      *widget,
                                           gpointer        data);
static gint      about_dialog_logo_expose (GtkWidget      *widget,
                                           GdkEventExpose *event,
                                           gpointer        data);
static gint      about_dialog_button      (GtkWidget      *widget,
                                           GdkEventButton *event,
                                           gpointer        data);
static gint      about_dialog_key         (GtkWidget      *widget,
                                           GdkEventKey    *event,
                                           gpointer        data);
static void      reshuffle_array          (void);
static gboolean  about_dialog_timer       (gpointer        data);


static gboolean     double_speed     = FALSE;

static PangoFontDescription *font_desc = NULL;
static gchar      **scroll_text      = authors;
static gint         nscroll_texts    = G_N_ELEMENTS (authors);
static gint         shuffle_array[G_N_ELEMENTS (authors)];


GtkWidget *
about_dialog_create (void)
{
  if (! about_info.about_dialog)
    {
      GtkWidget *widget;
      GdkGCValues shape_gcv;

      about_info.visible = FALSE;
      about_info.state = 0;
      about_info.animstep = -1;

      widget = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      about_info.about_dialog = widget;

      gtk_window_set_type_hint (GTK_WINDOW (widget),
                                GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);
      gtk_window_set_wmclass (GTK_WINDOW (widget), "about_dialog", "Gimp");
      gtk_window_set_title (GTK_WINDOW (widget), _("About The GIMP"));
      gtk_window_set_position (GTK_WINDOW (widget), GTK_WIN_POS_CENTER);

      /* The window must not be resizeable, since otherwise
       * the copying of nonexisting parts of the image pixmap
       * would result in an endless loop due to the X-Server
       * generating expose events on the pixmap. */

      gtk_window_set_resizable (GTK_WINDOW (widget), FALSE);

      g_signal_connect (widget, "destroy",
                        G_CALLBACK (about_dialog_destroy),
                        NULL);
      g_signal_connect (widget, "unmap",
                        G_CALLBACK (about_dialog_unmap),
                        NULL);
      g_signal_connect (widget, "button_press_event",
                        G_CALLBACK (about_dialog_button),
                        NULL);
      g_signal_connect (widget, "key_press_event",
                        G_CALLBACK (about_dialog_key),
                        NULL);
      
      gtk_widget_set_events (widget, GDK_BUTTON_PRESS_MASK);

      if (! about_dialog_load_logo (widget))
        {
	  gtk_widget_destroy (widget);
	  about_info.about_dialog = NULL;
	  return NULL;
	}

      /* place the scrolltext at the bottom of the image */
      about_info.textarea.width = about_info.pixmaparea.width;
      about_info.textarea.height = 50;
      about_info.textarea.x = 0;
      about_info.textarea.y = (about_info.pixmaparea.height -
                               about_info.textarea.height);

      widget = gtk_drawing_area_new ();
      about_info.logo_area = widget;

      gtk_widget_set_size_request (widget,
                                   about_info.pixmaparea.width,
                                   about_info.pixmaparea.height);
      gtk_widget_set_events (widget, GDK_EXPOSURE_MASK);
      gtk_container_add (GTK_CONTAINER (about_info.about_dialog),
                         widget);
      gtk_widget_show (widget);

      g_signal_connect (widget, "expose_event",
			G_CALLBACK (about_dialog_logo_expose),
			NULL);

      gtk_widget_realize (widget);
      gdk_window_set_background (widget->window,
                                 &(widget->style)->black);

      /* setup shape bitmap */

      about_info.shape_bitmap = gdk_pixmap_new (widget->window,
                                                about_info.pixmaparea.width,
                                                about_info.pixmaparea.height,
                                                1);
      about_info.trans_gc = gdk_gc_new (about_info.shape_bitmap);

      about_info.opaque_gc = gdk_gc_new (about_info.shape_bitmap);
      gdk_gc_get_values (about_info.opaque_gc, &shape_gcv);
      gdk_gc_set_foreground (about_info.opaque_gc, &shape_gcv.background);

      gdk_draw_rectangle (about_info.shape_bitmap,
                          about_info.trans_gc,
                          TRUE,
                          0, 0,
                          about_info.pixmaparea.width,
                          about_info.pixmaparea.height);

      gdk_draw_line (about_info.shape_bitmap,
                     about_info.opaque_gc,
                     0, 0,
                     about_info.pixmaparea.width, 0);

      gtk_widget_shape_combine_mask (about_info.about_dialog,
                                     about_info.shape_bitmap,
                                     0, 0);
      about_info.layout = gtk_widget_create_pango_layout (about_info.logo_area,
                                                          NULL);
      g_object_weak_ref (G_OBJECT (about_info.logo_area), 
                         (GWeakNotify) g_object_unref, about_info.layout);

      font_desc = pango_font_description_from_string ("Sans, 11");

      pango_layout_set_font_description (about_info.layout, font_desc);
      pango_layout_set_justify (about_info.layout, PANGO_ALIGN_CENTER);

    }

  if (! GTK_WIDGET_VISIBLE (about_info.about_dialog))
    {
      if (! double_speed)
	{
          about_info.state = 0;
          about_info.index = 0;

          reshuffle_array ();
          pango_layout_set_text (about_info.layout, "", -1);
	}
    }

  gtk_window_present (GTK_WINDOW (about_info.about_dialog));

  return about_info.about_dialog;
}

static void
about_dialog_destroy (GtkObject *object,
		      gpointer   data)
{
  about_info.about_dialog = NULL;
  about_dialog_unmap (NULL, NULL);
}

static void
about_dialog_unmap (GtkWidget *widget,
                    gpointer   data)
{
  if (about_info.timer)
    {
      g_source_remove (about_info.timer);
      about_info.timer = 0;
    }

  if (about_info.about_dialog)
    {
      gdk_draw_rectangle (about_info.shape_bitmap,
                          about_info.trans_gc,
                          TRUE,
                          0, 0,
                          about_info.pixmaparea.width,
                          about_info.pixmaparea.height);

      gdk_draw_line (about_info.shape_bitmap,
                     about_info.opaque_gc,
                     0, 0,
                     about_info.pixmaparea.width, 0);

      gtk_widget_shape_combine_mask (about_info.about_dialog,
                                     about_info.shape_bitmap,
                                     0, 0);
    }
}

static gint
about_dialog_logo_expose (GtkWidget      *widget,
			  GdkEventExpose *event,
			  gpointer        data)
{
  gint width, height;

  if (!about_info.timer)
    {
      GdkModifierType mask;

      /* No timeout, we were unmapped earlier */
      about_info.state = 0;
      about_info.index = 1;
      about_info.animstep = -1;
      about_info.visible = FALSE;
      reshuffle_array ();
      gdk_draw_rectangle (about_info.shape_bitmap,
                          about_info.trans_gc,
                          TRUE,
                          0, 0,
                          about_info.pixmaparea.width,
                          about_info.pixmaparea.height);

      gdk_draw_line (about_info.shape_bitmap,
                     about_info.opaque_gc,
                     0, 0,
                     about_info.pixmaparea.width, 0);

      gtk_widget_shape_combine_mask (about_info.about_dialog,
                                     about_info.shape_bitmap,
                                     0, 0);
      about_info.timer = g_timeout_add (30, about_dialog_timer, NULL);

      gdk_window_get_pointer (NULL, NULL, NULL, &mask);

      /* weird magic to determine the way the logo should be shown */
      mask &= ~GDK_BUTTON3_MASK;
      pp = (mask &= (GDK_SHIFT_MASK | GDK_CONTROL_MASK) & 
                    (GDK_CONTROL_MASK | GDK_MOD1_MASK) &
                    (GDK_MOD1_MASK | ~GDK_SHIFT_MASK),
      height = mask ? (about_info.pixmaparea.height > 0) &&
                      (about_info.pixmaparea.width > 0): 0);

    }

  /* only operate on the region covered by the pixmap */
  if (! gdk_rectangle_intersect (&(about_info.pixmaparea),
                                 &(event->area),
                                 &(event->area)))
    return FALSE;

  gdk_gc_set_clip_rectangle (widget->style->black_gc, &(event->area));

  gdk_draw_drawable (widget->window,
                     widget->style->white_gc,
                     about_info.logo_pixmap,
                     event->area.x, event->area.y +
                     (pp ? about_info.pixmaparea.height : 0),
                     event->area.x, event->area.y,
                     event->area.width, event->area.height);

  if (pp && about_info.state == 0 &&
      (about_info.index < about_info.pixmaparea.height / 12 ||
       about_info.index < g_random_int () %
                          (about_info.pixmaparea.height / 8 + 13)))
    {
      gdk_draw_rectangle (widget->window,
                          widget->style->black_gc,
                          TRUE,
                          0, 0, about_info.pixmaparea.width, 158);
                          
    }

  if (about_info.visible == TRUE)
    {
      gint layout_x, layout_y;

      pango_layout_get_pixel_size (about_info.layout, &width, &height);

      layout_x = about_info.textarea.x +
                        (about_info.textarea.width - width) / 2;
      layout_y = about_info.textarea.y +
                        (about_info.textarea.height - height) / 2;
      
      if (about_info.textrange[1] > 0)
        {
          GdkRegion *covered_region = NULL;
          GdkRegion *rect_region;

          covered_region = gdk_pango_layout_get_clip_region
                                    (about_info.layout,
                                     layout_x, layout_y,
                                     about_info.textrange, 1);

          rect_region = gdk_region_rectangle (&(event->area));

          gdk_region_intersect (covered_region, rect_region);
          gdk_region_destroy (rect_region);

          gdk_gc_set_clip_region (about_info.logo_area->style->text_gc[GTK_STATE_NORMAL],
                                  covered_region);
          gdk_region_destroy (covered_region);
        }

      gdk_draw_layout (widget->window,
                       widget->style->text_gc[GTK_STATE_NORMAL],
                       layout_x, layout_y,
                       about_info.layout);

    }

  gdk_gc_set_clip_rectangle (widget->style->black_gc, NULL);

  return FALSE;
}

static gint
about_dialog_button (GtkWidget      *widget,
		     GdkEventButton *event,
		     gpointer        data)
{
  gtk_widget_hide (about_info.about_dialog);

  return FALSE;
}

static gint
about_dialog_key (GtkWidget      *widget,
		  GdkEventKey    *event,
		  gpointer        data)
{
  /* placeholder */
  switch (event->keyval)
    {
    case GDK_a:
    case GDK_A:
    case GDK_b:
    case GDK_B:
    default:
        break;
    }
  
  return FALSE;
}

static gchar *
insert_spacers (const gchar *string)
{
  gchar *normalized, *ptr;
  gunichar unichr;
  GString *str;

  str = g_string_new (NULL);

  normalized = g_utf8_normalize (string, -1, G_NORMALIZE_DEFAULT_COMPOSE);
  ptr = normalized;

  while ((unichr = g_utf8_get_char (ptr)))
    {
      g_string_append_unichar (str, unichr);
      g_string_append_unichar (str, 0x200b);  /* ZERO WIDTH SPACE */
      ptr = g_utf8_next_char (ptr);
    }

  g_free (normalized);

  return g_string_free (str, FALSE);
}

static void
mix_gradient (PangoColor *gradient, guint ncolors,
              PangoColor *target, gdouble pos)
{
  gint index;

  g_return_if_fail (gradient != NULL);
  g_return_if_fail (ncolors > 1);
  g_return_if_fail (target != NULL);
  g_return_if_fail (pos >= 0.0  &&  pos <= 1.0);

  if (pos == 1.0)
    {
      target->red   = gradient[ncolors-1].red;
      target->green = gradient[ncolors-1].green;
      target->blue  = gradient[ncolors-1].blue;
      return;
    }
  
  index = (int) floor (pos * (ncolors-1));
  pos = pos * (ncolors - 1) - index;

  target->red   = gradient[index].red   * (1.0 - pos) + gradient[index+1].red   * pos;
  target->green = gradient[index].green * (1.0 - pos) + gradient[index+1].green * pos;
  target->blue  = gradient[index].blue  * (1.0 - pos) + gradient[index+1].blue  * pos;

}

static void
mix_colors (PangoColor *start, PangoColor *end,
            PangoColor *target,
            gdouble pos)
{
  g_return_if_fail (start != NULL);
  g_return_if_fail (end != NULL);
  g_return_if_fail (target != NULL);
  g_return_if_fail (pos >= 0.0  &&  pos <= 1.0);

  target->red   = start->red   * (1.0 - pos) + end->red   * pos;
  target->green = start->green * (1.0 - pos) + end->green * pos;
  target->blue  = start->blue  * (1.0 - pos) + end->blue  * pos;
}

static void
reshuffle_array (void)
{
  GRand *gr = g_rand_new ();
  gint i;

  for (i = 0; i < nscroll_texts; i++) 
    {
      shuffle_array[i] = i;
    }

  for (i = 0; i < nscroll_texts; i++) 
    {
      gint j;

      j = g_rand_int_range (gr, 0, nscroll_texts);
      if (i != j) 
        {
          gint t;

          t = shuffle_array[j];
          shuffle_array[j] = shuffle_array[i];
          shuffle_array[i] = t;
        }
    }

  g_rand_free (gr);
}

static void
decorate_text (PangoLayout *layout, gint anim_type, gdouble time)
{
  gint letter_count = 0;
  gint text_length = 0;
  gint text_bytelen = 0;
  gint cluster_start, cluster_end;
  const gchar *text;
  const gchar *ptr;
  gunichar unichr;
  PangoAttrList *attrlist = NULL;
  PangoAttribute *attr;
  PangoRectangle irect = {0, 0, 0, 0};
  PangoRectangle lrect = {0, 0, 0, 0};
  PangoColor mix;

  mix_colors (&(pp ? backgr0und : background),
              &(pp ? foregr0und : foreground),
              &mix, time);

  text = pango_layout_get_text (layout);
  text_length = g_utf8_strlen (text, -1);
  text_bytelen = strlen (text);

  attrlist = pango_attr_list_new ();

  about_info.textrange[0] = 0;
  about_info.textrange[1] = text_bytelen;

  switch (anim_type)
  {
    case 0: /* Fade in */
      attr = pango_attr_foreground_new (mix.red, mix.green, mix.blue);
      attr->start_index = 0;
      attr->end_index = text_bytelen;
      pango_attr_list_insert (attrlist, attr);
      break;

    case 1: /* Fade in, spread */
      attr = pango_attr_foreground_new (mix.red, mix.green, mix.blue);
      attr->start_index = 0;
      attr->end_index = text_bytelen;
      pango_attr_list_change (attrlist, attr);

      ptr = text;

      cluster_start = 0;
      while ((unichr = g_utf8_get_char (ptr)))
        {
          ptr = g_utf8_next_char (ptr);
          cluster_end = (ptr - text);

          if (unichr == 0x200b)
            {
              lrect.width = (1.0 - time) * 15.0 * PANGO_SCALE + 0.5;
              attr = pango_attr_shape_new (&irect, &lrect);
              attr->start_index = cluster_start;
              attr->end_index = cluster_end;
              pango_attr_list_change (attrlist, attr);
            }
          cluster_start = cluster_end;
        }
      break;

    case 2: /* Fade in, sinewave */
      attr = pango_attr_foreground_new (mix.red, mix.green, mix.blue);
      attr->start_index = 0;
      attr->end_index = text_bytelen;
      pango_attr_list_change (attrlist, attr);

      ptr = text;

      cluster_start = 0;
      while ((unichr = g_utf8_get_char (ptr)))
        {
          if (unichr == 0x200b)
            {
              cluster_end = ptr - text;
              attr = pango_attr_rise_new ((1.0 -time) * 18000 *
                                          sin (4.0 * time +
                                               (float) letter_count * 0.7));
              attr->start_index = cluster_start;
              attr->end_index = cluster_end;
              pango_attr_list_change (attrlist, attr);

              letter_count++;
              cluster_start = cluster_end;
            }

          ptr = g_utf8_next_char (ptr);
        }
      break;

    case 3: /* letterwise Fade in */
      ptr = text;

      letter_count = 0;
      cluster_start = 0;
      while ((unichr = g_utf8_get_char (ptr)))
        {
          gint border;
          gdouble pos;

          border = (text_length + 15) * time - 15;

          if (letter_count < border)
            pos = 0;
          else if (letter_count > border + 15)
            pos = 1;
          else
            pos = ((gdouble) (letter_count - border)) / 15;

          mix_colors (&(pp ? foregr0und : foreground),
                      &(pp ? backgr0und : background),
                      &mix, pos);

          ptr = g_utf8_next_char (ptr);

          cluster_end = ptr - text;

          attr = pango_attr_foreground_new (mix.red, mix.green, mix.blue);
          attr->start_index = cluster_start;
          attr->end_index = cluster_end;
          pango_attr_list_change (attrlist, attr);

          if (pos < 1.0)
            about_info.textrange[1] = cluster_end;

          letter_count++;
          cluster_start = cluster_end;
        }

      break;

    case 4: /* letterwise Fade in, triangular */
      ptr = text;

      letter_count = 0;
      cluster_start = 0;
      while ((unichr = g_utf8_get_char (ptr)))
        {
          gint border;
          gdouble pos;

          border = (text_length + 15) * time - 15;

          if (letter_count < border)
            pos = 1.0;
          else if (letter_count > border + 15)
            pos = 0.0;
          else
            pos = 1.0 - ((gdouble) (letter_count - border)) / 15;

          mix_gradient (pp ? grad1ent : gradient,
                        pp ? G_N_ELEMENTS (grad1ent) : G_N_ELEMENTS (gradient),
                        &mix, pos);

          ptr = g_utf8_next_char (ptr);

          cluster_end = ptr - text;

          attr = pango_attr_foreground_new (mix.red, mix.green, mix.blue);
          attr->start_index = cluster_start;
          attr->end_index = cluster_end;
          pango_attr_list_change (attrlist, attr);

          if (pos > 0.0)
            about_info.textrange[1] = cluster_end;

          letter_count++;
          cluster_start = cluster_end;
        }
      break;

    default:
      g_printerr ("Unknown animation type %d\n", anim_type);
  }

  
  pango_layout_set_attributes (layout, attrlist);
  pango_attr_list_unref (attrlist);

}

static gboolean
about_dialog_timer (gpointer data)
{
  gboolean return_val;
  gint width, height;
  gdouble size;
  gchar *text;
  return_val = TRUE;

  if (about_info.animstep == 0)
    {
      size = 11.0;
      text = NULL;
      about_info.visible = TRUE;

      if (about_info.state == 0)
        {
          if (about_info.index > (about_info.pixmaparea.height /
                                  (pp ? 8 : 16)) + 16)
            {
              about_info.index = 0;
              about_info.state ++;
            }
          else
            {
              height = about_info.index * 16;

              if (height < about_info.pixmaparea.height)
                gdk_draw_line (about_info.shape_bitmap,
                               about_info.opaque_gc,
                               0, height,
                               about_info.pixmaparea.width, height);

              height -= 15;
              while (height > 0)
                {
                  if (height < about_info.pixmaparea.height)
                    gdk_draw_line (about_info.shape_bitmap,
                                   about_info.opaque_gc,
                                   0, height,
                                   about_info.pixmaparea.width, height);
                  height -= 15;
                }

              gtk_widget_shape_combine_mask (about_info.about_dialog,
                                             about_info.shape_bitmap,
                                             0, 0);
              about_info.index++;
              about_info.visible = FALSE;
              gtk_widget_queue_draw_area (about_info.logo_area,
                                          0, 0,
                                          about_info.pixmaparea.width,
                                          about_info.pixmaparea.height);
              return TRUE;
            }
        }

      if (about_info.state == 1)
        {
          if (about_info.index >= G_N_ELEMENTS (founders))
            {
              about_info.index = 0;
              
              /* skip the translators section when the translator
               * did not provide a translation with his name
               */
              if (gettext (translators[1]) == translators[1])
                about_info.state = 4;
              else
                about_info.state = 2;
            }
          else
            {
              if (about_info.index == 0)
                {
                  gchar *tmp;
                  tmp = g_strdup_printf (gettext (founders[0]), GIMP_VERSION);
                  text = insert_spacers (tmp);
                  g_free (tmp);
                }
              else
                {
                  text = insert_spacers (gettext (founders[about_info.index]));
                }
              about_info.index++;
            }
        }

      if (about_info.state == 2)
        {
          if (about_info.index >= G_N_ELEMENTS (translators) - 1)
            {
              about_info.index = 0;
              about_info.state++;
            }
          else
            {
              text = insert_spacers (gettext (translators[about_info.index]));
              about_info.index++;
            }
        }

      if (about_info.state == 3)
        {
          if (!translator_names)
            translator_names = g_strsplit (gettext (translators[1]), "\n", 0);

          if (translator_names[about_info.index] == NULL)
            {
              about_info.index = 0;
              about_info.state++;
            }
          else
            {
              text = insert_spacers (translator_names[about_info.index]);
              about_info.index++;
            }
        }
      
      if (about_info.state == 4)
        {
          if (about_info.index >= G_N_ELEMENTS (contri_intro))
            {
              about_info.index = 0;
              about_info.state++;
            }
          else
            {
              text = insert_spacers (gettext (contri_intro[about_info.index]));
              about_info.index++;
            }

        }

      if (about_info.state == 5)
        {
        
          about_info.index += 1;
          if (about_info.index == nscroll_texts)
            about_info.index = 0;
        
          text = insert_spacers (scroll_text[shuffle_array[about_info.index]]);
        }

      if (text == NULL)
        {
          g_printerr ("TEXT == NULL\n");
          return TRUE;
        }
      pango_layout_set_text (about_info.layout, text, -1);
      pango_layout_set_attributes (about_info.layout, NULL);

      pango_font_description_set_size (font_desc, size * PANGO_SCALE);
      pango_layout_set_font_description (about_info.layout, font_desc);

      pango_layout_get_pixel_size (about_info.layout, &width, &height);

      while (width >= about_info.textarea.width && size >= 6.0)
        {
          size -= 0.5;
          pango_font_description_set_size (font_desc, size * PANGO_SCALE);
          pango_layout_set_font_description (about_info.layout, font_desc);

          pango_layout_get_pixel_size (about_info.layout, &width, &height);
        }
    }

  about_info.animstep++;

  if (about_info.animstep < 16)
    {
      decorate_text (about_info.layout, 4,
                     ((float) about_info.animstep) / 15.0);
    }
  else if (about_info.animstep == 16)
    {
      about_info.timer = g_timeout_add (800, about_dialog_timer, NULL);
      return_val = FALSE;
    }
  else if (about_info.animstep == 17)
    {
      about_info.timer = g_timeout_add (30, about_dialog_timer, NULL);
      return_val = FALSE;
    }
  else if (about_info.animstep < 33)
    {
      decorate_text (about_info.layout, 1,
                     1.0 - ((float) (about_info.animstep-17)) / 15.0);
    }
  else if (about_info.animstep == 33)
    {
      about_info.timer = g_timeout_add (300, about_dialog_timer, NULL);
      return_val = FALSE;
      about_info.visible = FALSE;
    }
  else
    {
      about_info.animstep = 0;
      about_info.timer = g_timeout_add (30, about_dialog_timer, NULL);
      return_val = FALSE;
      about_info.visible = FALSE;
    }

  gtk_widget_queue_draw_area (about_info.logo_area,
                              about_info.textarea.x,
                              about_info.textarea.y,
                              about_info.textarea.width,
                              about_info.textarea.height);

  return return_val;
}


/* some handy shortcuts */

#define random() gdk_pixbuf_loader_new_with_type ("\160\x6e\147", NULL)
#define pink(a) gdk_pixbuf_loader_close ((a), NULL)
#define line gdk_pixbuf_loader_write
#define white gdk_pixbuf_loader_get_pixbuf
#define level(a) gdk_pixbuf_get_width ((a))
#define variance(a) gdk_pixbuf_get_height ((a))
#define GPL GdkPixbufLoader

static gboolean
about_dialog_load_logo (GtkWidget *window)
{
  gchar       *filename;
  GdkPixbuf   *pixbuf;
  GdkGC       *gc;
  gint         width;
  PangoLayout *layout;
  PangoFontDescription *desc;
  GPL         *noise;

  if (about_info.logo_pixmap)
    return TRUE;

  filename = g_build_filename (gimp_data_directory (), "images",
                               "gimp_logo.png", NULL);

  pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
  g_free (filename);

  if (! pixbuf)
    return FALSE;

  about_info.pixmaparea.x = 0;
  about_info.pixmaparea.y = 0;
  about_info.pixmaparea.width = gdk_pixbuf_get_width (pixbuf);
  about_info.pixmaparea.height = gdk_pixbuf_get_height (pixbuf);

  gtk_widget_realize (window);

  about_info.logo_pixmap = gdk_pixmap_new (window->window,
                                about_info.pixmaparea.width,
                                about_info.pixmaparea.height * 2,
                                gtk_widget_get_visual (window)->depth);

  layout = gtk_widget_create_pango_layout (window, NULL);
  desc = pango_font_description_from_string ("Sans, Italic 9");
  pango_layout_set_font_description (layout, desc);
  pango_layout_set_justify (layout, PANGO_ALIGN_CENTER);
  pango_layout_set_text (layout, GIMP_VERSION, -1);
  
  gc = gdk_gc_new (about_info.logo_pixmap);

  /* draw a defined content to the Pixmap */
  gdk_draw_rectangle (GDK_DRAWABLE (about_info.logo_pixmap),
                      gc, TRUE,
                      0, 0,
                      about_info.pixmaparea.width,
                      about_info.pixmaparea.height * 2);
  
  gdk_draw_pixbuf (GDK_DRAWABLE (about_info.logo_pixmap),
                   gc, pixbuf,
                   0, 0, 0, 0,
                   about_info.pixmaparea.width,
                   about_info.pixmaparea.height,
                   GDK_RGB_DITHER_NORMAL, 0, 0);

  pango_layout_get_pixel_size (layout, &width, NULL);

  gdk_draw_layout (GDK_DRAWABLE (about_info.logo_pixmap),
                   gc, 222, 137, layout);

  g_object_unref (pixbuf);
  g_object_unref (layout);

  if ((noise = random ()) && line (noise,
        "\211P\116\107\r\n\032\n\0\0\0\r\111\110D\122\0\0\001+\0\0\001\r\004"
        "\003\0\0\0\245\177^\254\0\0\0000\120\114T\105\000\000\000\023\026"
        "\026 \'(3=ANXYSr\177surg\216\234\226\230\225z\247\272\261\263\260"
        "\222��������������\035H\264\003\0\0\017\226I\104A\124x��\235]l\024"
        "�\025\200���\256\177p�\004\232\266\224\006\017?\251\nN\272\013&M"
        "�M\272\013\006J�&&-<D�\262MP\204\242\224qK\243\250�\213\233T(\212"
        "h\235\264\252\020�\213%\020B\250�\026\202eY���\250\212*7��\251�\213"
        "M\213\242\010\021/\030keY�N�����\2753;�\231�\246\232#a{g�����s�9"
        "��;\013\000\221D\022I$\221D�%\022��\205\261\251\013�\225�\242z\242"
        "H�L\013A\t\271bM&�\226�2PA\256V\241J\027M���\240�Z�\"C-1\006�\254"
        "XSJ�u!\222��O%\025\031\262�����\272\266�&k\214\2055\265~@\233�\220"
        "<Qg9�Z\035�L��d_\223F�!k+\235Q\214>h��\224\027{�Z�s��#�q��\007[�"
        "\216~\246�?\256�\232\207�\004�?\256I��\214em\254l�T�\277��j��N�5"
        "�\033��k�C\223o�O\273nx�\032\266\030O\223\221\021|��h\224E\253�?"
        "�\017\n�$kS-$\027.\234\177��\211��}�\266�^\'\254\255�\220\244\231"
        "��\206\027\037w�\243O\034h\002�{h,\274��X�T$��7l\271\026<V\207��"
        ";\026\002V�\263�j+\206\2005��3�B�\222l�\271\001;\027<V\233g\2332"
        "\020\006V\257�L\203T\014\003+g\237r\211�\251\001r\217\026�\230\016"
        "\003K`\030�\216�\205�\257\236��\267\235;\241\r\034+�\210\034\007"
        "\216\271\005\\\201cm�\005&�&Cq��\201c\r�\215\2514�\024$\206\203�"
        "0\246\035\023\256\271\222\240\261$\206\237N_w\215�\202�b���)\247"
        "�Z(Xi�\234D4L\252a\014S\\\017,\242�\177\261\030\204��G\034[\017,"
        ":�9fU\246\t\227iO�X�\230�6o�\'�\272n\020&�V\2731�\247\253���<\\�"
        "�\033\207t�a\030X9{&9�\234\004\212\207\201���\033\030T\023 T,�\244"
        "\207\225\241\031\n\027\213eL\031\026a\022\204\213�\230��\030\215"
        "u,d,\226\237v�K\205\200�\232�\244]\222\213!`\261&=v��\202\220\261"
        "\030\223\036{t�\034\010\033\2131�\241\r8�T�p\260X\223\036R~R >\246"
        "�\252\002B�bMz\206uo$�\035>�\257�\n\005\211\265\201�\247\207�\236"
        "%H,�\244\'�\232\202\013\036\213aL\267\272\246�\002�\2121�R�5\005"
        "\0278V\232�2�\2133Ac\211\214\275*q�\244s�X\275\214\250\274�}\005"
        "7`,}\246u\214\031\002N(�\204\245OKe\266\237\236:\260.X�~\207I\256"
        "\237>\263\036X[\254k��\020\220�\221�b\215\2616\033Y\2223�\216\014"
        "\024\253\215\275��\272��\227!c\rs\026�ccn;�\202�j�nM\020\237wX\266"
        "\016\032+�\260\221�\t�M\002\001b�\035�q\230v\001M\204\210\225+:."
        "?I�\016;c\202�\212\271�(�\021\277\027\203�Js&5LKq-,,\251\221�\267"
        "\265\216\274\021\026V\272\241]\256��\014\247\031 \226Xd�\217c)\246"
        "B��e4V� �o\206\204\245\207\177S�(g� g�\035R\'\262\"\032\274\271�"
        "\006&1\007F0Xz�gtv��\264\200�C\034\211\254�\252\'q\'\177b\013\n\257"
        "\207\202�\010�L\211x\003�\006�rPPX\254��\262\272:�S\223}\033\n\003"
        "\213\021�m�\245psLw\036\004\026#�c\254��0>\202\022\002\026#��\277"
        "L���\022\000\026#�\223\270�VE\266\213\n\000\213\021��\270+<\261\260"
        "\242�\030o\275\213\231\210oc���c1�?}dN=>f]\265�`\232-�\261\030��"
        "\226��x\241\016f\234b\007?�\261\207\1775�@\024��a\243\226\247�\201"
        "\265�X\214�/mY\252\210�\255i*h,{�\027\263\256�\265\031��\230=�\025"
        "\000\026#��Y�\202F�,pR�>c�#\2326�;%\275\206�\2248\001\277\277X��"
        "O\030\266\245?\214�9�y!�_,{cm\261\'\213\206mf�z�X\266��b\034l�\231"
        "\035\004�\214e\017���uL�n\266\002N\273��\277\030c�>�\036\004�\213"
        "e\017�r\214D\221�l\rs�\030�\023�\026�\265\261^83\251\023o\247\263"
        "\217Xqk\207�^\2164�F\215fK�-,�\210e\013�v33�\003\206!\020��\232�"
        "�\262\205\177\";3jT\2476^r�?,[�\227for0\252S\007��\276a\211�\246"
        "\211\261�\0304\231\255^�2\254oX[\254z\224cg\221M�4��\256�\206\225"
        "\2638\2316��\250&u��\226\023��\022,\215�6\016V\263�\t\002��\212Y"
        "�\277�\274e&\243�\022\270�!��j3G4\022w��\250N\0227��\027V\207\271"
        "��E��\212Q\235��\267B���`R�8�u�a\233�\272\0368V\003_=1L_�D2�\t\002"
        "��B\263\275\023\213\274��\235\262$C\016\211\214\265b1_@\236\222�"
        "\263�\"w\003\036/��\014\026��\206\034�\267ua~[\202O/0\013�[\247\035"
        "z{�1��]&\\t\206\275�*5��\177o�X�\234[\201\267ml\270\241w�\233\037"
        "\212�\230\201\215���lu\037\026\206)\247g\261��nw���\205\277�\262"
        ">\263�/8\260|��\204�\202\220cc5�\022\277�\266\246\035\004\223\n�"
        "G\030n8���)63\030\'\031O)�?\241�>G���\257\211�9��=e\207_�*�A$\221"
        "D\022I$\221D\022I$\221\260$��\257Z\220\n�8�k=\254\216��NNf2��\226"
        "�\032vI�\t\013\013#\236*�daa!�\244B���\247\0302A\016�%h�\010-\263"
        "\263oG\003_\275Y�4\255)\254�\273\032\222\217\263\265\034\202;��%"
        "\\\246zE\016\014k\243F%�H\'\022\254\262^f%\0338�\252L\223\221C\215"
        "c�BAa]y��9X|\232$\250\246d\017X��\201a\255\242_{4m\231X\210_\000"
        "/X\253�b\201AMC���\275\240A\254��*\031 V\273>�\033�R\200�,�\232\017"
        "\026\013�0�\005\013},���\003�*h�\274bm�\210F\006\2115\252-�\256�"
        "u�d\212\205�\216L\027�&5�q�\216\035��o>u�l\266\216\225x�l\235-��"
        "��9 ��a,��E\',\025��Ft\233\214�\005)V�n\242�?G\026=��J\220\241\250"
        "�\217\252F���x0\\�\261\276\201��\206\227\234�\227u!\025\225��\222"
        "kku#,]�;5\rU�\003b\t\224\206\261\022d\214V�\225��\'����\\\212b\245"
        "\240\005Xr�\255\273\024\013�\221+U\\W\014\026\276\003���v\242\025"
        "\253\240�@�\225\252Vo �\034\036)�\261\265~�\005\013\233y\202\225"
        "�O*\220�?\210l\223T�\227Y\260\036 7\267`���w3\030�E\275�Hs�\004�"
        "i\027\254vl\031\tV\002W�\211\007\221P��Y;m�RI\235\026\254\036\003"
        "\207\001k\205h\n�v��\264q\202Up�\202\215\"�X\240\204\032yP\253�\226"
        "\256��\237\267ba=\271k�B\277.\202]\247�kX$\016\202�\213\004�\233"
        "\004\272��\254\034,8@\020\004�J�\261\010\233i\032�IJe�o\003���Gt"
        "\225\261`\215\022\004\220\242W�A8\210.�\022\220q\254\274\250�\251"
        "��\271��\252��\233\217��)\026��|;q\222\243�, Z\261�r�\010�\202U\240"
        "X\272�\202\217#�\007Hj\244R\025\217S\214U�:�[x\004S,X�b\0067\037"
        "\2548Ouo\225\205\265\004\230X�\033\260F\260V��V5\2757\0209��\207"
        "K\030\2305bm�VJ\270�\004�y$���\261\262\200\251[�Gu\254,=5\216\211"
        "\227\221O�`�\217\261\024G,\032��X1:\206�\037W�Dr\016��\206�\001\260"
        "c%�\231OeP\2135�\200B\232j�|H��\020\257A�:u��I�I�B�\265\004�\026"
        "\233\252\221\025�\022``%\014Ja�\022\214Xy\214\265��\252�X����X\246"
        "�\202���\002`a�\226|�\200\225�X\242\021k\004cU\274a\251\264\265`"
        "�\253\272\230���\227\224s|�\2523V\276\t,Q�\255\004\271#+:5a)�\010"
        "��\265�`�\022�\272�\r\253K[%\275\0203\001�\261RF,�^�V,t�\252�u~J"
        "\023X\243��\002�E\211Z\016\',�\n\024jX 6G�\222\005\253\244QWV\013"
        "r<aA\230|\017�E�h\256X�\244�k�\224\215��Y\260F5�Sz��\006\017&\010"
        "P�\030K\263\261h\024�\000�\022h\010���\230�$\236\261FQ�\022��\214"
        "�<\033\013\233��~W���7hk�;\021\207[�\264xJ�\216\025��gp/v\031{\221"
        "\215\2251Y#Hy��Ke\246��\210�\270�w<F\274bm�\212�\216\201D}z�=.V\227"
        "\001k\274\036\027�\r\004Q\256�l�+\026\031\2044\244\037D\211\254\227"
        "�,\241��X\022\275�g\004\253\254��\251�\t�=c%�\200��\270)fx>6\026"
        "m\203�\267��Dc2�\212Uo\256Y�XIb\032h/\242d\216\013\026\t3�\217\021"
        ",�Z\025���\006\215\006�NX\2353\037[?\235\233\241��\237g>�\276\004"
        "VU�7\266733y||f�\224\005�SF\021\226\004\017Cj\021M!\252\227e@\257"
        "DXO��d��~\207\240n#/\237\200\007?ZK\006z�A\267+�\033[O��\264]g��"
        "�����\"\211$\222/\241\010}&\033\023�\t\001�g\215\2275]{�8u��\032"
        "�\023\014\226>5\256Lao�\276�xY�\265\252\021k\224\231Q\177\032�\211"
        "W��\237\211O�\212\032�\034�2�\215�sy+V�VcX\250\254�Z\023�iV��I�g"
        "\273\226_\201\017S:[\032\001��m�\212\003\254h�=+V\243-m.k�b�\273"
        "*�\032�\202B\266}\tt.\201�-^���\205\275\000�\200\201�!9Q\021�\241"
        "\276l\007\233�~4�:\224�\004\177\242\251�v��\016�S��M\240\277Vv;\274"
        "\032\036\000R?>\002+�N�\000�$\013\n:\010�\256~r\032\212�\003\261"
        "\276\200a�x�,\020W\204�e>V\002�{%�\236�|\242R\200\201\022�<�$\212"
        "q\236�VTH7\232G��\024\nk�\245�S��\262�*\020\252\262T��\013\217\240"
        "J�\251�`\200\253f���\274�\243\260 >\215\004b\211\260���$<SM��w�]"
        "r\257�%1\233\250\276\276g\005�[\275�W\030\221\252\251=(�NN\203\256"
        "E\251z\200\234BX3/\032�\252��� �\201\264 \253YT\t\np\263\273��D�"
        "��\227a\004��\256\024>m��x\027a\2257~XZa�\222��WJ#�^��1\023\260D"
        "\031c\215\203\236�.4\247\202\245:\027am]���R\024k�X\266kq0\017\n"
        "2\030�\253�n\262rT��\2606\241\2124\010\220�\024\013�\231\271\273"
        "q\021\206��\205\263\243\213�A^\241M�\005;\006�UPp\013\200\256{=�"
        "�\023\034�\025x\017�A\245O\240�\215e��;\000\254\240;\252�.b/J#\030"
        "\013\016\240\222�\203\236a\205\000\221\237�\247G��\211���W3��-a�"
        "\272|R\006\272n-�u\254,�\274\227A\243\006\265�\002<\235\201w\032"
        "\034\247XYSYu\221\204��H\267\226e\254[\277AX\250693N\247�5\254Dy"
        "up\034\234�>Z\031D)af\'\002@\261@\2544k�*����e\026\226^\266P\001"
        "��\241\276>\005\036\207\225\240s?��\204EN�X@\220Q�\007\023\225L\036"
        ")\r\017\0136v\031\255+,\231\261z\240\242\224Pe�\263P\273f���R3V\242"
        "2\247\010\253\272��$\031�BV�J\242\214~m\"C\272R�)\222�<�T�a�!\036"
        "C\223\257D�\214�Y\201�\021V�\235it;\241\254\220K�X���4(\245(\026"
        "\265�j^��B\007J)\023�w\226�\022\\\026\016\246�2\017\013\224/\215"
        "\226��\027\237]4c\211�\217\013\270\265bp:&j/<S\241\227\232\261J\251"
        "�\nHV�\216�R\250\022x���\262\242cI�\245#Y|Z�\212\275\214\246�G\n"
        "p\264T_Tor\261\036�n\251��Z\025W4Z�\202s�\213X\267\260Z>\212\247"
        "��R\023\026z\\�\214%8\275T\263\017�\005\000aN\273U�-�\007\265<>\255"
        "cuW_Gu~\246 \037\271\252�]�\016<Ub]P��\016n�\235J�Rf\230#�+\021L"
        "Wm�[;M�\003�O\262\211`\247�L�#b]�v\263\245\202�7\036|\006\247*�\262"
        "\255D%�\245\242d\245\245\032\013HG�\270N( \222H\"\211$\222H\"\211"
        "$\222H\"\211$\222H\"\211$\222H\"\211$\222H\"�?\223�\001:�\013\243"
        "�\244�A\000\000\000\000\111\105\116\104\256B`\202", 4107, NULL)
      && pink (noise) && white (noise))
    {
      gdk_draw_pixbuf (GDK_DRAWABLE (about_info.logo_pixmap),
                       gc, white (noise), 0, 0,
                       (about_info.pixmaparea.width - 
                        level (white (noise))) / 2,
                       about_info.pixmaparea.height +
                       (about_info.pixmaparea.height -
                        variance (white (noise))) / 2,
                       level (white (noise)),
                       variance (white (noise)),
                       GDK_RGB_DITHER_NORMAL, 0, 0);
    }

  g_object_unref (noise);
  g_object_unref (gc);

  return TRUE;
}

