/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpwidgets-utils.h
 * Copyright (C) 1999-2003 Michael Natterer <mitch@gimp.org>
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

#ifndef __GIMP_WIDGETS_UTILS_H__
#define __GIMP_WIDGETS_UTILS_H__


void          gimp_message_box           (const gchar *message,
                                          GtkCallback  callback,
                                          gpointer     data);

void          gimp_menu_position         (GtkMenu     *menu,
                                          gint        *x,
                                          gint        *y,
                                          guint       *button,
                                          guint32     *activate_time);

void          gimp_table_attach_stock    (GtkTable    *table,
					  gint         row,
					  const gchar *label_text,
					  gdouble      yalign,
					  GtkWidget   *widget,
					  gint         colspan,
					  const gchar *stock_id);

GtkIconSize   gimp_get_icon_size         (GtkWidget   *widget,
                                          const gchar *stock_id,
                                          GtkIconSize  max_size,
                                          gint         width,
                                          gint         height);

const gchar * gimp_get_mod_name_shift    (void);
const gchar * gimp_get_mod_name_control  (void);
const gchar * gimp_get_mod_name_alt      (void);
const gchar * gimp_get_mod_separator     (void);

void          gimp_get_screen_resolution (GdkScreen   *screen,
                                          gdouble     *xres,
                                          gdouble     *yres);


#endif /* __GIMP_WIDGETS_UTILS_H__ */
