/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimphelp.c
 * Copyright (C) 1999-2000 Michael Natterer <mitch@gimp.org>
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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include "sys/types.h"

#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "core/core-types.h"

#include "app_procs.h"
#include "gimphelp.h"
#include "gimprc.h"
#include "plug_in.h"

#include "libgimp/gimpintl.h"


#ifndef G_OS_WIN32
#define DEBUG_HELP
#endif

typedef struct _GimpIdleHelp GimpIdleHelp;

struct _GimpIdleHelp
{
  gchar *help_path;
  gchar *help_data;
};

/*  local function prototypes  */
static gint      gimp_idle_help     (gpointer     data);
static gboolean  gimp_help_internal (const gchar *help_path,
				     const gchar *current_locale,
				     const gchar *help_data);
static void      gimp_help_netscape (const gchar *help_path,
				     const gchar *current_locale,
				     const gchar *help_data);

/**********************/
/*  public functions  */
/**********************/

/*  The standard help function  */
void
gimp_standard_help_func (const gchar *help_data)
{
  gimp_help (NULL, help_data);
}

/*  the main help function  */
void
gimp_help (const gchar *help_path,
	   const gchar *help_data)
{
  if (gimprc.use_help)
    {
      GimpIdleHelp *idle_help;

      idle_help = g_new0 (GimpIdleHelp, 1);

      if (help_path && strlen (help_path))
	idle_help->help_path = g_strdup (help_path);

      if (help_data && strlen (help_data))
	idle_help->help_data = g_strdup (help_data);

      gtk_idle_add ((GtkFunction) gimp_idle_help, (gpointer) idle_help);
    }
}

/*********************/
/*  local functions  */
/*********************/

static gint
gimp_idle_help (gpointer data)
{
  GimpIdleHelp *idle_help;
  static gchar *current_locale = "C";

  idle_help = (GimpIdleHelp *) data;

  if (idle_help->help_data == NULL && gimprc.help_browser != HELP_BROWSER_GIMP)
    idle_help->help_data = g_strdup ("introduction.html");

#ifdef DEBUG_HELP
  if (idle_help->help_path)
    g_print ("Help Path: %s\n", idle_help->help_path);
  else
    g_print ("Help Path: NULL\n");

  if (idle_help->help_data)
    g_print ("Help Page: %s\n", idle_help->help_data);
  else
    g_print ("Help Page: NULL\n");

  g_print ("\n");
#endif  /*  DEBUG_HELP  */

  switch (gimprc.help_browser)
    {
    case HELP_BROWSER_GIMP:
      if (gimp_help_internal (idle_help->help_path,
			      current_locale,
			      idle_help->help_data))
	break;

    case HELP_BROWSER_NETSCAPE:
      gimp_help_netscape (idle_help->help_path,
			  current_locale,
			  idle_help->help_data);
      break;

    default:
      break;
    }

  if (idle_help->help_path)
    g_free (idle_help->help_path);
  if (idle_help->help_data)
    g_free (idle_help->help_data);
  g_free (idle_help);

  return FALSE;
}

static void
gimp_help_internal_not_found_callback (GtkWidget *widget,
				       gboolean   use_netscape,
				       gpointer   data)
{
  GList *update = NULL;
  GList *remove = NULL;

  if (use_netscape)
    {
      gimprc.help_browser = HELP_BROWSER_NETSCAPE;

      update = g_list_append (update, "help-browser");
      save_gimprc (&update, &remove);
    }
  
  gtk_main_quit ();
}

static gboolean
gimp_help_internal (const gchar *help_path,
		    const gchar *current_locale,
		    const gchar *help_data)
{
  ProcRecord *proc_rec;

  /*  Check if a help browser is already running  */
  proc_rec = procedural_db_lookup (the_gimp,
				   "extension_gimp_help_browser_temp");

  if (proc_rec == NULL)
    {
      Argument *args = NULL;

      proc_rec = procedural_db_lookup (the_gimp,
				       "extension_gimp_help_browser");

      if (proc_rec == NULL)
	{
	  GtkWidget *not_found =
	    gimp_query_boolean_box (_("Could not find GIMP Help Browser"),
				    NULL, NULL, FALSE,
				    _("Could not find the GIMP Help Browser procedure.\n"
				      "It probably was not compiled because\n"
				      "you don't have GtkXmHTML installed."),
				    _("Use Netscape instead"),
				    _("Cancel"), 
				    NULL, NULL,
				    gimp_help_internal_not_found_callback,
				    NULL);
	  gtk_widget_show (not_found);
	  gtk_main ();
	  
	  return (gimprc.help_browser != HELP_BROWSER_NETSCAPE);
	}

      args = g_new (Argument, 4);
      args[0].arg_type = GIMP_PDB_INT32;
      args[0].value.pdb_int = RUN_INTERACTIVE;
      args[1].arg_type = GIMP_PDB_STRING;
      args[1].value.pdb_pointer = (gpointer) help_path;
      args[2].arg_type = GIMP_PDB_STRING;
      args[2].value.pdb_pointer = (gpointer) current_locale;
      args[3].arg_type = GIMP_PDB_STRING;
      args[3].value.pdb_pointer = (gpointer) help_data;

      plug_in_run (proc_rec, args, 4, FALSE, TRUE, 0);

      g_free (args);
    }
  else
    {
      Argument *return_vals;
      gint      nreturn_vals;

      return_vals =
        procedural_db_run_proc (the_gimp,
				"extension_gimp_help_browser_temp",
                                &nreturn_vals,
				GIMP_PDB_STRING, help_path,
				GIMP_PDB_STRING, current_locale,
                                GIMP_PDB_STRING, help_data,
                                GIMP_PDB_END);

      procedural_db_destroy_args (return_vals, nreturn_vals);
    }

  return TRUE;
}

static void
gimp_help_netscape (const gchar *help_path,
		    const gchar *current_locale,
		    const gchar *help_data)
{
  Argument *return_vals;
  gint      nreturn_vals;
  gchar    *url;

  if (help_data[0] == '/')  /* _not_ g_path_is_absolute() */
    {
      url = g_strconcat ("file:",
			 help_data,
			 NULL);
    }
  else
    {
      if (!help_path)
	{
	  url = g_strconcat ("file:",
			     gimp_data_directory (),
			     "/help/",
			     current_locale, "/",
			     help_data,
			     NULL);
	}
      else
	{
	  url = g_strconcat ("file:",
			     help_path, "/",
			     current_locale, "/",
			     help_data,
			     NULL);
	}
    }

  return_vals =
    procedural_db_run_proc (the_gimp,
			    "extension_web_browser",
			    &nreturn_vals,
			    GIMP_PDB_INT32,  RUN_NONINTERACTIVE,
			    GIMP_PDB_STRING, url,
			    GIMP_PDB_INT32,  FALSE,
			    GIMP_PDB_END);

  procedural_db_destroy_args (return_vals, nreturn_vals);

  g_free (url);
}
