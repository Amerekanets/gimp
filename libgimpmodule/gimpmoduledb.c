/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpmodules.c
 * (C) 1999 Austin Donnelly <austin@gimp.org>
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
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <time.h>

#include <glib-object.h>

#include "libgimpbase/gimpbase.h"

#include "gimpmoduletypes.h"

#include "gimpmodule.h"
#include "gimpmoduledb.h"


enum
{
  ADD,
  REMOVE,
  MODULE_MODIFIED,
  LAST_SIGNAL
};


/*  #define DUMP_DB 1  */


static void         gimp_module_db_class_init     (GimpModuleDBClass *klass);
static void         gimp_module_db_init           (GimpModuleDB      *db);

static void         gimp_module_db_finalize            (GObject      *object);

static void         gimp_module_db_module_initialize   (GimpDatafileData *file_data);

static GimpModule * gimp_module_db_module_find_by_path (GimpModuleDB *db,
                                                        const char   *fullpath);

#ifdef DUMP_DB
static void         gimp_module_db_dump_module         (gpointer      data,
                                                        gpointer      user_data);
#endif

#if 0
static gboolean     gimp_module_db_write_modulerc      (GimpModuleDB *db);
#endif

static void         gimp_module_db_module_on_disk_func (gpointer      data, 
                                                        gpointer      user_data);
static void         gimp_module_db_module_remove_func  (gpointer      data, 
                                                        gpointer      user_data);
static void         gimp_module_db_module_modified     (GimpModule   *module,
                                                        GimpModuleDB *db);


static GObjectClass *parent_class = NULL;

static guint  db_signals[LAST_SIGNAL] = { 0 };


GType
gimp_module_db_get_type (void)
{
  static GType db_type = 0;

  if (! db_type)
    {
      static const GTypeInfo db_info =
      {
        sizeof (GimpModuleDBClass),
        NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) gimp_module_db_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GimpModuleDB),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gimp_module_db_init,
      };

      db_type = g_type_register_static (G_TYPE_OBJECT,
                                        "GimpModuleDB",
                                        &db_info, 0);
    }

  return db_type;
}

static void
gimp_module_db_class_init (GimpModuleDBClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  db_signals[ADD] =
    g_signal_new ("add",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GimpModuleDBClass, add),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
                  GIMP_TYPE_MODULE);

  db_signals[REMOVE] =
    g_signal_new ("remove",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GimpModuleDBClass, remove),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
                  GIMP_TYPE_MODULE);

  db_signals[MODULE_MODIFIED] =
    g_signal_new ("module_modified",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GimpModuleDBClass, module_modified),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
                  GIMP_TYPE_MODULE);

  object_class->finalize = gimp_module_db_finalize;

  klass->add             = NULL;
  klass->remove          = NULL;
}

static void
gimp_module_db_init (GimpModuleDB *db)
{
  db->modules      = NULL;
  db->load_inhibit = NULL;
  db->verbose      = FALSE;
}

