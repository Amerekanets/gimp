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

#include <gtk/gtk.h>

#include "libgimpwidgets/gimpwidgets.h"

#include "gui-types.h"

#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpobject.h"

#include "file/file-open.h"
#include "file/file-save.h"
#include "file/file-utils.h"

#include "display/gimpdisplay.h"
#include "display/gimpdisplay-foreach.h"
#include "display/gimpdisplayshell.h"

#include "file-commands.h"
#include "file-new-dialog.h"
#include "file-open-dialog.h"
#include "file-save-dialog.h"

#include "app_procs.h"
#include "gimprc.h"
#include "undo.h"

#include "libgimp/gimpintl.h"


#define REVERT_DATA_KEY "revert-confirm-dialog"

#define return_if_no_display(gdisp,data) \
  gdisp = gimp_context_get_display (gimp_get_user_context (GIMP (data))); \
  if (!gdisp) return


/*  local function prototypes  */

static void   file_revert_confirm_callback (GtkWidget *widget,
					    gboolean   revert,
					    gpointer   data);


/*  public functions  */

void
file_new_cmd_callback (GtkWidget *widget,
		       gpointer   data,
		       guint      action)
{
  Gimp      *gimp;
  GimpImage *gimage = NULL;

  gimp = GIMP (data);

  /*  if called from the image menu  */
  if (action)
    {
      gimage = gimp_context_get_image (gimp_get_user_context (gimp));
    }

  file_new_dialog_create (gimp, gimage);
}

void
file_open_by_extension_cmd_callback (GtkWidget *widget,
				     gpointer   data)
{
  file_open_dialog_menu_reset ();
}

void
file_open_cmd_callback (GtkWidget *widget,
			gpointer   data)
{
  file_open_dialog_show (GIMP (data));
}

void
file_last_opened_cmd_callback (GtkWidget *widget,
			       gpointer   data,
			       guint      action)
{
  Gimp          *gimp;
  GimpImagefile *imagefile;
  guint          num_entries;

  gimp = GIMP (data);

  num_entries = gimp_container_num_children (gimp->documents);

  if (action >= num_entries)
    return;

  imagefile = (GimpImagefile *)
    gimp_container_get_child_by_index (gimp->documents, action);

  if (imagefile)
    {
      GimpPDBStatusType dummy;

      file_open_with_display (gimp, GIMP_OBJECT (imagefile)->name,
                              &dummy, NULL);
    }
}

void
file_save_by_extension_cmd_callback (GtkWidget *widget,
				     gpointer   data)
{
  file_save_dialog_menu_reset ();
}

void
file_save_cmd_callback (GtkWidget *widget,
			gpointer   data)
{
  GimpDisplay *gdisp;
  return_if_no_display (gdisp, data);

  if (! gimp_image_active_drawable (gdisp->gimage))
    return;

  /*  Only save if the gimage has been modified  */
  if (! gimprc.trust_dirty_flag || gdisp->gimage->dirty != 0)
    {
      const gchar *uri;

      uri = gimp_object_get_name (GIMP_OBJECT (gdisp->gimage));

      if (! uri)
	{
	  file_save_as_cmd_callback (widget, data);
	}
      else
	{
	  GimpPDBStatusType status;

	  status = file_save (gdisp->gimage,
			      uri,
			      uri,
                              NULL,
			      GIMP_RUN_WITH_LAST_VALS,
			      TRUE);

	  if (status != GIMP_PDB_SUCCESS &&
	      status != GIMP_PDB_CANCEL)
	    {
	      /* Error message should be added. --bex */
	      g_message (_("Saving '%s' failed."), uri);
	    }
	}
    }
}

void
file_save_as_cmd_callback (GtkWidget *widget,
			   gpointer   data)
{
  GimpDisplay *gdisp;
  return_if_no_display (gdisp, data);

  file_save_dialog_show (gdisp->gimage);
}

void
file_save_a_copy_cmd_callback (GtkWidget *widget,
                               gpointer   data)
{
  GimpDisplay *gdisp;
  return_if_no_display (gdisp, data);

  file_save_a_copy_dialog_show (gdisp->gimage);
}

void
file_revert_cmd_callback (GtkWidget *widget,
			  gpointer   data)
{
  GimpDisplay *gdisp;
  GtkWidget   *query_box;
  const gchar *uri;

  return_if_no_display (gdisp, data);

  uri = gimp_object_get_name (GIMP_OBJECT (gdisp->gimage));

  query_box = g_object_get_data (G_OBJECT (gdisp->gimage), REVERT_DATA_KEY);

  if (! uri)
    {
      g_message (_("Revert failed.\n"
		   "No file name associated with this image."));
    }
  else if (query_box)
    {
      gtk_window_present (GTK_WINDOW (query_box->window));
    }
  else
    {
      gchar *basename;
      gchar *text;

      basename = g_path_get_basename (uri);

      text = g_strdup_printf (_("Revert '%s' to\n"
				"'%s'?\n\n"
				"(You will lose all your changes,\n"
				"including all undo information)"),
			      basename, uri);

      g_free (basename);

      query_box = gimp_query_boolean_box (_("Revert Image"),
					  gimp_standard_help_func,
					  "file/revert.html",
					  GTK_STOCK_DIALOG_QUESTION,
					  text,
					  GTK_STOCK_YES, GTK_STOCK_NO,
					  G_OBJECT (gdisp->gimage),
					  "disconnect",
					  file_revert_confirm_callback,
					  gdisp->gimage);

      g_free (text);

      g_object_set_data (G_OBJECT (gdisp->gimage), REVERT_DATA_KEY,
			 query_box);

      gtk_widget_show (query_box);
    }
}

void
file_close_cmd_callback (GtkWidget *widget,
			 gpointer   data)
{
  GimpDisplay *gdisp;
  return_if_no_display (gdisp, data);

  gimp_display_shell_close (GIMP_DISPLAY_SHELL (gdisp->shell), FALSE);
}

void
file_quit_cmd_callback (GtkWidget *widget,
			gpointer   data)
{
  app_exit (FALSE);
}


/*  private functions  */

static void
file_revert_confirm_callback (GtkWidget *widget,
			      gboolean   revert,
			      gpointer   data)
{
  GimpImage *old_gimage;

  old_gimage = (GimpImage *) data;

  g_object_set_data (G_OBJECT (old_gimage), REVERT_DATA_KEY, NULL);

  if (revert)
    {
      GimpImage         *new_gimage;
      const gchar       *uri;
      GimpPDBStatusType  status;
      GError            *error = NULL;

      uri = gimp_object_get_name (GIMP_OBJECT (old_gimage));

      new_gimage = file_open_image (old_gimage->gimp,
				    uri,
                                    uri,
				    NULL,
				    GIMP_RUN_INTERACTIVE,
				    &status,
                                    &error);

      if (new_gimage)
	{
	  undo_free (new_gimage);

	  gdisplays_reconnect (old_gimage, new_gimage);

	  gimp_image_clean_all (new_gimage);

          gimp_image_flush (new_gimage);
	}
      else if (status != GIMP_PDB_CANCEL)
	{
          gchar *filename;

          filename = file_utils_uri_to_utf8_filename (uri);

	  g_message (_("Reverting to '%s' failed:\n%s"),
                     filename, error->message);
          g_error_free (error);

          g_free (filename);
	}
    }
}
