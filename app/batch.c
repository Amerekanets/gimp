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
#include <stdlib.h>

#include <glib-object.h>

#include "core/core-types.h"

#include "core/gimp.h"

#include "batch.h"

#include "pdb/procedural_db.h"

#include "gimp-intl.h"


#define BATCH_DEFAULT_EVAL_PROC   "plug_in_script_fu_eval"


static gboolean  batch_exit_after_callback (Gimp        *gimp,
                                            gboolean     kill_it);
static void      batch_run_cmd             (Gimp        *gimp,
                                            const gchar *proc_name,
                                            ProcRecord  *proc,
                                            const gchar *cmd);


void
batch_run (Gimp         *gimp,
           const gchar  *batch_interpreter,
           const gchar **batch_commands)
{
  ProcRecord *eval_proc;
  gulong      exit_id;
  gint        i;

  if (! batch_commands ||
      ! batch_commands[0])
    return;

  exit_id = g_signal_connect_after (gimp, "exit",
                                    G_CALLBACK (batch_exit_after_callback),
                                    NULL);

  if (! batch_interpreter)
    {
      batch_interpreter = BATCH_DEFAULT_EVAL_PROC;

      g_printerr ("No batch interpreter specified, using the default '%s'.\n",
                  batch_interpreter);
    }

  /*  script-fu text console, hardcoded for backward compatibility  */

  if (strcmp (batch_interpreter, "plug_in_script_fu_eval") == 0 &&
      strcmp (batch_commands[0], "-") == 0)
    {
      batch_commands[0] = "(plug-in-script-fu-text-console RUN-INTERACTIVE)";
      batch_commands[1] = NULL;
    }

  eval_proc = procedural_db_lookup (gimp, batch_interpreter);
  if (! eval_proc)
    {
      g_message (_("The batch interpreter '%s' is not available, "
                   "batch mode disabled."), batch_interpreter);
      return;
    }

  for (i = 0; batch_commands[i]; i++)
    batch_run_cmd (gimp, batch_interpreter, eval_proc, batch_commands[i]);

  g_signal_handler_disconnect (gimp, exit_id);
}


static gboolean
batch_exit_after_callback (Gimp     *gimp,
                           gboolean  kill_it)
{
  if (gimp->be_verbose)
    g_print ("EXIT: %s\n", G_STRLOC);

  exit (EXIT_SUCCESS);

  return TRUE;
}

static void
batch_run_cmd (Gimp        *gimp,
               const gchar *proc_name,
               ProcRecord  *proc,
	       const gchar *cmd)
{
  Argument *args;
  Argument *vals;
  gint      i;

  args = g_new0 (Argument, proc->num_args);
  for (i = 0; i < proc->num_args; i++)
    args[i].arg_type = proc->args[i].arg_type;

  args[0].value.pdb_int     = GIMP_RUN_NONINTERACTIVE;
  args[1].value.pdb_pointer = (gpointer) cmd;

  vals = procedural_db_execute (gimp,
                                gimp_get_user_context (gimp), NULL,
                                proc_name, args);

  switch (vals[0].value.pdb_int)
    {
    case GIMP_PDB_EXECUTION_ERROR:
      g_printerr ("batch command: experienced an execution error.\n");
      break;

    case GIMP_PDB_CALLING_ERROR:
      g_printerr ("batch command: experienced a calling error.\n");
      break;

    case GIMP_PDB_SUCCESS:
      g_printerr ("batch command: executed successfully.\n");
      break;
    }

  procedural_db_destroy_args (vals, proc->num_values);
  g_free (args);

  return;
}
