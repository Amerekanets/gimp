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

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>

#include <glib-object.h>

#ifdef G_OS_WIN32 /* gets defined by glib.h */
#include <io.h>
#endif

#ifndef _O_BINARY
#define _O_BINARY 0
#endif

#include <stdio.h>

#include "core-types.h"

#include "base/brush-scale.h"
#include "base/temp-buf.h"

#include "config/gimpbaseconfig.h"

#include "gimpbrush.h"
#include "gimpbrush-header.h"
#include "gimpbrushgenerated.h"
#include "gimpmarshal.h"

#include "libgimp/gimpintl.h"


enum
{
  SPACING_CHANGED,
  LAST_SIGNAL
};


static void        gimp_brush_class_init            (GimpBrushClass *klass);
static void        gimp_brush_init                  (GimpBrush      *brush);

static void        gimp_brush_finalize              (GObject        *object);

static gsize       gimp_brush_get_memsize           (GimpObject     *object);

static gboolean    gimp_brush_get_popup_size        (GimpViewable   *viewable,
                                                     gint            width,
                                                     gint            height,
                                                     gboolean        dot_for_dot,
                                                     gint           *popup_width,
                                                     gint           *popup_height);
static TempBuf   * gimp_brush_get_new_preview       (GimpViewable   *viewable,
                                                     gint            width,
                                                     gint            height);
static gchar     * gimp_brush_get_extension         (GimpData       *data);

static GimpBrush * gimp_brush_real_select_brush     (GimpBrush      *brush,
                                                     GimpCoords     *last_coords,
                                                     GimpCoords     *cur_coords);
static gboolean    gimp_brush_real_want_null_motion (GimpBrush      *brush,
                                                     GimpCoords     *last_coords,
                                                     GimpCoords     *cur_coords);


static guint brush_signals[LAST_SIGNAL] = { 0 };

static GimpDataClass *parent_class = NULL;


