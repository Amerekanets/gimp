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


#include <glib-object.h>

#include "pdb-types.h"
#include "gimpargument.h"
#include "gimpprocedure.h"
#include "procedural_db.h"
#include "core/gimpparamspecs.h"

#include "core/gimp.h"

static GimpProcedure fonts_popup_proc;
static GimpProcedure fonts_close_popup_proc;
static GimpProcedure fonts_set_popup_proc;

void
register_font_select_procs (Gimp *gimp)
{
  GimpProcedure *procedure;

  /*
   * fonts_popup
   */
  procedure = gimp_procedure_init (&fonts_popup_proc, 3, 0);
  gimp_procedure_add_argument (procedure,
                               GIMP_PDB_STRING,
                               gimp_param_spec_string ("font-callback",
                                                       "font callback",
                                                       "The callback PDB proc to call when font selection is made",
                                                       FALSE, FALSE,
                                                       NULL,
                                                       GIMP_PARAM_READWRITE));
  gimp_procedure_add_argument (procedure,
                               GIMP_PDB_STRING,
                               gimp_param_spec_string ("popup-title",
                                                       "popup title",
                                                       "Title to give the font popup window",
                                                       FALSE, FALSE,
                                                       NULL,
                                                       GIMP_PARAM_READWRITE));
  gimp_procedure_add_argument (procedure,
                               GIMP_PDB_STRING,
                               gimp_param_spec_string ("initial-font",
                                                       "initial font",
                                                       "The name of the font to set as the first selected",
                                                       FALSE, TRUE,
                                                       NULL,
                                                       GIMP_PARAM_READWRITE));
  procedural_db_register (gimp, procedure);

  /*
   * fonts_close_popup
   */
  procedure = gimp_procedure_init (&fonts_close_popup_proc, 1, 0);
  gimp_procedure_add_argument (procedure,
                               GIMP_PDB_STRING,
                               gimp_param_spec_string ("font-callback",
                                                       "font callback",
                                                       "The name of the callback registered for this popup",
                                                       FALSE, FALSE,
                                                       NULL,
                                                       GIMP_PARAM_READWRITE));
  procedural_db_register (gimp, procedure);

  /*
   * fonts_set_popup
   */
  procedure = gimp_procedure_init (&fonts_set_popup_proc, 2, 0);
  gimp_procedure_add_argument (procedure,
                               GIMP_PDB_STRING,
                               gimp_param_spec_string ("font-callback",
                                                       "font callback",
                                                       "The name of the callback registered for this popup",
                                                       FALSE, FALSE,
                                                       NULL,
                                                       GIMP_PARAM_READWRITE));
  gimp_procedure_add_argument (procedure,
                               GIMP_PDB_STRING,
                               gimp_param_spec_string ("font-name",
                                                       "font name",
                                                       "The name of the font to set as selected",
                                                       FALSE, FALSE,
                                                       NULL,
                                                       GIMP_PARAM_READWRITE));
  procedural_db_register (gimp, procedure);

}

static GimpArgument *
fonts_popup_invoker (GimpProcedure      *procedure,
                     Gimp               *gimp,
                     GimpContext        *context,
                     GimpProgress       *progress,
                     const GimpArgument *args)
{
  gboolean success = TRUE;
  const gchar *font_callback;
  const gchar *popup_title;
  const gchar *initial_font;

  font_callback = g_value_get_string (&args[0].value);
  popup_title = g_value_get_string (&args[1].value);
  initial_font = g_value_get_string (&args[2].value);

  if (success)
    {
      if (gimp->no_interface ||
          ! procedural_db_lookup (gimp, font_callback) ||
          ! gimp_pdb_dialog_new (gimp, context, gimp->fonts,
                                 popup_title, font_callback, initial_font,
                                 NULL))
        success = FALSE;
    }

  return gimp_procedure_get_return_values (procedure, success);
}

static GimpProcedure fonts_popup_proc =
{
  TRUE, TRUE,
  "gimp-fonts-popup",
  "gimp-fonts-popup",
  "Invokes the Gimp font selection.",
  "This procedure popups the font selection dialog.",
  "Sven Neumann <sven@gimp.org>",
  "Sven Neumann",
  "2003",
  NULL,
  GIMP_INTERNAL,
  0, NULL, 0, NULL,
  { { fonts_popup_invoker } }
};

static GimpArgument *
fonts_close_popup_invoker (GimpProcedure      *procedure,
                           Gimp               *gimp,
                           GimpContext        *context,
                           GimpProgress       *progress,
                           const GimpArgument *args)
{
  gboolean success = TRUE;
  const gchar *font_callback;

  font_callback = g_value_get_string (&args[0].value);

  if (success)
    {
      if (gimp->no_interface ||
          ! procedural_db_lookup (gimp, font_callback) ||
          ! gimp_pdb_dialog_close (gimp, gimp->fonts, font_callback))
        success = FALSE;
    }

  return gimp_procedure_get_return_values (procedure, success);
}

static GimpProcedure fonts_close_popup_proc =
{
  TRUE, TRUE,
  "gimp-fonts-close-popup",
  "gimp-fonts-close-popup",
  "Popdown the Gimp font selection.",
  "This procedure closes an opened font selection dialog.",
  "Sven Neumann <sven@gimp.org>",
  "Sven Neumann",
  "2003",
  NULL,
  GIMP_INTERNAL,
  0, NULL, 0, NULL,
  { { fonts_close_popup_invoker } }
};

static GimpArgument *
fonts_set_popup_invoker (GimpProcedure      *procedure,
                         Gimp               *gimp,
                         GimpContext        *context,
                         GimpProgress       *progress,
                         const GimpArgument *args)
{
  gboolean success = TRUE;
  const gchar *font_callback;
  const gchar *font_name;

  font_callback = g_value_get_string (&args[0].value);
  font_name = g_value_get_string (&args[1].value);

  if (success)
    {
      if (gimp->no_interface ||
          ! procedural_db_lookup (gimp, font_callback) ||
          ! gimp_pdb_dialog_set (gimp, gimp->fonts, font_callback, font_name,
                                 NULL))
        success = FALSE;
    }

  return gimp_procedure_get_return_values (procedure, success);
}

static GimpProcedure fonts_set_popup_proc =
{
  TRUE, TRUE,
  "gimp-fonts-set-popup",
  "gimp-fonts-set-popup",
  "Sets the current font selection in a popup.",
  "Sets the current font selection in a popup.",
  "Sven Neumann <sven@gimp.org>",
  "Sven Neumann",
  "2003",
  NULL,
  GIMP_INTERNAL,
  0, NULL, 0, NULL,
  { { fonts_set_popup_invoker } }
};
