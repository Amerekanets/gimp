/* The GIMP -- an open manipulation program
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

#ifndef __FILE_DIALOG_MENU_H__
#define __FILE_DIALOG_MENU_H__


void   file_dialog_menu_setup (GimpUIManager *manager,
                               const gchar   *ui_path,
                               GSList        *file_procs,
                               const gchar   *xcf_proc_name);


#endif /* __FILE_DIALOG_MENU_H__ */
