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

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef  WAIT_ANY
#define  WAIT_ANY -1
#endif

#include <locale.h>

#include <glib-object.h>

#include "libgimpbase/gimpbase.h"

#include "config/gimpconfig-dump.h"

#include "core/core-types.h"
#include "core/gimp.h"

#include "app_procs.h"
#include "errors.h"

#include "gimp-intl.h"


#ifdef G_OS_WIN32
#include <windows.h>
#else
static void   gimp_sigfatal_handler  (gint         sig_num);
static void   gimp_sigchld_handler   (gint         sig_num);
#endif


static void   gimp_show_version      (void);
static void   gimp_show_help         (const gchar *progname);
static void   gimp_text_console_exit (gint         status);


/*
 *  argv processing:
 *      Arguments are either switches, their associated
 *      values, or image files.  As switches and their
 *      associated values are processed, those slots in
 *      the argv[] array are NULLed. We do this because
 *      unparsed args are treated as images to load on
 *      startup.
 *
 *      The GTK switches are processed first (X switches are
 *      processed here, not by any X routines).  Then the
 *      general GIMP switches are processed.  Any args
 *      left are assumed to be image files the GIMP should
 *      display.
 *
 *      The exception is the batch switch.  When this is
 *      encountered, all remaining args are treated as batch
 *      commands.
 */

int
main (int    argc,
      char **argv)
{
  gchar              *full_prog_name          = NULL;
  gchar              *alternate_system_gimprc = NULL;
  gchar              *alternate_gimprc        = NULL;
  gchar             **batch_cmds              = NULL;
  gboolean            show_help               = FALSE;
  gboolean            no_interface            = FALSE;
  gboolean            no_data                 = FALSE;
  gboolean            no_splash               = FALSE;
  gboolean            no_splash_image         = FALSE;
  gboolean            be_verbose              = FALSE;
  gboolean            use_shm                 = FALSE;
  gboolean            use_mmx                 = TRUE;
  gboolean            console_messages        = FALSE;
  gboolean            use_debug_handler       = FALSE;
  GimpStackTraceMode  stack_trace_mode        = GIMP_STACK_TRACE_QUERY;
  gboolean            restore_session         = FALSE;
  gint       i, j;

#if 0
  g_mem_set_vtable (glib_mem_profiler_table);
  g_atexit (g_mem_profile);
#endif

  /* Initialize variables */

  full_prog_name = argv[0];

  /* Initialize i18n support */

  setlocale (LC_ALL, "");

  bindtextdomain (GETTEXT_PACKAGE"-libgimp", gimp_locale_directory ());
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE"-libgimp", "UTF-8");
#endif

  bindtextdomain (GETTEXT_PACKAGE, gimp_locale_directory ());
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

  textdomain (GETTEXT_PACKAGE);

  /* Check argv[] for "--no-interface" before trying to initialize gtk+.
   * We also check for "--help" or "--version" here since those shouldn't
   * require gui libs either.
   */
  for (i = 1; i < argc; i++)
    {
      const gchar *arg = argv[i];

      if (arg[0] != '-')
        continue;

      if ((strcmp (arg, "--no-interface") == 0) ||
	  (strcmp (arg, "-i") == 0))
	{
	  no_interface = TRUE;
	}
      else if ((strcmp (arg, "--version") == 0) ||
               (strcmp (arg, "-v") == 0))
	{
	  gimp_show_version ();
	  gimp_text_console_exit (EXIT_SUCCESS);
	}
      else if ((strcmp (arg, "--help") == 0) ||
	       (strcmp (arg, "-h") == 0))
	{
	  gimp_show_help (full_prog_name);
	  gimp_text_console_exit (EXIT_SUCCESS);
	}
      else if (strncmp (arg, "--dump-gimprc", 13) == 0)
        {
          GimpConfigDumpFormat format = GIMP_CONFIG_DUMP_NONE;

          if (strcmp (arg, "--dump-gimprc") == 0)
            format = GIMP_CONFIG_DUMP_GIMPRC;
          if (strcmp (arg, "--dump-gimprc-system") == 0)
            format = GIMP_CONFIG_DUMP_GIMPRC_SYSTEM;
          else if (strcmp (arg, "--dump-gimprc-manpage") == 0)
            format = GIMP_CONFIG_DUMP_GIMPRC_MANPAGE;

          if (format)
            {
              g_type_init ();
              the_gimp = g_object_new (GIMP_TYPE_GIMP, NULL);

              gimp_text_console_exit (gimp_config_dump (format) ?
                                      EXIT_SUCCESS : EXIT_FAILURE);
            }
        }
    }

  if (no_interface)
    {
      gchar *basename;

      basename = g_path_get_basename (argv[0]);
      g_set_prgname (basename);
      g_free (basename);

      g_type_init ();
    }
  else if (! app_gui_libs_init (&argc, &argv))
    {
      const gchar *msg;

      msg = _("GIMP could not initialize the graphical user interface.\n"
              "Make sure a proper setup for your display environment exists.");
      g_print ("%s\n\n", msg);

      gimp_text_console_exit (EXIT_FAILURE);
    }

  g_set_application_name (_("The GIMP"));

