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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib-object.h>

#include "core/core-types.h"

#include "core/gimp.h"

#include "batch.h"

#include "pdb/procedural_db.h"


static void   batch_run_cmd      (Gimp        *gimp,
                                  const gchar *cmd);
static void   batch_perl_server  (Gimp        *gimp,
                                  GimpRunMode  run_mode,
                                  gint         flags,
                                  gint         extra);


static ProcRecord *eval_proc = NULL;


void
batch_init (Gimp         *gimp,
            const gchar **batch_cmds)
{
  gboolean perl_server_already_running = FALSE;
  gint     i;

  if (batch_cmds[0] && strcmp (batch_cmds[0], "-") == 0)
    {
      batch_cmds[0] = "(plug-in-script-fu-text-console RUN-INTERACTIVE)";
      batch_cmds[1] = NULL;
    }

  for (i = 0; batch_cmds[i]; i++)
    {
      /* until --batch-interp=xxx or something similar is implemented
       * and gimp-1.0 is not extinct use a shortcut to speed up starting the
       * perl-server tremendously. This is also fully compatible with 1.0.
       */
      {
        gint  run_mode, flags, extra;

        if (sscanf (batch_cmds[i],
                    "(extension%*[-_]perl%*[-_]server %i %i %i)",
                    &run_mode, &flags, &extra) == 3)
          {
            if (!perl_server_already_running)
              {
                batch_perl_server (gimp, run_mode, flags, extra);
                perl_server_already_running = TRUE;
              }
            continue;
          }
      }

      if (! eval_proc)
        eval_proc = procedural_db_lookup (gimp, "plug_in_script_fu_eval");

      if (! eval_proc)
        {
          g_message ("script-fu not available: batch mode disabled\n");
          return;
        }

      batch_run_cmd (gimp, batch_cmds[i]);
    }
}


static void
batch_run_cmd (Gimp        *gimp,
	       const gchar *cmd)
{
  Argument *args;
  Argument *vals;
  gint      i;

  if (g_ascii_strcasecmp (cmd, "(gimp-quit 0)") == 0)
    {
      gimp_exit (gimp, TRUE);
      return;
    }

  args = g_new0 (Argument, eval_proc->num_args);
  for (i = 0; i < eval_proc->num_args; i++)
    args[i].arg_type = eval_proc->args[i].arg_type;

  args[0].value.pdb_int     = GIMP_RUN_NONINTERACTIVE;
  args[1].value.pdb_pointer = (gpointer) cmd;

  vals = procedural_db_execute (gimp, "plug_in_script_fu_eval", args);

  switch (vals[0].value.pdb_int)
    {
    case GIMP_PDB_EXECUTION_ERROR:
      g_print ("batch command: experienced an execution error.\n");
      break;
    case GIMP_PDB_CALLING_ERROR:
      g_print ("batch command: experienced a calling error.\n");
      break;
    case GIMP_PDB_SUCCESS:
      g_print ("batch command: executed successfully.\n");
      break;
    default:
      break;
    }

  procedural_db_destroy_args (vals, eval_proc->num_values);
  g_free (args);

  return;
}

static void
batch_perl_server (Gimp        *gimp,
                   GimpRunMode  run_mode,
                   gint         flags,
                   gint         extra)
{
  ProcRecord *pserver_proc;
  Argument   *args;
  Argument   *vals;
  gint        i;

  pserver_proc = procedural_db_lookup (gimp, "extension_perl_server");

  if (!pserver_proc)
    {
      g_message ("extension_perl_server not available: "
                 "unable to start the perl server");
      return;
    }

  args = g_new0 (Argument, pserver_proc->num_args);
  for (i = 0; i < pserver_proc->num_args; i++)
    args[i].arg_type = pserver_proc->args[i].arg_type;

  args[0].value.pdb_int = run_mode;
  args[1].value.pdb_int = flags;
  args[2].value.pdb_int = extra;

  vals = procedural_db_execute (gimp, "extension_perl_server", args);

  switch (vals[0].value.pdb_int)
    {
    case GIMP_PDB_EXECUTION_ERROR:
      g_printerr ("perl server: experienced an execution error.\n");
      break;
    case GIMP_PDB_CALLING_ERROR:
      g_printerr ("perl server: experienced a calling error.\n");
      break;
    case GIMP_PDB_SUCCESS:
      g_printerr ("perl server: executed successfully.\n");
      break;
    default:
      break;
    }
  
  procedural_db_destroy_args (vals, pserver_proc->num_values);
  g_free(args);

  return;
}
