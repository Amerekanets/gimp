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

#ifndef __APP_PROCS_H__
#define __APP_PROCS_H__


#ifndef GIMP_APP_GLUE_COMPILATION
#error You must not #include "app_procs.h" from an app/ subdir
#endif


gboolean  app_libs_init (gboolean            *no_interface,
                         gint                *gimp_argc,
                         gchar             ***gimp_argv);
void      app_abort     (gboolean             no_interface,
                         const gchar         *abort_message) G_GNUC_NORETURN;
void      app_exit      (gint                 status) G_GNUC_NORETURN;

void      app_run       (const gchar         *full_prog_name,
                         gint                 gimp_argc,
                         gchar              **gimp_argv,
                         const gchar         *alternate_system_gimprc,
                         const gchar         *alternate_gimprc,
                         const gchar         *session_name,
                         const gchar         *batch_interpreter,
                         const gchar        **batch_commands,
                         gboolean             no_interface,
                         gboolean             no_data,
                         gboolean             no_fonts,
                         gboolean             no_splash,
                         gboolean             be_verbose,
                         gboolean             use_shm,
                         gboolean             use_cpu_accel,
                         gboolean             console_messages,
                         GimpStackTraceMode   stack_trace_mode,
                         GimpPDBCompatMode    pdb_compat_mode);


#endif /* __APP_PROCS_H__ */