#if defined (HAVE_SHM_H) || defined (G_OS_WIN32)
  use_shm = TRUE;
#endif

  batch_cmds    = g_new (gchar *, argc);
  batch_cmds[0] = NULL;

  for (i = 1; i < argc; i++)
    {
      if (strcmp (argv[i], "--g-fatal-warnings") == 0)
	{
          GLogLevelFlags fatal_mask;

          fatal_mask = g_log_set_always_fatal (G_LOG_FATAL_MASK);
          fatal_mask |= G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL;
          g_log_set_always_fatal (fatal_mask);
 	  argv[i] = NULL;
	}
      else if ((strcmp (argv[i], "--no-interface") == 0) ||
               (strcmp (argv[i], "-i") == 0))
	{
	  no_interface = TRUE;
 	  argv[i] = NULL;
	}
      else if ((strcmp (argv[i], "--batch") == 0) ||
	       (strcmp (argv[i], "-b") == 0))
	{
	  argv[i] = NULL;
	  for (j = 0, i++ ; i < argc; j++, i++)
	    {
	      batch_cmds[j] = argv[i];
	      argv[i] = NULL;
	    }
	  batch_cmds[j] = NULL;

	  if (batch_cmds[0] == NULL)  /* We need at least one batch command */
	    show_help = TRUE;
	}
      else if (strcmp (argv[i], "--system-gimprc") == 0)
	{
 	  argv[i] = NULL;
	  if (argc <= ++i)
            {
	      show_help = TRUE;
	    }
          else
            {
	      alternate_system_gimprc = argv[i];
	      argv[i] = NULL;
            }
	}
      else if ((strcmp (argv[i], "--gimprc") == 0) ||
               (strcmp (argv[i], "-g") == 0))
	{
	  argv[i] = NULL;
	  if (argc <= ++i)
            {
	      show_help = TRUE;
	    }
          else
            {
	      alternate_gimprc = argv[i];
	      argv[i] = NULL;
            }
	}
      else if ((strcmp (argv[i], "--no-data") == 0) ||
	       (strcmp (argv[i], "-d") == 0))
	{
	  no_data = TRUE;
 	  argv[i] = NULL;
	}
      else if ((strcmp (argv[i], "--no-splash") == 0) ||
	       (strcmp (argv[i], "-s") == 0))
	{
	  no_splash = TRUE;
 	  argv[i] = NULL;
	}
      else if ((strcmp (argv[i], "--no-splash-image") == 0) ||
	       (strcmp (argv[i], "-S") == 0))
	{
	  no_splash_image = TRUE;
 	  argv[i] = NULL;
	}
      else if (strcmp (argv[i], "--verbose") == 0)
	{
	  be_verbose = TRUE;
 	  argv[i] = NULL;
	}
      else if (strcmp (argv[i], "--no-shm") == 0)
	{
	  use_shm = FALSE;
 	  argv[i] = NULL;
	}
      else if (strcmp (argv[i], "--no-mmx") == 0)
	{
	  use_mmx = FALSE;
 	  argv[i] = NULL;
	}
      else if (strcmp (argv[i], "--debug-handlers") == 0)
	{
	  use_debug_handler = TRUE;
 	  argv[i] = NULL;
	}
      else if ((strcmp (argv[i], "--console-messages") == 0) ||
	       (strcmp (argv[i], "-c") == 0))
	{
	  console_messages = TRUE;
 	  argv[i] = NULL;
	}
      else if ((strcmp (argv[i], "--restore-session") == 0) ||
	       (strcmp (argv[i], "-r") == 0))
	{
	  restore_session = TRUE;
 	  argv[i] = NULL;
	}
      else if (strcmp (argv[i], "--enable-stack-trace") == 0)
	{
 	  argv[i] = NULL;
	  if (argc <= ++i)
            {
	      show_help = TRUE;
	    }
          else
            {
	      if (! strcmp (argv[i], "never"))
		stack_trace_mode = GIMP_STACK_TRACE_NEVER;
	      else if (! strcmp (argv[i], "query"))
		stack_trace_mode = GIMP_STACK_TRACE_QUERY;
	      else if (! strcmp (argv[i], "always"))
		stack_trace_mode = GIMP_STACK_TRACE_ALWAYS;
	      else
		show_help = TRUE;

	      argv[i] = NULL;
            }
	}
      else if (strcmp (argv[i], "--") == 0)
        {
          /*
           *  everything after "--" is a parameter (i.e. image to load)
           */
          argv[i] = NULL;
          break;
        }
      else if (argv[i][0] == '-')
	{
          /*
           *  anything else starting with a '-' is an error.
           */
	  g_print (_("\nInvalid option \"%s\"\n"), argv[i]);
	  show_help = TRUE;
	}
    }

