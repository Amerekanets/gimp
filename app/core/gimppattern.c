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
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>

#include <glib-object.h>

#ifdef G_OS_WIN32
#include <io.h>
#endif

#ifndef _O_BINARY
#define _O_BINARY 0
#endif

#include "core-types.h"

#include "config/gimpbaseconfig.h"

#include "base/temp-buf.h"

#include "gimpimage.h"
#include "gimppattern.h"
#include "gimppattern-header.h"

#include "libgimp/gimpintl.h"


static void       gimp_pattern_class_init      (GimpPatternClass *klass);
static void       gimp_pattern_init            (GimpPattern      *pattern);

static void       gimp_pattern_finalize        (GObject          *object);

static gsize      gimp_pattern_get_memsize     (GimpObject       *object);

static TempBuf  * gimp_pattern_get_new_preview (GimpViewable     *viewable,
                                                gint              width,
                                                gint              height);
static gchar    * gimp_pattern_get_extension   (GimpData         *data);
static GimpData * gimp_pattern_duplicate       (GimpData         *data,
                                                gboolean          stingy_memory_use);


static GimpDataClass *parent_class = NULL;


GType
gimp_pattern_get_type (void)
{
  static GType pattern_type = 0;

  if (! pattern_type)
    {
      static const GTypeInfo pattern_info =
      {
        sizeof (GimpPatternClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_pattern_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data     */
	sizeof (GimpPattern),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_pattern_init,
      };

      pattern_type = g_type_register_static (GIMP_TYPE_DATA,
					     "GimpPattern", 
					     &pattern_info, 0);
  }

  return pattern_type;
}

static void
gimp_pattern_class_init (GimpPatternClass *klass)
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

  object_class->finalize          = gimp_pattern_finalize;

  gimp_object_class->get_memsize  = gimp_pattern_get_memsize;

  viewable_class->get_new_preview = gimp_pattern_get_new_preview;

  data_class->get_extension       = gimp_pattern_get_extension;
  data_class->duplicate           = gimp_pattern_duplicate;
}

static void
gimp_pattern_init (GimpPattern *pattern)
{
  pattern->mask = NULL;
}

