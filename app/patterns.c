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
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

#include <glib.h>

#include "datafiles.h"
#include "patterns.h"
#include "pattern_header.h"
#include "gimpcontext.h"
#include "gimprc.h"

#include "libgimp/gimpintl.h"

/*  global variables  */
GPattern *active_pattern = NULL;
GSList   *pattern_list   = NULL;
gint      num_patterns   = 0;

/*  static variables  */
static GPattern *standard_pattern = NULL;

/*  local function prototypes  */
static void     load_pattern           (gchar *);
static void     pattern_free_func      (gpointer, gpointer);
static gint     pattern_compare_func   (gconstpointer, 
					gconstpointer);

/*  public functions  */

void
patterns_init (gboolean no_data)
{
  GSList *list;

  if (pattern_list)
    patterns_free ();

  if (pattern_path != NULL && !no_data)
    datafiles_read_directories (pattern_path, load_pattern, 0);

  /*  assign indexes to the loaded patterns  */
  for (list = pattern_list; list; list = g_slist_next (list))
    {
      /*  Set the pattern index  */
      ((GPattern *) list->data)->index = num_patterns++;
    }

  gimp_context_refresh_patterns ();
}

void
patterns_free (void)
{
  if (pattern_list)
    {
      g_slist_foreach (pattern_list, pattern_free_func, NULL);
      g_slist_free (pattern_list);
    }

  num_patterns = 0;
  pattern_list = NULL;
}

GPattern *
patterns_get_standard_pattern (void)
{
  if (! standard_pattern)
    {
      guchar *data;
      gint row, col;

      standard_pattern = g_new (GPattern, 1);

      standard_pattern->filename = NULL;
      standard_pattern->name     = g_strdup ("Standard");
      standard_pattern->index    = -1;  /*  not part of the pattern list  */
      standard_pattern->mask     = temp_buf_new (32, 32, 3, 0, 0, NULL);

      data = temp_buf_data (standard_pattern->mask);

      for (row = 0; row < standard_pattern->mask->height; row++)
	for (col = 0; col < standard_pattern->mask->width; col++)
	  {
	    memset (data, (col % 2) && (row % 2) ? 255 : 0, 3);
	    data += 3;
	  }
    }

  return standard_pattern;
}

GPattern *
pattern_list_get_pattern_by_index (GSList *list,
				   gint    index)
{
  GPattern *pattern = NULL;

  list = g_slist_nth (list, index);
  if (list)
    pattern = (GPattern *) list->data;

  return pattern;
}

GPattern *
pattern_list_get_pattern (GSList *list,
			  gchar  *name)
{
  GPattern *pattern;

  for (; list; list = g_slist_next (list))
    {
      pattern = (GPattern *) list->data;
      
      if (!strcmp (pattern->name, name))
	return pattern;
    }

  return NULL;
}

gboolean
pattern_load (GPattern *pattern,
	      FILE     *fp,
	      gchar    *filename)
{
  PatternHeader header;
  gint   bn_size;
  guchar buf [sizeof (PatternHeader)];
  guint *hp;
  gint   i;

  /*  Read in the header size  */
  if ((fread (buf, 1, sizeof (PatternHeader), fp)) < sizeof (PatternHeader))
    {
      fclose (fp);
      pattern_free (pattern);
      return FALSE;
    }

  /*  rearrange the bytes in each unsigned int  */
  hp = (guint *) &header;
  for (i = 0; i < (sizeof (PatternHeader) / 4); i++)
    hp [i] = (buf [i * 4] << 24) + (buf [i * 4 + 1] << 16) +
             (buf [i * 4 + 2] << 8) + (buf [i * 4 + 3]);

  /*  Check for correct file format */
  if (header.magic_number != GPATTERN_MAGIC)
    {
      /*  One thing that can save this error is if the pattern is version 1  */
      if (header.version != 1)
	{
	  fclose (fp);
	  pattern_free (pattern);
	  return FALSE;
	}
    }
  /*  Check for correct version  */
  if (header.version != GPATTERN_FILE_VERSION)
    {
      g_message (_("Unknown GIMP pattern version #%d in \"%s\"\n"),
		 header.version, filename);
      fclose (fp);
      pattern_free (pattern);
      return FALSE;
    }

  /*  Get a new pattern mask  */
  pattern->mask =
    temp_buf_new (header.width, header.height, header.bytes, 0, 0, NULL);

  /*  Read in the pattern name  */
  if ((bn_size = (header.header_size - sizeof (PatternHeader))))
    {
      pattern->name = g_new (gchar, bn_size);
      if ((fread (pattern->name, 1, bn_size, fp)) < bn_size)
	{
	  g_message (_("Error in GIMP pattern file \"%s\""), filename);
	  fclose (fp);
	  pattern_free (pattern);
	  return FALSE;
	}
    }
  else
    pattern->name = g_strdup (_("Unnamed"));

  /*  Read the pattern mask data  */
  /*  Read the image data  */
  if ((fread (temp_buf_data (pattern->mask), 1,
	      header.width * header.height * header.bytes, fp)) <
      header.width * header.height * header.bytes)
    {
      g_message (_("GIMP pattern file \"%s\" appears to be truncated."),
		 filename);
    }

  /*  success  */
  return TRUE;
}

void
pattern_free (GPattern *pattern)
{
  if (pattern->mask)
    temp_buf_free (pattern->mask);
  if (pattern->filename)
    g_free (pattern->filename);
  if (pattern->name)
    g_free (pattern->name);

  g_free (pattern);
}

/*  private functions  */

static void
pattern_free_func (gpointer data,
		   gpointer dummy)
{
  pattern_free ((GPattern *) data);
}

static gint
pattern_compare_func (gconstpointer first,
		      gconstpointer second)
{
  return strcmp (((const GPattern *) first)->name, 
		 ((const GPattern *) second)->name);
}

static void
load_pattern (gchar *filename)
{
  GPattern *pattern;
  FILE *fp;

  pattern = g_new0 (GPattern, 1);

  pattern->filename = g_strdup (filename);
  pattern->name     = NULL;
  pattern->mask     = NULL;

  /*  Open the requested file  */
  if (! (fp = fopen (filename, "rb")))
    {
      pattern_free (pattern);
      return;
    }

  if (! pattern_load (pattern, fp, filename))
    {
      g_message (_("Error loading pattern \"%s\""), filename);
      return;
    }

  /*  Clean up  */
  fclose (fp);

  /*temp_buf_swap (pattern->mask);*/

  pattern_list = g_slist_insert_sorted (pattern_list, pattern,
					pattern_compare_func);
}