#ifdef G_OS_WIN32
  /* Common windoze apps don't have a console at all. So does Gimp
   * - if appropiate. This allows to compile as console application
   * with all it's benefits (like inheriting the console) but hide
   * it, if the user doesn't want it.
   */
  if (!show_help && !be_verbose && !console_messages)
    FreeConsole ();
#endif

  if (show_help)
    {
      gimp_show_help (full_prog_name);
      gimp_text_console_exit (EXIT_FAILURE);
    }

#ifndef G_OS_WIN32

  /* No use catching these on Win32, the user won't get any
   * stack trace from glib anyhow. It's better to let Windows inform
   * about the program error, and offer debugging (if the user
   * has installed MSVC or some other compiler that knows how to
   * install itself as a handler for program errors).
   */

  /* Handle fatal signals */

  /* these are handled by gimp_terminate() */
  gimp_signal_private (SIGHUP,  gimp_sigfatal_handler, 0);
  gimp_signal_private (SIGINT,  gimp_sigfatal_handler, 0);
  gimp_signal_private (SIGQUIT, gimp_sigfatal_handler, 0);
  gimp_signal_private (SIGABRT, gimp_sigfatal_handler, 0);
  gimp_signal_private (SIGTERM, gimp_sigfatal_handler, 0);

  if (stack_trace_mode != GIMP_STACK_TRACE_NEVER)
    {
      /* these are handled by gimp_fatal_error() */
      gimp_signal_private (SIGBUS,  gimp_sigfatal_handler, 0);
      gimp_signal_private (SIGSEGV, gimp_sigfatal_handler, 0);
      gimp_signal_private (SIGFPE,  gimp_sigfatal_handler, 0);
    }

  /* Ignore SIGPIPE because plug_in.c handles broken pipes */

  gimp_signal_private (SIGPIPE, SIG_IGN, 0);

  /* Collect dead children */

  gimp_signal_private (SIGCHLD, gimp_sigchld_handler, SA_RESTART);

#endif /* G_OS_WIN32 */

  gimp_errors_init (full_prog_name,
                    use_debug_handler,
                    stack_trace_mode);

  app_init (full_prog_name,
            argc - 1,
	    argv + 1,
            alternate_system_gimprc,
            alternate_gimprc,
            (const gchar **) batch_cmds,
            no_interface,
            no_data,
            no_splash,
            no_splash_image,
            be_verbose,
            use_shm,
            use_mmx,
            console_messages,
            stack_trace_mode,
            restore_session);

  g_free (batch_cmds);

  return EXIT_SUCCESS;
}


