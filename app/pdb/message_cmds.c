/* The GIMP -- an image manipulation program
 * Copyright (C) 1995-2003 Spencer Kimball and Peter Mattis
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

/* NOTE: This file is autogenerated by pdbgen.pl. */

#include "config.h"

#include <string.h>

#include <glib-object.h>

#include "pdb-types.h"
#include "gimp-pdb.h"
#include "gimpprocedure.h"
#include "core/gimpparamspecs.h"

#include "core/gimp.h"
#include "gimp-intl.h"
#include "plug-in/plug-in-progress.h"
#include "plug-in/plug-in.h"


static GValueArray *
message_invoker (GimpProcedure     *procedure,
                 Gimp              *gimp,
                 GimpContext       *context,
                 GimpProgress      *progress,
                 const GValueArray *args)
{
  gboolean success = TRUE;
  const gchar *message;

  message = g_value_get_string (&args->values[0]);

  if (success)
    {
      if (gimp->current_plug_in)
        plug_in_progress_message (gimp->current_plug_in, message);
      else
        gimp_message (gimp, NULL, message);
    }

  return gimp_procedure_get_return_values (procedure, success);
}

static GValueArray *
message_get_handler_invoker (GimpProcedure     *procedure,
                             Gimp              *gimp,
                             GimpContext       *context,
                             GimpProgress      *progress,
                             const GValueArray *args)
{
  GValueArray *return_vals;
  gint32 handler = 0;

  handler = gimp->message_handler;

  return_vals = gimp_procedure_get_return_values (procedure, TRUE);
  g_value_set_enum (&return_vals->values[1], handler);

  return return_vals;
}

static GValueArray *
message_set_handler_invoker (GimpProcedure     *procedure,
                             Gimp              *gimp,
                             GimpContext       *context,
                             GimpProgress      *progress,
                             const GValueArray *args)
{
  gboolean success = TRUE;
  gint32 handler;

  handler = g_value_get_enum (&args->values[0]);

  if (success)
    {
      gimp->message_handler = handler;
    }

  return gimp_procedure_get_return_values (procedure, success);
}

void
register_message_procs (Gimp *gimp)
{
  GimpProcedure *procedure;

  /*
   * gimp-message
   */
  procedure = gimp_procedure_new ();
  gimp_procedure_initialize (procedure, GIMP_INTERNAL, 1, 0,
                             message_invoker);
  gimp_procedure_set_static_strings (procedure,
                                     "gimp-message",
                                     "gimp-message",
                                     "Displays a dialog box with a message.",
                                     "Displays a dialog box with a message. Useful for status or error reporting. The message must be in UTF-8 encoding.",
                                     "Manish Singh",
                                     "Manish Singh",
                                     "1998",
                                     NULL);

  gimp_procedure_add_argument (procedure,
                               gimp_param_spec_string ("message",
                                                       "message",
                                                       "Message to display in the dialog",
                                                       FALSE, FALSE,
                                                       NULL,
                                                       GIMP_PARAM_READWRITE));
  gimp_pdb_register (gimp, procedure);

  /*
   * gimp-message-get-handler
   */
  procedure = gimp_procedure_new ();
  gimp_procedure_initialize (procedure, GIMP_INTERNAL, 0, 1,
                             message_get_handler_invoker);
  gimp_procedure_set_static_strings (procedure,
                                     "gimp-message-get-handler",
                                     "gimp-message-get-handler",
                                     "Returns the current state of where warning messages are displayed.",
                                     "This procedure returns the way g_message warnings are displayed. They can be shown in a dialog box or printed on the console where gimp was started.",
                                     "Manish Singh",
                                     "Manish Singh",
                                     "1998",
                                     NULL);

  gimp_procedure_add_return_value (procedure,
                                   g_param_spec_enum ("handler",
                                                      "handler",
                                                      "The current handler type: { GIMP_MESSAGE_BOX (0), GIMP_CONSOLE (1), GIMP_ERROR_CONSOLE (2) }",
                                                      GIMP_TYPE_MESSAGE_HANDLER_TYPE,
                                                      GIMP_MESSAGE_BOX,
                                                      GIMP_PARAM_READWRITE));
  gimp_pdb_register (gimp, procedure);

  /*
   * gimp-message-set-handler
   */
  procedure = gimp_procedure_new ();
  gimp_procedure_initialize (procedure, GIMP_INTERNAL, 1, 0,
                             message_set_handler_invoker);
  gimp_procedure_set_static_strings (procedure,
                                     "gimp-message-set-handler",
                                     "gimp-message-set-handler",
                                     "Controls where warning messages are displayed.",
                                     "This procedure controls how g_message warnings are displayed. They can be shown in a dialog box or printed on the console where gimp was started.",
                                     "Manish Singh",
                                     "Manish Singh",
                                     "1998",
                                     NULL);

  gimp_procedure_add_argument (procedure,
                               g_param_spec_enum ("handler",
                                                  "handler",
                                                  "The new handler type: { GIMP_MESSAGE_BOX (0), GIMP_CONSOLE (1), GIMP_ERROR_CONSOLE (2) }",
                                                  GIMP_TYPE_MESSAGE_HANDLER_TYPE,
                                                  GIMP_MESSAGE_BOX,
                                                  GIMP_PARAM_READWRITE));
  gimp_pdb_register (gimp, procedure);

}
