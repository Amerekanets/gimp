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

#include <string.h>

#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"

#include "display-types.h"

#ifdef __GNUC__
#warning FIXME #include "gui/gui-types.h"
#endif
#include "gui/gui-types.h"

#include "config/gimpdisplayconfig.h"

#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimpimage.h"

#include "file/file-utils.h"

#include "gui/info-window.h"

#include "gimpdisplay.h"
#include "gimpdisplayshell.h"
#include "gimpdisplayshell-title.h"
#include "gimpstatusbar.h"

#include "gimp-intl.h"


#define MAX_TITLE_BUF 256


static gboolean gimp_display_shell_update_title_idle (gpointer          data);
static void     gimp_display_shell_format_title      (GimpDisplayShell *gdisp,
                                                      gchar            *title,
                                                      gint              title_len,
                                                      const gchar      *format);


/*  public functions  */

void
gimp_display_shell_update_title (GimpDisplayShell *shell)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  if (shell->title_idle_id)
    g_source_remove (shell->title_idle_id);

  shell->title_idle_id = g_idle_add (gimp_display_shell_update_title_idle,
                                     shell);
}


/*  private functions  */

static gboolean
gimp_display_shell_update_title_idle (gpointer data)
{
  GimpDisplayShell  *shell;
  GimpDisplayConfig *config;
  gchar              title[MAX_TITLE_BUF];

  shell  = GIMP_DISPLAY_SHELL (data);
  config = GIMP_DISPLAY_CONFIG (shell->gdisp->gimage->gimp->config);

  shell->title_idle_id = 0;

  /* format the title */
  gimp_display_shell_format_title (shell, title, sizeof (title),
                                   config->image_title_format);
  gdk_window_set_title (GTK_WIDGET (shell)->window, title);

  /* format the statusbar */
  if (strcmp (config->image_title_format, config->image_status_format))
    {
      gimp_display_shell_format_title (shell, title, sizeof (title),
                                       config->image_status_format);
    }

  gimp_statusbar_pop (GIMP_STATUSBAR (shell->statusbar), "title");
  gimp_statusbar_push (GIMP_STATUSBAR (shell->statusbar), "title", title);

#ifdef __GNUC__
#warning FIXME: dont call info_window_update() here.
#endif
  info_window_update (shell->gdisp);

  return FALSE;
}


static gint print (gchar       *buf,
                   gint         len,
                   gint         start,
                   const gchar *fmt,
                   ...) G_GNUC_PRINTF (4, 5);

static gint
print (gchar       *buf,
       gint         len,
       gint         start,
       const gchar *fmt,
       ...)
{
  va_list args;
  gint    printed;

  va_start (args, fmt);

  printed = g_vsnprintf (buf + start, len - start, fmt, args);
  if (printed < 0)
    printed = len - start;

  va_end (args);

  return printed;
}

