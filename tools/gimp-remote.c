/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 * 
 * gimp-remote.c
 * Copyright (C) 2000-2003  Sven Neumann <sven@gimp.org>
 *                          Simon Budig <simon@gimp.org>
 *
 * Tells a running gimp to open files by creating a synthetic drop-event.
 *
 * compile with
 *  gcc -o gimp-remote `pkg-config --cflags --libs gtk+-2.0` -lXmu gimp-remote.c
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

/* Disclaimer:
 *
 * It is a really bad idea to use Drag'n'Drop for inter-client
 * communication. Dont even think about doing this in your own newly
 * created application. We do this *only*, because we are in a
 * feature freeze for Gimp 1.2 and adding a completely new communication
 * infrastructure for remote controlling Gimp is definitely a new
 * feature...
 * Think about sockets or Corba when you want to do something similiar.
 * We definitely consider this for Gimp 2.0.
 *                                                Simon
 */


#include "config.h"

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <X11/Xmu/WinUtil.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "libgimpbase/gimpversion.h"


static gboolean  start_new = FALSE;


static GdkWindow *
gimp_remote_find_window (void)
{
  GdkWindow *result = NULL;
  Window     root, parent;
  Window    *children;
  Atom       class_atom;
  Atom       string_atom;
  guint      nchildren;
  gint       i;

  if (XQueryTree (gdk_display,
                  gdk_x11_get_default_root_xwindow (), 
                  &root, &parent, &children, &nchildren) == 0)
    return NULL;
  
  if (! (children && nchildren))
    return NULL;
  
  class_atom  = XInternAtom (gdk_display, "WM_CLASS", TRUE);
  string_atom = XInternAtom (gdk_display, "STRING",   TRUE);

  for (i = nchildren - 1; i >= 0; i--)
    {
      Window  window;
      Atom    ret_type;
      gint    ret_format;
      gulong  bytes_after;
      gulong  nitems;
      guchar *data;

      /*  The XmuClientWindow() function finds a window at or below the
       *  specified window, that has a WM_STATE property. If such a
       *  window is found, it is returned; otherwise the argument window
       *  is returned.
       */

      window = XmuClientWindow (gdk_display, children[i]);

      /*  We are searching the Gimp toolbox: Its WM_CLASS Property
       *  has the values "toolbox\0Gimp\0". This is pretty relieable,
       *  it is more reliable when we ask a special property, explicitely
       *  set from the gimp. See below... :-)
       */

      if (XGetWindowProperty (gdk_display, window,
                              class_atom,
                              0, 32,
                              FALSE,
                              string_atom,
                              &ret_type, &ret_format, &nitems, &bytes_after,
                              &data) == Success &&
          ret_type)
        {
          if (nitems > 12                        &&
              strcmp (data,     "toolbox") == 0  &&
              strcmp (data + 8, "Gimp")    == 0) 
            {
              XFree (data);
              result = gdk_window_foreign_new (window);
              break;
            }

          XFree (data);
        }
    }
  
  XFree (children);

  return result;
}

static void  
source_selection_get (GtkWidget          *widget,
		      GtkSelectionData   *selection_data,
		      guint               info,
		      guint               time,
		      gpointer            data)
{
  gchar *uri = (gchar *) data;

  gtk_selection_data_set (selection_data,
                          selection_data->target,
                          8, uri, strlen (uri));
  gtk_main_quit ();
}

static gboolean
toolbox_hidden (gpointer data)
{
  g_printerr ("Could not connect to the Gimp.\n"
              "Make sure that the Toolbox is visible!\n");
  gtk_main_quit ();

  return FALSE;
}
  
static void
usage (const gchar *name)
{
  g_print ("gimp-remote version %s\n\n", GIMP_VERSION);
  g_print ("Tells a running Gimp to open a (local or remote) image file.\n\n"
	   "Usage: %s [options] [FILE|URI]...\n\n", name);
  g_print ("Valid options are:\n" 
	   "  -h --help       Output this help.\n"
	   "  -v --version    Output version info.\n"
	   "  -n --new        Start gimp if no active gimp window was found.\n\n");
  g_print ("Example:  %s http://www.gimp.org/icons/frontpage-small.gif\n"
	   "     or:  %s localfile.png\n\n", name, name);
}

static void
start_new_gimp (gchar *argv0, GString *file_list)
{     
  gint    i;
  gchar **argv;
  gchar  *gimp, *path, *name, *pwd;
  const gchar *spath;
  
  file_list = g_string_prepend (file_list, "gimp\n");
  argv = g_strsplit (file_list->str, "\n", 0);
  g_string_free (file_list, TRUE);

  /* We are searching for the path the gimp-remote executable lives in */

  /*
   * the "_" environment variable usually gets set by the sh-family of
   * shells. We have to sanity-check it. If we do not find anything
   * usable in it try argv[0], then fall back to search the path.
   */

  gimp = NULL;
  spath = NULL;

  for (i=0; i < 2; i++)
    {
      if (i == 0)
        spath = g_getenv ("_");
      else if (i == 1)
        spath = argv0;

      if (spath)
        {
          name = g_path_get_basename (spath);

          if (!strncmp (name, "gimp-remote", 11))
            {
              if (g_path_is_absolute (spath))
                {
                  path = g_path_get_dirname (spath);
                  gimp = g_strconcat (path, G_DIR_SEPARATOR_S,
                                      "gimp-1.3", NULL);
                  g_free (path);
                }
              else
                {
                  pwd = g_get_current_dir ();
                  path = g_path_get_dirname (spath);
                  gimp = g_strconcat (pwd, G_DIR_SEPARATOR_S, path,
                                      G_DIR_SEPARATOR_S, "gimp-1.3", NULL);
                  g_free (path);
                  g_free (pwd);
                }
            }

          g_free (name);
        }

      if (gimp)
        break;
    }

  for (i = 1; argv[i]; i++)
    {
      if (g_ascii_strncasecmp ("file:", argv[i], 5) == 0)
	argv[i] += 5;
    }

  execv (gimp, argv);
  execvp ("gimp-1.3", argv);
	  
  /*  if execv and execvp return, there was an arror  */
  g_printerr ("Couldn't start gimp-1.3 for the following reason: %s\n",
              g_strerror (errno));
  exit (-1);
}