static void
gimp_pattern_finalize (GObject *object)
{
  GimpPattern *pattern;

  pattern = GIMP_PATTERN (object);

  if (pattern->mask)
    {
      temp_buf_free (pattern->mask);
      pattern->mask = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gsize
gimp_pattern_get_memsize (GimpObject *object)
{
  GimpPattern *pattern;
  gsize        memsize = 0;

  pattern = GIMP_PATTERN (object);

  if (pattern->mask)
    memsize += temp_buf_get_memsize (pattern->mask);

  return memsize + GIMP_OBJECT_CLASS (parent_class)->get_memsize (object);
}

static TempBuf *
gimp_pattern_get_new_preview (GimpViewable *viewable,
			      gint          width,
			      gint          height)
{
  GimpPattern *pattern;
  TempBuf     *temp_buf;
  guchar       white[MAX_CHANNELS] = { 255, 255, 255, 255 };
  gint         copy_width;
  gint         copy_height;
  gint         x, y;

  pattern = GIMP_PATTERN (viewable);

  copy_width  = MIN (width,  pattern->mask->width);
  copy_height = MIN (height, pattern->mask->height);

  x = (copy_width  == width)  ? 0 : (width -  copy_width)  / 2;
  y = (copy_height == height) ? 0 : (height - copy_height) / 2;

  temp_buf = temp_buf_new (width, height,
			   pattern->mask->bytes,
			   0, 0,
			   white);

  temp_buf_copy_area (pattern->mask, temp_buf,
		      0, 0,
		      copy_width, copy_height,
		      x, y);

  return temp_buf;
}

static gchar *
gimp_pattern_get_extension (GimpData *data)
{
  return GIMP_PATTERN_FILE_EXTENSION;
}

static GimpData *
gimp_pattern_duplicate (GimpData *data,
                        gboolean  stingy_memory_use)
{
  GimpPattern *pattern;

  pattern = GIMP_PATTERN (g_object_new (GIMP_TYPE_PATTERN, NULL));

  pattern->mask = temp_buf_copy (GIMP_PATTERN (data)->mask, NULL);

  /*  Swap the pattern to disk (if we're being stingy with memory) */
  if (stingy_memory_use)
    temp_buf_swap (pattern->mask);

  return GIMP_DATA (pattern);
}

GimpData *
gimp_pattern_new (const gchar *name,
                  gboolean     stingy_memory_use)
{
  GimpPattern *pattern;
  guchar      *data;
  gint         row, col;

  g_return_val_if_fail (name != NULL, NULL);

  pattern = GIMP_PATTERN (g_object_new (GIMP_TYPE_PATTERN, NULL));

  gimp_object_set_name (GIMP_OBJECT (pattern), name);

  pattern->mask = temp_buf_new (32, 32, 3, 0, 0, NULL);

  data = temp_buf_data (pattern->mask);

  for (row = 0; row < pattern->mask->height; row++)
    for (col = 0; col < pattern->mask->width; col++)
      {
	memset (data, (col % 2) && (row % 2) ? 255 : 0, 3);
	data += 3;
      }

  /*  Swap the pattern to disk (if we're being stingy with memory) */
  if (stingy_memory_use)
    temp_buf_swap (pattern->mask);

  return GIMP_DATA (pattern);
}

GimpData *
gimp_pattern_get_standard (void)
{
  static GimpPattern *standard_pattern = NULL;

  if (! standard_pattern)
    {
      guchar *data;
      gint    row, col;

      standard_pattern = GIMP_PATTERN (g_object_new (GIMP_TYPE_PATTERN, NULL));

      gimp_object_set_name (GIMP_OBJECT (standard_pattern), "Standard");

      standard_pattern->mask = temp_buf_new (32, 32, 3, 0, 0, NULL);

      data = temp_buf_data (standard_pattern->mask);

      for (row = 0; row < standard_pattern->mask->height; row++)
	for (col = 0; col < standard_pattern->mask->width; col++)
	  {
	    memset (data, (col % 2) && (row % 2) ? 255 : 0, 3);
	    data += 3;
	  }

      /*  set ref_count to 2 --> never swap the standard pattern  */
      g_object_ref (G_OBJECT (standard_pattern));
    }

  return GIMP_DATA (standard_pattern);
}

GimpData *
gimp_pattern_load (const gchar  *filename,
                   gboolean      stingy_memory_use,
                   GError      **error)
{
  GimpPattern   *pattern = NULL;
  gint           fd;
  PatternHeader  header;
  gint           bn_size;
  gchar         *name    = NULL;

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

  /*  Read in the header size  */
  if (read (fd, &header, sizeof (header)) != sizeof (header))
    {
      g_set_error (error, GIMP_DATA_ERROR, GIMP_DATA_ERROR_READ,
                   _("Could not read %d bytes from '%s': %s"),
                   (gint) sizeof (header), filename, g_strerror (errno));
      goto error;
    }

  /*  rearrange the bytes in each unsigned int  */
  header.header_size  = g_ntohl (header.header_size);
  header.version      = g_ntohl (header.version);
  header.width        = g_ntohl (header.width);
  header.height       = g_ntohl (header.height);
  header.bytes        = g_ntohl (header.bytes);
  header.magic_number = g_ntohl (header.magic_number);

  /*  Check for correct file format */
  if (header.magic_number != GPATTERN_MAGIC || header.version != 1 || 
      header.header_size <= sizeof (header)) 
    {
      g_set_error (error, GIMP_DATA_ERROR, GIMP_DATA_ERROR_READ,
                   _("Unknown pattern format version %d in '%s'."),
                   header.version, filename);
      goto error;
    }
  
  /*  Check for supported bit depths  */
  if (header.bytes != 1 && header.bytes != 3)
    {
      g_set_error (error, GIMP_DATA_ERROR, GIMP_DATA_ERROR_READ,
                   _("Unsupported pattern depth %d\n"
                     "in file '%s'.\n"
                     "GIMP Patterns must be GRAY or RGB.\n"),
                   header.bytes, filename);
      goto error;
    }

  /*  Read in the pattern name  */
  if ((bn_size = (header.header_size - sizeof (header))))
    {
      name = g_new (gchar, bn_size);

      if ((read (fd, name, bn_size)) < bn_size)
        {
          g_set_error (error, GIMP_DATA_ERROR, GIMP_DATA_ERROR_READ,
                       _("Error in GIMP pattern file '%s'."),
                       filename);
	  goto error;
        }
        
      if (!g_utf8_validate (name, -1, NULL))
        {
          g_message (_("Invalid UTF-8 string in pattern file '%s'."), 
                     filename);
          g_free (name);
          name = NULL;
        }
    }

  if (!name)
    name = g_strdup (_("Unnamed"));

  pattern = GIMP_PATTERN (g_object_new (GIMP_TYPE_PATTERN, NULL));

  pattern->mask = temp_buf_new (header.width, header.height, header.bytes,
                                0, 0, NULL);
  if (read (fd, temp_buf_data (pattern->mask), 
            header.width * header.height * header.bytes) <
      header.width * header.height * header.bytes)
    {
      g_set_error (error, GIMP_DATA_ERROR, GIMP_DATA_ERROR_READ,
                   _("Fatal parsing error: "
                     "Pattern file '%s' appears truncated."),
                   filename);
      goto error;
    }

  close (fd);

  gimp_object_set_name (GIMP_OBJECT (pattern), name);
  g_free (name);

  gimp_data_set_filename (GIMP_DATA (pattern), filename);

  /*  Swap the pattern to disk (if we're being stingy with memory) */
  if (stingy_memory_use)
    temp_buf_swap (pattern->mask);

  GIMP_DATA (pattern)->dirty = FALSE;

  return GIMP_DATA (pattern);

 error:
  if (pattern)
    g_object_unref (G_OBJECT (pattern));
  else if (name)
    g_free (name);

  close (fd);

  return NULL;
}

TempBuf *
gimp_pattern_get_mask (const GimpPattern *pattern)
{
  g_return_val_if_fail (GIMP_IS_PATTERN (pattern), NULL);

  return pattern->mask;
}
