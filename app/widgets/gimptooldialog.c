/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpviewabledialog.c
 * Copyright (C) 2000 Michael Natterer <mitch@gimp.org>
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

#include "widgets-types.h"

#include "core/gimpobject.h"
#include "core/gimptoolinfo.h"

#include "gimpdialogfactory.h"
#include "gimptooldialog.h"
#include "gimpviewabledialog.h"


GtkWidget *
gimp_tool_dialog_new (GimpToolInfo     *tool_info,
                      const gchar      *desc,
                      ...)
{
  GtkWidget   *dialog;
  const gchar *stock_id;
  gchar       *identifier;
  va_list      args;

  g_return_val_if_fail (GIMP_IS_TOOL_INFO (tool_info), NULL);

  stock_id = gimp_viewable_get_stock_id (GIMP_VIEWABLE (tool_info));

  dialog = gimp_viewable_dialog_new (NULL,
                                     tool_info->blurb,
                                     GIMP_OBJECT (tool_info)->name,
                                     stock_id,
                                     desc ? desc : tool_info->help,
                                     gimp_standard_help_func,
                                     tool_info->help_id,
                                     NULL);

  gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_UTILITY);

  va_start (args, desc);
  gimp_dialog_create_action_areav (GIMP_DIALOG (dialog), args);
  va_end (args);

  identifier = g_strconcat (GIMP_OBJECT (tool_info)->name, "-dialog", NULL);

  gimp_dialog_factory_add_foreign (gimp_dialog_factory_from_name ("toplevel"),
                                   identifier,
                                   dialog);

  g_free (identifier);

  return dialog;
}