static void
parse_option (const gchar *progname,
              const gchar *arg)
{
  if (strcmp (arg, "-v") == 0 || 
      strcmp (arg, "--version") == 0)
    {
      g_print ("gimp-remote version %s\n", GIMP_VERSION);
      exit (0);
    }
  else if (strcmp (arg, "-h") == 0 || 
	   strcmp (arg, "-?") == 0 || 
	   strcmp (arg, "--help") == 0 || 
	   strcmp (arg, "--usage") == 0)
    {
      usage (progname);
      exit (0);
    }
  else if (strcmp (arg, "-n") == 0 ||
	   strcmp (arg, "--new") == 0)
    {
      start_new = TRUE;
    }
  else
    {
      g_print ("Unknown option %s\n", arg);
      g_print ("Try gimp-remote --help to get detailed usage instructions.\n");
      exit (0);
    }
}

gint 
main (gint    argc, 
      gchar **argv)
{
  GtkWidget *source;
  GdkWindow *gimp_window;
    
  GdkDragContext  *context;
  GdkDragProtocol  protocol;
  
  GdkAtom   sel_type;
  GdkAtom   sel_id;
  GList    *targetlist;
  guint     timeout;
  gboolean  options = TRUE;
  
  GString  *file_list = g_string_new (NULL);
  gchar    *cwd       = g_get_current_dir ();
  gchar    *file_uri  = "";
  guint    i;
  
  for (i = 1; i < argc; i++)
    {
      if (strlen (argv[i]) == 0)
        continue;
      
      if (options && *argv[i] == '-')
        {
          if (strcmp (argv[i], "--"))
            {
              parse_option (argv[0], argv[i]);
              continue;
            }
          else
            {
              /*  everything following a -- is interpreted as arguments  */
              options = FALSE;
              continue;
            }
        }
      
      /* If not already a valid URI */
      if (g_ascii_strncasecmp ("file:", argv[i], 5) &&
          g_ascii_strncasecmp ("ftp:",  argv[i], 4) &&
          g_ascii_strncasecmp ("http:", argv[i], 5))
        {
          if (g_path_is_absolute (argv[i]))
            file_uri = g_strconcat ("file:", argv[i], NULL);
          else
            file_uri = g_strconcat ("file:", cwd, "/", argv[i], NULL);
        }
      else
        file_uri = g_strdup (argv[i]);
      
      if (file_list->len > 0)
        file_list = g_string_append_c (file_list, '\n');
      
      file_list = g_string_append (file_list, file_uri);
      g_free (file_uri);
    }

  if (file_list->len == 0) 
    {
      usage (argv[0]);
      return EXIT_SUCCESS;
    }

  gtk_init (&argc, &argv); 
  
  /*  locate Gimp window */
  gimp_window = gimp_remote_find_window ();

  if (!gimp_window)
    {
      if (start_new)
        start_new_gimp (argv[0], file_list);
      
      g_printerr ("No gimp window found on display %s\n", gdk_get_display ());
      return EXIT_FAILURE;
    }

  gdk_drag_get_protocol (GDK_WINDOW_XID (gimp_window), &protocol);
  if (protocol != GDK_DRAG_PROTO_XDND)
    {
      g_printerr ("Gimp Window doesnt use Xdnd-Protocol - huh?\n");
      return EXIT_FAILURE;
    }	

  /*  Problem: If the Toolbox is hidden via Tab (gtk_widget_hide)
   *  it does not accept DnD-Operations and gtk_main() will not be
   *  terminated. If the Toolbox is simply unmapped (by the Windowmanager)
   *  DnD works. But in both cases gdk_window_is_visible () == 0.... :-( 
   *  To work around this add a timeout and abort after 1.5 seconds.
   */
 
  timeout = g_timeout_add (1500, toolbox_hidden, NULL);

  /*  set up an DND-source  */
  source = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (source, "selection_get",
                    G_CALLBACK (source_selection_get), file_list->str);
  gtk_widget_realize (source);


  /*  specify the id and the content-type of the selection used to
   *  pass the URIs to Gimp.
   */
  sel_id   = gdk_atom_intern ("XdndSelection", FALSE);
  sel_type = gdk_atom_intern ("text/uri-list", FALSE);
  targetlist = g_list_prepend (NULL, GUINT_TO_POINTER (sel_type));

  /*  assign the selection to our DnD-source  */
  gtk_selection_owner_set (source, sel_id, GDK_CURRENT_TIME);
  gtk_selection_add_target (source, sel_id, sel_type, 0);

  /*  drag_begin/motion/drop  */
  context = gdk_drag_begin (source->window, targetlist);

  gdk_drag_motion (context, gimp_window, protocol, 0, 0,
                   GDK_ACTION_COPY, GDK_ACTION_COPY, GDK_CURRENT_TIME);

  gdk_drag_drop (context, GDK_CURRENT_TIME);

  /*  finally enter the mainloop to handle the events  */
  gtk_main ();

  g_source_remove (timeout);
  g_string_free (file_list, TRUE);
  
  return EXIT_SUCCESS;
}
