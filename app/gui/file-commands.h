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

#ifndef __FILE_COMMANDS_H__
#define __FILE_COMMANDS_H__


void   file_new_cmd_callback               (GtkWidget   *widget,
					    gpointer     data,
					    guint        action);

void   file_open_by_extension_cmd_callback (GtkWidget   *widget,
					    gpointer     data,
					    guint        action);
void   file_open_type_cmd_callback         (GtkWidget   *widget,
					    gpointer     data,
					    guint        action);
void   file_open_cmd_callback              (GtkWidget   *widget,
					    gpointer     data,
					    guint        action);
void   file_last_opened_cmd_callback       (GtkWidget   *widget,
					    gpointer     data,
					    guint        action);

void   file_save_by_extension_cmd_callback (GtkWidget   *widget,
					    gpointer     data,
					    guint        action);
void   file_save_type_cmd_callback         (GtkWidget   *widget,
					    gpointer     data,
					    guint        action);
void   file_save_cmd_callback              (GtkWidget   *widget,
					    gpointer     data,
					    guint        action);
void   file_save_as_cmd_callback           (GtkWidget   *widget,
					    gpointer     data,
					    guint        action);
void   file_save_a_copy_cmd_callback       (GtkWidget   *widget,
					    gpointer     data,
					    guint        action);

void   file_revert_cmd_callback            (GtkWidget   *widget,
					    gpointer     data,
					    guint        action);
void   file_pref_cmd_callback              (GtkWidget   *widget,
					    gpointer     data,
					    guint        action);
void   file_close_cmd_callback             (GtkWidget   *widget,
					    gpointer     data,
					    guint        action);
void   file_quit_cmd_callback              (GtkWidget   *widget,
					    gpointer     data,
					    guint        action);

void   file_file_open_dialog               (Gimp        *gimp,
                                            const gchar *uri);


#endif /* __FILE_COMMANDS_H__ */