static void
gimp_module_db_finalize (GObject *object)
{
  GimpModuleDB *db;

  db = GIMP_MODULE_DB (object);

  if (db->modules)
    {
      g_list_free (db->modules);
      db->modules = NULL;
    }

  if (db->load_inhibit)
    {
      g_free (db->load_inhibit);
      db->load_inhibit = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

GimpModuleDB *
gimp_module_db_new (gboolean verbose)
{
  GimpModuleDB *db;

  db = g_object_new (GIMP_TYPE_MODULE_DB, NULL);

  db->verbose = verbose ? TRUE : FALSE;

  return db;
}

static gboolean
is_in_inhibit_list (const gchar *filename,
                    const gchar *inhibit_list)
{
  gchar       *p;
  gint         pathlen;
  const gchar *start;
  const gchar *end;

  if (! inhibit_list || ! strlen (inhibit_list))
    return FALSE;

  p = strstr (inhibit_list, filename);
  if (!p)
    return FALSE;

  /* we have a substring, but check for colons either side */
  start = p;
  while (start != inhibit_list && *start != G_SEARCHPATH_SEPARATOR)
    start--;

  if (*start == G_SEARCHPATH_SEPARATOR)
    start++;

  end = strchr (p, G_SEARCHPATH_SEPARATOR);
  if (! end)
    end = inhibit_list + strlen (inhibit_list);

  pathlen = strlen (filename);

  if ((end - start) == pathlen)
    return TRUE;

  return FALSE;
}

void
gimp_module_db_set_load_inhibit (GimpModuleDB *db,
                                 const gchar  *load_inhibit)
{
  GimpModule *module;
  GList      *list;

  g_return_if_fail (GIMP_IS_MODULE_DB (db));

  if (db->load_inhibit)
    g_free (db->load_inhibit);

  db->load_inhibit = g_strdup (load_inhibit);

  for (list = db->modules; list; list = g_list_next (list))
    {
      module = (GimpModule *) list->data;

      gimp_module_set_load_inhibit (module,
                                    is_in_inhibit_list (module->filename,
                                                        load_inhibit));
    }
}

const gchar *
gimp_module_db_get_load_inhibit (GimpModuleDB *db)
{
  g_return_val_if_fail (GIMP_IS_MODULE_DB (db), NULL);

  return db->load_inhibit;
}

void
gimp_module_db_load (GimpModuleDB *db,
                     const gchar  *module_path)
{
  g_return_if_fail (GIMP_IS_MODULE_DB (db));
  g_return_if_fail (module_path != NULL);

  if (g_module_supported ())
    gimp_datafiles_read_directories (module_path,
                                     G_FILE_TEST_EXISTS,
				     gimp_module_db_module_initialize,
                                     db);

#ifdef DUMP_DB
  g_list_foreach (db->modules, gimp_module_db_dump_module, NULL);
#endif
}

void
gimp_module_db_refresh (GimpModuleDB *db,
                        const gchar  *module_path)
{
  GList *kill_list = NULL;

  g_return_if_fail (GIMP_IS_MODULE_DB (db));
  g_return_if_fail (module_path != NULL);

  /* remove modules we don't have on disk anymore */
  g_list_foreach (db->modules,
                  gimp_module_db_module_on_disk_func,
                  &kill_list);
  g_list_foreach (kill_list,
                  gimp_module_db_module_remove_func,
                  db);
  g_list_free (kill_list);

  /* walk filesystem and add new things we find */
  gimp_datafiles_read_directories (module_path,
                                   G_FILE_TEST_EXISTS,
				   gimp_module_db_module_initialize,
                                   db);
}

#if 0
static void
add_to_inhibit_string (gpointer data, 
		       gpointer user_data)
{
  GimpModule *module = data;
  GString    *str    = user_data;

  if (module->load_inhibit)
    {
      str = g_string_append_c (str, G_SEARCHPATH_SEPARATOR);
      str = g_string_append (str, module->filename);
    }
}

static gboolean
gimp_modules_write_modulerc (Gimp *gimp)
{
  GString  *str;
  gchar    *p;
  gchar    *filename;
  FILE     *fp;
  gboolean  saved = FALSE;

  str = g_string_new (NULL);
  gimp_container_foreach (gimp->modules, add_to_inhibit_string, str);
  if (str->len > 0)
    p = str->str + 1;
  else
    p = "";

  filename = gimp_personal_rc_file ("modulerc");
  fp = fopen (filename, "wt");
  g_free (filename);
  if (fp)
    {
      fprintf (fp, "(module-load-inhibit \"%s\")\n", p);
      fclose (fp);
      saved = TRUE;
    }

  g_string_free (str, TRUE);

  return saved;
}
#endif

/* name must be of the form lib*.so (Unix) or *.dll (Win32) */
static gboolean
valid_module_name (const gchar *filename)
{
  gchar *basename;

  basename = g_path_get_basename (filename);

#if !defined(G_OS_WIN32) && !defined(G_WITH_CYGWIN) && !defined(__EMX__)
  if (strncmp (basename, "lib", 3))
    goto no_module;

  if (! gimp_datafiles_check_extension (basename, ".so"))
    goto no_module;
#else
  if (! gimp_datafiles_check_extension (basename, ".dll"))
    goto no_module;
#endif

  g_free (basename);

  return TRUE;

 no_module:
  g_free (basename);

  return FALSE;
}

static void
gimp_module_db_module_initialize (GimpDatafileData *file_data)
{
  GimpModuleDB *db;
  GimpModule   *module;
  gboolean      load_inhibit;

  db = GIMP_MODULE_DB (file_data->user_data);

  if (! valid_module_name (file_data->filename))
    return;

  /* don't load if we already know about it */
  if (gimp_module_db_module_find_by_path (db, file_data->filename))
    return;

  load_inhibit = is_in_inhibit_list (file_data->filename,
                                     db->load_inhibit);

  module = gimp_module_new (file_data->filename,
                            load_inhibit,
                            db->verbose);

  g_signal_connect (G_OBJECT (module), "modified",
                    G_CALLBACK (gimp_module_db_module_modified),
                    db);

  db->modules = g_list_append (db->modules, module);
  g_signal_emit (G_OBJECT (db), db_signals[ADD], 0, module);
}

static GimpModule *
gimp_module_db_module_find_by_path (GimpModuleDB *db,
                                    const char   *fullpath)
{
  GimpModule *module;
  GList      *list;

  for (list = db->modules; list; list = g_list_next (list))
    {
      module = (GimpModule *) list->data;

      if (! strcmp (module->filename, fullpath))
        return module;
    }

  return NULL;
}

#ifdef DUMP_DB
static void
gimp_module_db_dump_module (gpointer data, 
                            gpointer user_data)
{
  GimpModule *i = data;

  g_print ("\n%s: %s\n",
	   i->filename,
           gimp_module_state_name (i->state));

  g_print ("  module:%p  lasterr:%s  query:%p register:%p\n",
	   i->module,
           i->last_module_error ? i->last_module_error : "NONE",
	   i->query_module,
	   i->register_module);

  if (i->info)
    {
      g_print ("  purpose:   %s\n"
               "  author:    %s\n"
               "  version:   %s\n"
               "  copyright: %s\n"
               "  date:      %s\n",
               i->info->purpose   ? i->info->purpose   : "NONE",
               i->info->author    ? i->info->author    : "NONE",
               i->info->version   ? i->info->version   : "NONE",
               i->info->copyright ? i->info->copyright : "NONE",
               i->info->date      ? i->info->date      : "NONE");
    }
}
#endif

static void
gimp_module_db_module_on_disk_func (gpointer data, 
                                    gpointer user_data)
{
  GimpModule  *module;
  GList      **kill_list;
  gint         old_on_disk;

  module    = (GimpModule *) data;
  kill_list = (GList **) user_data;

  old_on_disk = module->on_disk;

  module->on_disk = g_file_test (module->filename, G_FILE_TEST_IS_REGULAR);

  /* if it's not on the disk, and it isn't in memory, mark it to be
   * removed later.
   */
  if (! module->on_disk && ! module->module)
    {
      *kill_list = g_list_append (*kill_list, module);
      module = NULL;
    }

  if (module && module->on_disk != old_on_disk)
    gimp_module_modified (module);
}

static void
gimp_module_db_module_remove_func (gpointer data, 
                                   gpointer user_data)
{
  GimpModule   *module;
  GimpModuleDB *db;

  module = (GimpModule *)   data;
  db     = (GimpModuleDB *) user_data;

  g_signal_handlers_disconnect_by_func (G_OBJECT (module),
                                        gimp_module_db_module_modified,
                                        db);

  g_list_remove (db->modules, module);
  g_signal_emit (G_OBJECT (db), db_signals[REMOVE], 0, module);
}

static void
gimp_module_db_module_modified (GimpModule   *module,
                                GimpModuleDB *db)
{
  g_signal_emit (G_OBJECT (db), db_signals[MODULE_MODIFIED], 0, module);
}
