/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * Datafiles module copyight (C) 1996 Federico Mena Quintero
 * federico@nuclecu.unam.mx
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

#include <glib.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

#ifdef G_OS_WIN32
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif
#ifndef S_IXUSR
#define S_IXUSR _S_IEXEC
#endif
#endif /* G_OS_WIN32 */

#include "libgimpbase/gimpbase.h"

#include "core-types.h"

#include "gimpdatafiles.h"


static gboolean    filestat_valid = FALSE;
static struct stat filestat;


#ifdef G_OS_WIN32
/*
 * On Windows there is no concept like the Unix executable flag.
 * There is a weak emulation provided by the MS C Runtime using file
 * extensions (com, exe, cmd, bat). This needs to be extended to treat
 * scripts (Python, Perl, ...) as executables, too. We use the PATHEXT
 * variable, which is also used by cmd.exe.
 */
static gboolean
is_script (const gchar *filename)
{
  static gchar **exts = NULL;

  const gchar   *ext = strrchr (filename, '.');
  gchar         *pathext;
  gint           i;

  if (exts == NULL)
    {
      pathext = g_getenv ("PATHEXT");
      if (pathext != NULL)
	{
	  exts = g_strsplit (pathext, G_SEARCHPATH_SEPARATOR_S, 100);
	}
      else
	{
	  exts = g_new (gchar *, 1);
	  exts[0] = NULL;
	}
    }

  i = 0;
  while (exts[i] != NULL)
    {
      if (g_strcasecmp (ext, exts[i]) == 0)
	return TRUE;
      i++;
    }

  return FALSE;
}
#else  /* !G_OS_WIN32 */
#define is_script(filename) FALSE
#endif

gboolean
gimp_datafiles_check_extension (const gchar *filename,
				const gchar *extension)
{
  gint name_len;
  gint ext_len;

  g_return_val_if_fail (filename != NULL, FALSE);
  g_return_val_if_fail (extension != NULL, FALSE);

  name_len = strlen (filename);
  ext_len  = strlen (extension);

  if (! (name_len && ext_len && (name_len > ext_len)))
    return FALSE;

  return (strcmp (&filename[name_len - ext_len], extension) == 0);
}

void
gimp_datafiles_read_directories (const gchar            *path_str,
				 GimpDataFileFlags       flags,
				 GimpDataFileLoaderFunc  loader_func,
				 gpointer                loader_data)
{
  gchar         *local_path;
  GList         *path;
  GList         *list;
  gchar         *filename;
  gint           err;
  DIR           *dir;
  struct dirent *dir_ent;

  g_return_if_fail (path_str != NULL);
  g_return_if_fail (loader_func != NULL);

  local_path = g_strdup (path_str);

#ifdef __EMX__
  /*
   *  Change drive so opendir works.
   */
  if (local_path[1] == ':')
    {
      _chdrive (local_path[0]);
    }
#endif

  path = gimp_path_parse (local_path, 16, TRUE, NULL);

  for (list = path; list; list = g_list_next (list))
    {
      /* Open directory */
      dir = opendir ((gchar *) list->data);

      if (!dir)
	{
	  g_message ("error reading datafiles directory \"%s\"",
		     (gchar *) list->data);
	}
      else
	{
	  while ((dir_ent = readdir (dir)))
	    {
	      if (! strcmp (dir_ent->d_name, ".") ||
		  ! strcmp (dir_ent->d_name, ".."))
		{
		  continue;
		}

	      filename = g_build_filename ((gchar *) list->data,
                                           dir_ent->d_name, NULL);

	      /* Check the file and see that it is not a sub-directory */
	      err = stat (filename, &filestat);

	      if (! err)
		{
		  filestat_valid = TRUE;

		  if (S_ISDIR (filestat.st_mode) && (flags & TYPE_DIRECTORY))
		    {
		      (* loader_func) (filename, loader_data);
		    }
		  else if (S_ISREG (filestat.st_mode) &&
			   (!(flags & MODE_EXECUTABLE) ||
			    (filestat.st_mode & S_IXUSR) ||
			    is_script (filename)))
		    {
		      (* loader_func) (filename, loader_data);
		    }

		  filestat_valid = FALSE;
		}

	      g_free (filename);
	    }

	  closedir (dir);
	}
    }

  gimp_path_free (path);
  g_free (local_path);
}

time_t
gimp_datafile_atime (void)
{
  if (filestat_valid)
    return filestat.st_atime;

  return 0;
}

time_t
gimp_datafile_mtime (void)
{
  if (filestat_valid)
    return filestat.st_mtime;

  return 0;
}

time_t
gimp_datafile_ctime (void)
{
  if (filestat_valid)
    return filestat.st_ctime;

  return 0;
}