GType
gimp_brush_get_type (void)
{
  static GType brush_type = 0;

  if (! brush_type)
    {
      static const GTypeInfo brush_info =
      {
        sizeof (GimpBrushClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_brush_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data     */
	sizeof (GimpBrush),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_brush_init,
      };

      brush_type = g_type_register_static (GIMP_TYPE_DATA,
					   "GimpBrush", 
					   &brush_info, 0);
  }

  return brush_type;
}

static void
gimp_brush_class_init (GimpBrushClass *klass)
{
  GObjectClass      *object_class;
  GimpObjectClass   *gimp_object_class;
  GimpViewableClass *viewable_class;
  GimpDataClass     *data_class;

  object_class      = G_OBJECT_CLASS (klass);
  gimp_object_class = GIMP_OBJECT_CLASS (klass);
  viewable_class    = GIMP_VIEWABLE_CLASS (klass);
  data_class        = GIMP_DATA_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  brush_signals[SPACING_CHANGED] = 
    g_signal_new ("spacing_changed",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GimpBrushClass, spacing_changed),
		  NULL, NULL,
		  gimp_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  object_class->finalize          = gimp_brush_finalize;

  gimp_object_class->get_memsize  = gimp_brush_get_memsize;

  viewable_class->get_popup_size  = gimp_brush_get_popup_size;
  viewable_class->get_new_preview = gimp_brush_get_new_preview;

  data_class->get_extension       = gimp_brush_get_extension;

  klass->select_brush             = gimp_brush_real_select_brush;
  klass->want_null_motion         = gimp_brush_real_want_null_motion;
  klass->spacing_changed          = NULL;
}

static void
gimp_brush_init (GimpBrush *brush)
{
  brush->mask     = NULL;
  brush->pixmap   = NULL;

  brush->spacing  = 20;
  brush->x_axis.x = 15.0;
  brush->x_axis.y =  0.0;
  brush->y_axis.x =  0.0;
  brush->y_axis.y = 15.0;
}

static void
gimp_brush_finalize (GObject *object)
{
  GimpBrush *brush;

  brush = GIMP_BRUSH (object);

  if (brush->mask)
    {
      temp_buf_free (brush->mask);
      brush->mask = NULL;
    }

  if (brush->pixmap)
    {
      temp_buf_free (brush->pixmap);
      brush->pixmap = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gsize
gimp_brush_get_memsize (GimpObject *object)
{
  GimpBrush *brush;
  gsize      memsize = 0;

  brush = GIMP_BRUSH (object);

  if (brush->mask)
    memsize += temp_buf_get_memsize (brush->mask);

  if (brush->pixmap)
    memsize += temp_buf_get_memsize (brush->pixmap);

  return memsize + GIMP_OBJECT_CLASS (parent_class)->get_memsize (object);
}

static gboolean
gimp_brush_get_popup_size (GimpViewable *viewable,
                           gint          width,
                           gint          height,
                           gboolean      dot_for_dot,
                           gint         *popup_width,
                           gint         *popup_height)
{
  GimpBrush *brush;

  brush = GIMP_BRUSH (viewable);

  if (brush->mask->width > width || brush->mask->height > height)
    {
      *popup_width  = brush->mask->width;
      *popup_height = brush->mask->height;

      return TRUE;
    }

  return FALSE;
}

static TempBuf *
gimp_brush_get_new_preview (GimpViewable *viewable,
			    gint          width,
			    gint          height)
{
  GimpBrush *brush;
  gint       brush_width;
  gint       brush_height;
  TempBuf   *mask_buf   = NULL;
  TempBuf   *pixmap_buf = NULL;
  TempBuf   *return_buf = NULL;
  guchar     transp[4] = { 0, 0, 0, 0 };
  guchar    *mask;
  guchar    *buf;
  gint       x, y;
  gboolean   scale = FALSE;

  brush = GIMP_BRUSH (viewable);

  mask_buf   = gimp_brush_get_mask (brush);
  pixmap_buf = gimp_brush_get_pixmap (brush);

  brush_width  = mask_buf->width;
  brush_height = mask_buf->height;

  if (brush_width > width || brush_height > height)
    {
      gdouble ratio_x = (gdouble) brush_width  / width;
      gdouble ratio_y = (gdouble) brush_height / height;

      brush_width  = (gdouble) brush_width  / MAX (ratio_x, ratio_y) + 0.5;
      brush_height = (gdouble) brush_height / MAX (ratio_x, ratio_y) + 0.5;

      mask_buf = brush_scale_mask (mask_buf, brush_width, brush_height);

      if (pixmap_buf)
        {
          /* TODO: the scale function should scale the pixmap and the
	   *  mask in one run
	   */
          pixmap_buf =
	    brush_scale_pixmap (pixmap_buf, brush_width, brush_height);
        }

      scale = TRUE;
    }

  return_buf = temp_buf_new (brush_width, brush_height, 4, 0, 0, transp);

  mask = temp_buf_data (mask_buf);
  buf  = temp_buf_data (return_buf);

  if (pixmap_buf)
    {
      guchar *pixmap = temp_buf_data (pixmap_buf);

      for (y = 0; y < brush_height; y++)
        {
          for (x = 0; x < brush_width ; x++)
            {
              *buf++ = *pixmap++;
              *buf++ = *pixmap++;
              *buf++ = *pixmap++;
              *buf++ = *mask++;
            }
        }
    }
  else
    {
      for (y = 0; y < brush_height; y++)
        {
          for (x = 0; x < brush_width ; x++)
            {
              *buf++ = 0;
              *buf++ = 0;
              *buf++ = 0;
              *buf++ = *mask++;
            }
        }
    }

  if (scale)
    {
      temp_buf_free (mask_buf);

      if (pixmap_buf)
        temp_buf_free (pixmap_buf);
    }

  return return_buf;
}

static gchar *
gimp_brush_get_extension (GimpData *data)
{
  return GIMP_BRUSH_FILE_EXTENSION;
}

GimpData *
gimp_brush_new (const gchar *name,
                gboolean     stingy_memory_use)
{
  GimpBrush *brush;

  g_return_val_if_fail (name != NULL, NULL);

  brush = GIMP_BRUSH (gimp_brush_generated_new (5.0, 0.5, 0.0, 1.0,
                                                stingy_memory_use));

  gimp_object_set_name (GIMP_OBJECT (brush), name);

  return GIMP_DATA (brush);
}

GimpData *
gimp_brush_get_standard (void)
{
  static GimpBrush *standard_brush = NULL;

  if (! standard_brush)
    {
      standard_brush = GIMP_BRUSH (gimp_brush_generated_new (5.0, 0.5,
                                                             0.0, 1.0,
                                                             FALSE));

      gimp_object_set_name (GIMP_OBJECT (standard_brush), "Standard");

      /*  set ref_count to 2 --> never swap the standard brush  */
      g_object_ref (standard_brush);
    }

  return GIMP_DATA (standard_brush);
}

GimpData *
gimp_brush_load (const gchar  *filename,
                 gboolean      stingy_memory_use,
                 GError      **error)
{
  GimpBrush *brush;
  gint       fd;

  g_return_val_if_fail (filename != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  fd = open (filename, O_RDONLY | _O_BINARY);
  if (fd == -1)
    {
      g_set_error (error, GIMP_DATA_ERROR, GIMP_DATA_ERROR_OPEN,
                   _("Could not open '%s' for reading: %s"),
                   filename, g_strerror (errno));
      return NULL;
    }

  brush = gimp_brush_load_brush (fd, filename, error);

  close (fd);

  if (! brush)
    return NULL;

  gimp_data_set_filename (GIMP_DATA (brush), filename);

  /*  Swap the brush to disk (if we're being stingy with memory) */
  if (stingy_memory_use)
    {
      temp_buf_swap (brush->mask);

      if (brush->pixmap)
	temp_buf_swap (brush->pixmap);
    }

  GIMP_DATA (brush)->dirty = FALSE;

  return GIMP_DATA (brush);
}

GimpBrush *
gimp_brush_select_brush (GimpBrush  *brush,
                         GimpCoords *last_coords,
                         GimpCoords *cur_coords)
{
  g_return_val_if_fail (GIMP_IS_BRUSH (brush), NULL);
  g_return_val_if_fail (last_coords != NULL, NULL);
  g_return_val_if_fail (cur_coords != NULL, NULL);

  return GIMP_BRUSH_GET_CLASS (brush)->select_brush (brush,
                                                     last_coords,
                                                     cur_coords);
}

gboolean
gimp_brush_want_null_motion (GimpBrush  *brush,
                             GimpCoords *last_coords,
                             GimpCoords *cur_coords)
{
  g_return_val_if_fail (GIMP_IS_BRUSH (brush), FALSE);
  g_return_val_if_fail (last_coords != NULL, FALSE);
  g_return_val_if_fail (cur_coords != NULL, FALSE);

  return GIMP_BRUSH_GET_CLASS (brush)->want_null_motion (brush,
                                                         last_coords,
                                                         cur_coords);
}

static GimpBrush *
gimp_brush_real_select_brush (GimpBrush  *brush,
                              GimpCoords *last_coords,
                              GimpCoords *cur_coords)
{
  return brush;
}

static gboolean
gimp_brush_real_want_null_motion (GimpBrush  *brush,
                                  GimpCoords *last_coords,
                                  GimpCoords *cur_coords)
{
  return TRUE;
}

TempBuf *
gimp_brush_get_mask (const GimpBrush *brush)
{
  g_return_val_if_fail (brush != NULL, NULL);
  g_return_val_if_fail (GIMP_IS_BRUSH (brush), NULL);

  return brush->mask;
}

TempBuf *
gimp_brush_get_pixmap (const GimpBrush *brush)
{
  g_return_val_if_fail (brush != NULL, NULL);
  g_return_val_if_fail (GIMP_IS_BRUSH (brush), NULL);

  return brush->pixmap;
}

gint
gimp_brush_get_spacing (const GimpBrush *brush)
{
  g_return_val_if_fail (GIMP_IS_BRUSH (brush), 0);

  return brush->spacing;
}

void
gimp_brush_set_spacing (GimpBrush *brush,
			gint       spacing)
{
  g_return_if_fail (GIMP_IS_BRUSH (brush));

  if (brush->spacing != spacing)
    {
      brush->spacing = spacing;

      gimp_brush_spacing_changed (brush);
    }
}

void
gimp_brush_spacing_changed (GimpBrush *brush)
{
  g_return_if_fail (GIMP_IS_BRUSH (brush));

  g_signal_emit (brush, brush_signals[SPACING_CHANGED], 0);
}

GimpBrush *
gimp_brush_load_brush (gint          fd,
		       const gchar  *filename,
                       GError      **error)
{
  GimpBrush   *brush;
  gint         bn_size;
  BrushHeader  header;
  gchar       *name = NULL;
  gint         i;

  g_return_val_if_fail (filename != NULL, NULL);
  g_return_val_if_fail (fd != -1, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /*  Read in the header size  */
  if (read (fd, &header, sizeof (header)) != sizeof (header))
    {
      g_set_error (error, GIMP_DATA_ERROR, GIMP_DATA_ERROR_READ,
                   _("Could not read %d bytes from '%s': %s"),
                   (gint) sizeof (header), filename, g_strerror (errno));
      return NULL;
    }

  /*  rearrange the bytes in each unsigned int  */
  header.header_size  = g_ntohl (header.header_size);
  header.version      = g_ntohl (header.version);
  header.width        = g_ntohl (header.width);
  header.height       = g_ntohl (header.height);
  header.bytes        = g_ntohl (header.bytes);
  header.magic_number = g_ntohl (header.magic_number);
  header.spacing      = g_ntohl (header.spacing);

  /*  Check for correct file format */
  /*  It looks as if version 1 did not have the same magic number.  (neo)  */
  if (header.version != 1 &&
      (header.magic_number != GBRUSH_MAGIC || header.version != 2))
    {
      g_set_error (error, GIMP_DATA_ERROR, GIMP_DATA_ERROR_READ,
                   _("Fatal parsing error (unknown version %d):\n"
                     "Brush file '%s'"),
                   header.version, filename);
      return NULL;
    }

  if (header.version == 1)
    {
      /*  If this is a version 1 brush, set the fp back 8 bytes  */
      lseek (fd, -8, SEEK_CUR);
      header.header_size += 8;
      /*  spacing is not defined in version 1  */
      header.spacing = 25;
    }
  
   /*  Read in the brush name  */
  if ((bn_size = (header.header_size - sizeof (header))))
    {
      name = g_new (gchar, bn_size);
      if ((read (fd, name, bn_size)) < bn_size)
	{
	  g_set_error (error, GIMP_DATA_ERROR, GIMP_DATA_ERROR_READ,
                       _("Fatal parsing error:\n"
                         "Brush file '%s' appears truncated."),
                       filename);
	  g_free (name);
	  return NULL;
	}

      if (!g_utf8_validate (name, -1, NULL))
        {
          g_message (_("Invalid UTF-8 string in brush file '%s'."), 
                     filename);
          g_free (name);
          name = NULL;
        }
    }
  
  if (!name)
    name = g_strdup (_("Unnamed"));

  switch (header.bytes)
    {
    case 1:
      brush = GIMP_BRUSH (g_object_new (GIMP_TYPE_BRUSH, NULL));
      brush->mask = temp_buf_new (header.width, header.height, 1,
				  0, 0, NULL);
      if (read (fd,
		temp_buf_data (brush->mask), header.width * header.height) <
	  header.width * header.height)
	{
	  g_set_error (error, GIMP_DATA_ERROR, GIMP_DATA_ERROR_READ,
                       _("Fatal parsing error:\n"
                         "Brush file '%s' appears truncated."),
                       filename);
	  g_free (name);
	  g_object_unref (brush);
	  return NULL;
	}
      break;

    case 4:
      brush = GIMP_BRUSH (g_object_new (GIMP_TYPE_BRUSH, NULL));
      brush->mask   = temp_buf_new (header.width, header.height, 1, 0, 0, NULL);
      brush->pixmap = temp_buf_new (header.width, header.height, 3, 0, 0, NULL);

      for (i = 0; i < header.width * header.height; i++)
	{
	  if (read (fd, temp_buf_data (brush->pixmap)
		    + i * 3, 3) != 3 ||
	      read (fd, temp_buf_data (brush->mask) + i, 1) != 1)
	    {
	      g_set_error (error, GIMP_DATA_ERROR, GIMP_DATA_ERROR_READ,
                           _("Fatal parsing error:\n"
                             "Brush file '%s' appears truncated."),
                           filename);
	      g_free (name);
	      g_object_unref (brush);
	      return NULL;
	    }
	}
      break;

    default:
      g_set_error (error, GIMP_DATA_ERROR, GIMP_DATA_ERROR_READ,
                   _("Unsupported brush depth %d\n"
                     "in file '%s'.\n"
                     "GIMP brushes must be GRAY or RGBA."),
                   header.bytes, filename);
      g_free (name);
      return NULL;
    }

  gimp_object_set_name (GIMP_OBJECT (brush), name);
  g_free (name);

  brush->spacing  = header.spacing;
  brush->x_axis.x = header.width  / 2.0;
  brush->x_axis.y = 0.0;
  brush->y_axis.x = 0.0;
  brush->y_axis.y = header.height / 2.0;

  return brush;
}




