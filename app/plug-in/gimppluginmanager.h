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

#ifndef __PLUG_IN_H__
#define __PLUG_IN_H__


#include <time.h>      /* time_t */
#include <sys/types.h> /* pid_t  */

#include "pdb/procedural_db.h"  /* ProcRecord */


#define WRITE_BUFFER_SIZE  512

#define PLUG_IN_RGB_IMAGE       0x01
#define PLUG_IN_GRAY_IMAGE      0x02
#define PLUG_IN_INDEXED_IMAGE   0x04
#define PLUG_IN_RGBA_IMAGE      0x08
#define PLUG_IN_GRAYA_IMAGE     0x10
#define PLUG_IN_INDEXEDA_IMAGE  0x20


struct _PlugIn
{
  guint         open : 1;         /* Is the plug-in open* */
  guint         query : 1;        /* Are we querying the plug-in? */
  guint         synchronous : 1;  /* Is the plug-in running synchronously? */
  guint         recurse : 1;      /* Have we called 'gtk_main' recursively? */
  guint         busy : 1;         /* Is the plug-in busy with a temp proc? */
  pid_t         pid;              /* Plug-ins process id */
  gchar        *args[7];          /* Plug-ins command line arguments */

  GIOChannel   *my_read;          /* App's read and write channels */
  GIOChannel   *my_write;
  GIOChannel   *his_read;         /* Plug-in's read and write channels */
  GIOChannel   *his_write;
#ifdef G_OS_WIN32
  guint         his_thread_id;    /* Plug-in's thread ID */
  gint          his_read_fd;      /* Plug-in's read pipe fd */
#endif

  guint32       input_id;         /* Id of input proc */

  gchar         write_buffer[WRITE_BUFFER_SIZE]; /* Buffer for writing */
  gint          write_buffer_index;              /* Buffer index for writing */

  GSList       *temp_proc_defs;   /* Temporary procedures  */

  GimpProgress *progress;         /* Progress dialog */

  PlugInDef    *user_data;        /* DON'T USE!! */
};

struct _PlugInProcDef
{
  gchar      *prog;
  gchar      *menu_path;
  gchar      *accelerator;
  gchar      *extensions;
  gchar      *prefixes;
  gchar      *magics;
  gchar      *image_types;
  gint        image_types_val;
  ProcRecord  db_info;
  GSList     *extensions_list;
  GSList     *prefixes_list;
  GSList     *magics_list;
  time_t      mtime;
};


/* Initialize the plug-ins */
void            plug_in_init                 (Gimp               *gimp,
                                              GimpInitStatusFunc  status_callback);

/* Kill all running plug-ins */
void            plug_in_kill                 (void);

/*  Add a plug-in to the list of valid plug-ins and query the plug-in
 *  for information if necessary.
 */
void            plug_in_add                  (gchar         *name,
					      gchar         *menu_path,
					      gchar         *accelerator);

/* Get the "image_types" the plug-in works on. */
gchar         * plug_in_image_types          (gchar         *name);

/* Add in the file load/save handler fields procedure. */
PlugInProcDef * plug_in_file_handler         (gchar         *name,
					      gchar         *extensions,
					      gchar         *prefixes,
					      gchar         *magics);

/* Add a plug-in definition. */
void            plug_in_def_add              (PlugInDef     *plug_in_def);

/* Allocate and free a plug-in definition. */
PlugInDef     * plug_in_def_new              (const gchar   *prog);
void            plug_in_def_free             (PlugInDef     *plug_in_def, 
					      gboolean       free_proc_defs);

void      plug_in_def_set_mtime              (PlugInDef     *plug_in_def,
                                              time_t         mtime);
void      plug_in_def_set_locale_domain_name (PlugInDef     *plug_in_def,
                                              const gchar   *domain_name);
void      plug_in_def_set_locale_domain_path (PlugInDef     *plug_in_def,
                                              const gchar   *domain_path);
void      plug_in_def_set_help_path          (PlugInDef     *plug_in_def,
                                              const gchar   *help_path);
void      plug_in_def_add_proc_def           (PlugInDef     *plug_in_def,
                                              PlugInProcDef *proc_def);

/* Retrieve a plug-ins menu path */
gchar         * plug_in_menu_path            (gchar         *name);

/* Retrieve a plug-ins help path */
gchar         * plug_in_help_path            (gchar         *prog_name);

/* Create a new plug-in structure */
PlugIn        * plug_in_new                  (gchar         *name);

/*  Destroy a plug-in structure.
 *  This will close the plug-in first if necessary.
 */
void            plug_in_destroy              (PlugIn        *plug_in);


/* Open a plug-in. This cause the plug-in to run.
 * If returns TRUE, you must destroy the plugin.
 * If returns FALSE, you must not destroy the plugin.
 */
gboolean        plug_in_open                 (PlugIn        *plug_in);

/* Close a plug-in. This kills the plug-in and releases its resources. */
void            plug_in_close                (PlugIn        *plug_in,
					      gboolean       kill_it);

/* Run a plug-in as if it were a procedure database procedure */
Argument      * plug_in_run                  (ProcRecord    *proc_rec,
					      Argument      *args,
					      gint           argc,
					      gboolean       synchronous,
					      gboolean       destroy_values,
					      gint           gdisp_ID);

/*  Run the last plug-in again with the same arguments. Extensions
 *  are exempt from this "privelege".
 */
void            plug_in_repeat               (gboolean       with_interface);

/* Set the sensitivity for plug-in menu items based on the image type. */
void            plug_in_set_menu_sensitivity (GimpImageType  type);


/* Register an internal plug-in.  This is for file load-save
 * handlers, which are organized around the plug-in data structure.
 * This could all be done a little better, but oh well.  -josh
 */
void            plug_in_add_internal         (PlugInProcDef *proc_def);
GSList        * plug_in_extensions_parse     (gchar         *extensions);
gint            plug_in_image_types_parse    (gchar         *image_types);

void            plug_in_progress_init        (PlugIn        *plug_in,
					      gchar         *message,
					      gint           gdisp_ID);
void            plug_in_progress_update      (PlugIn        *plug_in,
					      gdouble        percentage);


extern PlugIn *current_plug_in;
extern GSList *proc_defs;


#endif /* __PLUG_IN_H__ */
