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


/*
 *  this is a temp hack
 */
extern Gimp *the_gimp;


void       app_init             (gint       gimp_argc,
				 gchar    **gimp_argv);
void       app_exit             (gboolean   kill_it);
void       app_exit_finish      (void);
gboolean   app_exit_finish_done (void);


#endif /* __APP_PROCS_H__ */
