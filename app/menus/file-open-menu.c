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

#include <string.h>

#include <gtk/gtk.h>

#include "libgimpwidgets/gimpwidgets.h"

#include "gui-types.h"

#include "core/gimp.h"

#include "plug-in/plug-in-proc.h"
#include "plug-in/plug-ins.h"

#include "widgets/gimphelp-ids.h"
#include "widgets/gimpitemfactory.h"

#include "file-commands.h"
#include "file-open-menu.h"
#include "menus.h"

#include "gimp-intl.h"


GimpItemFactoryEntry file_open_menu_entries[] =
{
  { { N_("/Automatic"), NULL,
      file_open_by_extension_cmd_callback, 0 },
    NULL,
    GIMP_HELP_FILE_OPEN_BY_EXTENSION, NULL },

  MENU_SEPARATOR ("/---")
};

gint n_file_open_menu_entries = G_N_ELEMENTS (file_open_menu_entries);


void
file_open_menu_setup (GimpItemFactory *factory)
{
  GSList *list;

  for (list = factory->gimp->load_procs; list; list = g_slist_next (list))
    {
      PlugInProcDef        *file_proc;
      GimpItemFactoryEntry  entry;
      const gchar          *locale_domain = NULL;
      const gchar          *item_type     = NULL;
      const gchar          *stock_id      = NULL;
      gchar                *help_id;
      gboolean              is_xcf;

      file_proc = (PlugInProcDef *) list->data;

      is_xcf = (strcmp (file_proc->db_info.name, "gimp_xcf_load") == 0);

      if (is_xcf)
        {
          item_type = "<StockItem>";
          stock_id  = GIMP_STOCK_WILBER;
          help_id   = g_strdup (GIMP_HELP_FILE_OPEN_XCF);
        }
      else
        {
          const gchar *progname;
          const gchar *help_path;

          progname = plug_in_proc_def_get_progname (file_proc);

          locale_domain = plug_ins_locale_domain (factory->gimp, progname, NULL);
          help_path     = plug_ins_help_path (factory->gimp, progname);

          help_id = plug_in_proc_def_get_help_id (file_proc, help_path);
        }

      entry.entry.path            = strstr (file_proc->menu_path, "/");
      entry.entry.accelerator     = NULL;
      entry.entry.callback        = file_open_type_cmd_callback;
      entry.entry.callback_action = 0;
      entry.entry.item_type       = item_type;
      entry.entry.extra_data      = stock_id;
      entry.quark_string          = NULL;
      entry.help_id               = help_id;
      entry.description           = NULL;

      gimp_item_factory_create_item (factory,
                                     &entry,
                                     locale_domain,
                                     file_proc, 2,
                                     TRUE, FALSE);

      if (is_xcf)
        {
          GtkWidget *menu_item;

          menu_item = gtk_item_factory_get_widget (GTK_ITEM_FACTORY (factory),
                                                   entry.entry.path);

          if (menu_item)
            gtk_menu_reorder_child (GTK_MENU (menu_item->parent),
                                    menu_item, 1);
        }

      g_free (help_id);
    }
}