static void
gimp_display_shell_format_title (GimpDisplayShell *shell,
                                 gchar            *title,
                                 gint              title_len,
                                 const gchar      *format)
{
  GimpImage *gimage;
  gchar     *image_type_str = NULL;
  gboolean   empty;
  gint       i;
  gchar      unit_format[8];

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  gimage = shell->gdisp->gimage;

  empty = gimp_image_is_empty (gimage);

  switch (gimp_image_base_type (gimage))
    {
    case GIMP_RGB:
      image_type_str = empty ? _("RGB-empty") : _("RGB");
      break;
    case GIMP_GRAY:
      image_type_str = empty ? _("grayscale-empty") : _("grayscale");
      break;
    case GIMP_INDEXED:
      image_type_str = empty ? _("indexed-empty") : _("indexed");
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  i = 0;

  while (i < title_len && *format)
    {
      switch (*format)
	{
	case '%':
	  format++;
	  switch (*format)
	    {
	    case 0:
	      g_warning ("image-title-format string ended within %%-sequence");
	      break;

	    case '%':
	      title[i++] = '%';
	      break;

	    case 'f': /* pruned filename */
	      {
		gchar *basename;

		basename = file_utils_uri_to_utf8_basename (gimp_image_get_uri (gimage));

		i += print (title, title_len, i, "%s", basename);

		g_free (basename);
	      }
	      break;

	    case 'F': /* full filename */
	      {
		gchar *filename;

		filename = file_utils_uri_to_utf8_filename (gimp_image_get_uri (gimage));

                i += print (title, title_len, i, "%s", filename);

                g_free (filename);
              }
	      break;

	    case 'p': /* PDB id */
	      i += print (title, title_len, i, "%d", gimp_image_get_ID (gimage));
	      break;

	    case 'i': /* instance */
	      i += print (title, title_len, i, "%d", shell->gdisp->instance);
	      break;

	    case 't': /* type */
	      i += print (title, title_len, i, "%s", image_type_str);
	      break;

	    case 's': /* user source zoom factor */
	      i += print (title, title_len, i, "%d", SCALESRC (shell));
	      break;

	    case 'd': /* user destination zoom factor */
	      i += print (title, title_len, i, "%d", SCALEDEST (shell));
	      break;

	    case 'z': /* user zoom factor (percentage) */
	      i += print (title, title_len, i, "%d",
                          100 * SCALEDEST (shell) / SCALESRC (shell));
	      break;

	    case 'D': /* dirty flag */
	      if (format[1] == 0)
		{
		  g_warning ("image-title-format string ended within "
                             "%%D-sequence");
		  break;
		}
	      if (gimage->dirty)
		title[i++] = format[1];
	      format++;
	      break;

	    case 'C': /* clean flag */
	      if (format[1] == 0)
		{
		  g_warning ("image-title-format string ended within "
                             "%%C-sequence");
		  break;
		}
	      if (! gimage->dirty)
		title[i++] = format[1];
	      format++;
	      break;

            case 'm': /* memory used by image */
              {
                gulong  memsize;
                gchar  *memsize_str;

                memsize = gimp_object_get_memsize (GIMP_OBJECT (gimage), NULL);

                memsize_str = gimp_memsize_to_string (memsize);

                i += print (title, title_len, i, "%s", memsize_str);

                g_free (memsize_str);
              }
              break;

            case 'l': /* number of layers */
              i += print (title, title_len, i, "%d",
                          gimp_container_num_children (gimage->layers));
              break;

            case 'L': /* active drawable name */
              {
                GimpDrawable *drawable;

                drawable = gimp_image_active_drawable (gimage);

                if (drawable)
                  i += print (title, title_len, i, "%s",
                              gimp_object_get_name (GIMP_OBJECT (drawable)));
                else
                  i += print (title, title_len, i, "%s", _("(none)"));
              }
              break;

	    case 'w': /* width in pixels */
	      i += print (title, title_len, i, "%d", gimage->width);
	      break;

	    case 'W': /* width in real-world units */
              g_snprintf (unit_format, sizeof (unit_format), "%%.%df",
                          gimp_unit_get_digits (gimage->unit) + 1);
              i += print (title, title_len, i, unit_format,
                          (gimage->width *
                           gimp_unit_get_factor (gimage->unit) /
                           gimage->xresolution));
              break;

	    case 'h': /* height in pixels */
	      i += print (title, title_len, i, "%d", gimage->height);
	      break;

	    case 'H': /* height in real-world units */
              g_snprintf (unit_format, sizeof (unit_format), "%%.%df",
                          gimp_unit_get_digits (gimage->unit) + 1);
              i += print (title, title_len, i, unit_format,
                          (gimage->height *
                           gimp_unit_get_factor (gimage->unit) /
                           gimage->yresolution));
              break;

	    case 'u': /* unit symbol */
	      i += print (title, title_len, i, "%s",
			  gimp_unit_get_symbol (gimage->unit));
	      break;

	    case 'U': /* unit abbreviation */
	      i += print (title, title_len, i, "%s",
			  gimp_unit_get_abbreviation (gimage->unit));
	      break;

	      /* Other cool things to be added:
	       * %r = xresolution
               * %R = yresolution
               * %� = image's fractal dimension
               * %� = the answer to everything
	       */

	    default:
	      g_warning ("image-title-format contains unknown "
                         "format sequence '%%%c'", *format);
	      break;
	    }
	  break;

	default:
	  title[i++] = *format;
	  break;
	}

      format++;
    }

  title[MIN (i, title_len - 1)] = '\0';
}