static void
gimp_show_version (void)
{
  g_print ("%s %s\n", _("GIMP version"), GIMP_VERSION);
}

static void
gimp_show_help (const gchar *progname)
{
  gimp_show_version ();

  g_print (_("\nUsage: %s [option ... ] [file ... ]\n\n"), progname);
  g_print (_("Options:\n"));
  g_print (_("  -h, --help               Output this help.\n"));
  g_print (_("  -v, --version            Output version information.\n"));
  g_print (_("  --verbose                Show startup messages.\n"));
  g_print (_("  --no-shm                 Do not use shared memory between GIMP and plugins.\n"));
  g_print (_("  --no-mmx                 Do not use MMX routines.\n"));
  g_print (_("  -d, --no-data            Do not load brushes, gradients, palettes, patterns.\n"));
  g_print (_("  -i, --no-interface       Run without a user interface.\n"));
  g_print (_("  --display <display>      Use the designated X display.\n"));
  g_print (_("  -s, --no-splash          Do not show the startup window.\n"));
  g_print (_("  -S, --no-splash-image    Do not add an image to the startup window.\n"));
  g_print (_("  -r, --restore-session    Try to restore saved session.\n"));
  g_print (_("  -g, --gimprc <gimprc>    Use an alternate gimprc file.\n"));
  g_print (_("  --system-gimprc <gimprc> Use an alternate system gimprc file.\n"));
  g_print (_("  --dump-gimprc            Output a gimprc file with default settings.\n"));
  g_print (_("  -c, --console-messages   Display warnings to console instead of a dialog box.\n"));
  g_print (_("  --debug-handlers         Enable non-fatal debugging signal handlers.\n"));
  g_print (_("  --enable-stack-trace <never | query | always>\n"
             "                           Debugging mode for fatal signals.\n"));
  g_print (_("  -b, --batch <commands>   Process commands in batch mode.\n"));
  g_print ("\n");
};


static void
gimp_text_console_exit (gint status)
{
#ifdef G_OS_WIN32
  /* Give them time to read the message if it was printed in a
   * separate console window. I would really love to have
   * some way of asking for confirmation to close the console
   * window.
   */
  HANDLE console;
  DWORD mode;

  console = GetStdHandle (STD_OUTPUT_HANDLE);
  if (GetConsoleMode (console, &mode) != 0)
    {
      g_print (_("(This console window will close in ten seconds)\n"));
      Sleep(10000);
    }
#endif

  exit (status);
}


#ifdef G_OS_WIN32

/* In case we build this as a windowed application */

#ifdef __GNUC__
#  ifndef _stdcall
#    define _stdcall  __attribute__((stdcall))
#  endif
#endif

int _stdcall
WinMain (struct HINSTANCE__ *hInstance,
	 struct HINSTANCE__ *hPrevInstance,
	 char               *lpszCmdLine,
	 int                 nCmdShow)
{
  return main (__argc, __argv);
}

#endif /* G_OS_WIN32 */


#ifndef G_OS_WIN32

/* gimp core signal handler for fatal signals */

static void
gimp_sigfatal_handler (gint sig_num)
{
  switch (sig_num)
    {
    case SIGHUP:
    case SIGINT:
    case SIGQUIT:
    case SIGABRT:
    case SIGTERM:
      gimp_terminate (g_strsignal (sig_num));
      break;

    case SIGBUS:
    case SIGSEGV:
    case SIGFPE:
    default:
      gimp_fatal_error (g_strsignal (sig_num));
      break;
    }
}

/* gimp core signal handler for death-of-child signals */

static void
gimp_sigchld_handler (gint sig_num)
{
  gint pid;
  gint status;

  while (TRUE)
    {
      pid = waitpid (WAIT_ANY, &status, WNOHANG);

      if (pid <= 0)
	break;
    }
}

#endif /* ! G_OS_WIN32 */
