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

#include "plug-in/plug-ins.h"

#include "widgets/gimphelp-ids.h"
#include "widgets/gimpitemfactory.h"

#include "debug-commands.h"
#include "dialogs-commands.h"
#include "file-commands.h"
#include "help-commands.h"
#include "menus.h"
#include "plug-in-menus.h"
#include "toolbox-menu.h"

#include "gimp-intl.h"


GimpItemFactoryEntry toolbox_menu_entries[] =
{
  /*  <Toolbox>/File  */

  MENU_BRANCH (N_("/_File")),

  { { N_("/File/_New..."), "<control>N",
      file_new_cmd_callback, 0,
      "<StockItem>", GTK_STOCK_NEW },
    NULL,
    GIMP_HELP_FILE_NEW, NULL },
  { { N_("/File/_Open..."), "<control>O",
      file_open_cmd_callback, 0,
      "<StockItem>", GTK_STOCK_OPEN },
    NULL,
    GIMP_HELP_FILE_OPEN, NULL },

  /*  <Toolbox>/File/Open Recent  */

  MENU_BRANCH (N_("/File/Open _Recent")),

  { { N_("/File/Open Recent/(None)"), NULL, NULL, 0 },
    NULL, NULL, NULL },

  MENU_SEPARATOR ("/File/Open Recent/---"),

  { { N_("/File/Open Recent/Document _History..."), "foo",
      dialogs_create_dockable_cmd_callback, 0,
      "<StockItem>", GTK_STOCK_OPEN },
    "gimp-document-list",
    GIMP_HELP_DOCUMENT_DIALOG, NULL },

  /*  <Toolbox>/File/Acquire  */

  MENU_BRANCH (N_("/File/_Acquire")),

  MENU_SEPARATOR ("/File/---"),

  { { N_("/File/_Preferences..."), NULL,
      dialogs_create_toplevel_cmd_callback, 0,
      "<StockItem>", GTK_STOCK_PREFERENCES },
    "gimp-preferences-dialog",
    GIMP_HELP_PREFERENCES_DIALOG, NULL },

  /*  <Toolbox>/File/Dialogs  */

  MENU_BRANCH (N_("/File/_Dialogs")),

  MENU_BRANCH (N_("/File/Dialogs/Create New Doc_k")),

  { { N_("/File/Dialogs/Create New Dock/_Layers, Channels & Paths..."), NULL,
      dialogs_create_lc_cmd_callback, 0 },
    NULL,
    NULL, NULL },
  { { N_("/File/Dialogs/Create New Dock/_Brushes, Patterns & Gradients..."), NULL,
      dialogs_create_data_cmd_callback, 0 },
    NULL,
    NULL, NULL },
  { { N_("/File/Dialogs/Create New Dock/_Misc. Stuff..."), NULL,
      dialogs_create_stuff_cmd_callback, 0 },
    NULL,
    NULL, NULL },

  { { N_("/File/Dialogs/Tool _Options..."), "<control><shift>T",
      dialogs_create_dockable_cmd_callback, 0,
      "<StockItem>", GIMP_STOCK_TOOL_OPTIONS },
    "gimp-tool-options",
    GIMP_HELP_TOOL_OPTIONS_DIALOG, NULL },
  { { N_("/File/Dialogs/_Device Status..."), NULL,
      dialogs_create_dockable_cmd_callback, 0,
      "<StockItem>", GIMP_STOCK_DEVICE_STATUS },
    "gimp-device-status",
    GIMP_HELP_DEVICE_STATUS_DIALOG, NULL },

  MENU_SEPARATOR ("/File/Dialogs/---"),

  { { N_("/File/Dialogs/_Layers..."), "<control>L",
      dialogs_create_dockable_cmd_callback, 0,
      "<StockItem>", GIMP_STOCK_LAYERS },
    "gimp-layer-list",
    NULL, NULL },
  { { N_("/File/Dialogs/_Channels..."), NULL,
      dialogs_create_dockable_cmd_callback, 0,
      "<StockItem>", GIMP_STOCK_CHANNELS },
    "gimp-channel-list",
    NULL, NULL },
  { { N_("/File/Dialogs/_Paths..."), NULL,
      dialogs_create_dockable_cmd_callback, 0,
      "<StockItem>", GIMP_STOCK_PATHS },
    "gimp-vectors-list",
    NULL, NULL },
  { { N_("/File/Dialogs/_Indexed Palette..."), NULL,
      dialogs_create_dockable_cmd_callback, 0,
      "<StockItem>", GIMP_STOCK_INDEXED_PALETTE },
    "gimp-indexed-palette",
    GIMP_HELP_INDEXED_PALETTE_DIALOG, NULL },
  { { N_("/File/Dialogs/_Selection Editor..."), NULL,
      dialogs_create_dockable_cmd_callback, 0,
      "<StockItem>", GIMP_STOCK_TOOL_RECT_SELECT },
    "gimp-selection-editor",
    GIMP_HELP_SELECT_DIALOG, NULL },
  { { N_("/File/Dialogs/Na_vigation..."), NULL,
      dialogs_create_dockable_cmd_callback, 0,
      "<StockItem>", GIMP_STOCK_NAVIGATION },
    "gimp-navigation-view",
    GIMP_HELP_NAVIGATION_DIALOG, NULL },
  { { N_("/File/Dialogs/_Undo History..."), NULL,
      dialogs_create_dockable_cmd_callback, 0,
      "<StockItem>", GIMP_STOCK_UNDO_HISTORY },
    "gimp-undo-history",
    GIMP_HELP_UNDO_DIALOG, NULL },

  MENU_SEPARATOR ("/File/Dialogs/---"),

  { { N_("/File/Dialogs/Colo_rs..."), NULL,
      dialogs_create_dockable_cmd_callback, 0,
      "<StockItem>", GIMP_STOCK_DEFAULT_COLORS },
    "gimp-color-editor",
    NULL, NULL },
  { { N_("/File/Dialogs/Brus_hes..."), "<control><shift>B",
      dialogs_create_dockable_cmd_callback, 0,
      "<StockItem>", GIMP_STOCK_TOOL_PAINTBRUSH },
    "gimp-brush-grid",
    GIMP_HELP_BRUSH_DIALOG, NULL },
  { { N_("/File/Dialogs/P_atterns..."), "<control><shift>P",
      dialogs_create_dockable_cmd_callback, 0,
      "<StockItem>", GIMP_STOCK_TOOL_BUCKET_FILL },
    "gimp-pattern-grid",
    GIMP_HELP_PATTERN_DIALOG, NULL },
  { { N_("/File/Dialogs/_Gradients..."), "<control>G",
      dialogs_create_dockable_cmd_callback, 0,
      "<StockItem>", GIMP_STOCK_TOOL_BLEND },
    "gimp-gradient-list",
    GIMP_HELP_GRADIENT_DIALOG, NULL },
  { { N_("/File/Dialogs/Pal_ettes..."), "<control>P",
      dialogs_create_dockable_cmd_callback, 0,
      "<StockItem>", GTK_STOCK_SELECT_COLOR },
    "gimp-palette-list",
    GIMP_HELP_PALETTE_DIALOG, NULL },
  { { N_("/File/Dialogs/_Fonts..."), NULL,
      dialogs_create_dockable_cmd_callback, 0,
      "<StockItem>", GTK_STOCK_SELECT_FONT },
    "gimp-font-list",
    GIMP_HELP_FONT_DIALOG, NULL },
  { { N_("/File/Dialogs/_Buffers..."), "foo",
      dialogs_create_dockable_cmd_callback, 0,
      "<StockItem>", GTK_STOCK_PASTE },
    "gimp-buffer-list",
    GIMP_HELP_BUFFER_DIALOG, NULL },

  MENU_SEPARATOR ("/File/Dialogs/---"),

  { { N_("/File/Dialogs/I_mages..."), NULL,
      dialogs_create_dockable_cmd_callback, 0,
      "<StockItem>", GIMP_STOCK_IMAGES },
    "gimp-image-list",
    GIMP_HELP_IMAGE_DIALOG, NULL },
  { { N_("/File/Dialogs/Document Histor_y..."), "",
      dialogs_create_dockable_cmd_callback, 0,
      "<StockItem>", GTK_STOCK_OPEN },
    "gimp-document-list",
    GIMP_HELP_DOCUMENT_DIALOG, NULL },
  { { N_("/File/Dialogs/_Templates..."), "",
      dialogs_create_dockable_cmd_callback, 0,
      "<StockItem>", GIMP_STOCK_TEMPLATE },
    "gimp-template-list",
    GIMP_HELP_TEMPLATE_DIALOG, NULL },
  { { N_("/File/Dialogs/Error Co_nsole..."), NULL,
      dialogs_create_dockable_cmd_callback, 0,
      "<StockItem>", GIMP_STOCK_WARNING },
    "gimp-error-console",
    GIMP_HELP_ERROR_DIALOG, NULL },

#ifdef ENABLE_DEBUG_ENTRIES
  MENU_BRANCH (N_("/File/D_ebug")),

  { { "/File/Debug/_Mem Profile", NULL,
      debug_mem_profile_cmd_callback, 0 },
    NULL, NULL, NULL },
  { { "/File/Debug/_Dump Items", NULL,
      debug_dump_menus_cmd_callback, 0 },
    NULL, NULL, NULL },
#endif

  MENU_SEPARATOR ("/File/---"),

  { { N_("/File/_Quit"), "<control>Q",
      file_quit_cmd_callback, 0,
      "<StockItem>", GTK_STOCK_QUIT },
    NULL,
    GIMP_HELP_FILE_QUIT, NULL },

  /*  <Toolbox>/Xtns  */

  MENU_BRANCH (N_("/_Xtns")),

  { { N_("/Xtns/_Module Manager..."), NULL,
      dialogs_create_toplevel_cmd_callback, 0 },
    "gimp-module-manager-dialog",
    GIMP_HELP_MODULE_DIALOG, NULL },

  MENU_SEPARATOR ("/Xtns/---"),

  /*  <Toolbox>/Help  */

  MENU_BRANCH (N_("/_Help")),

  { { N_("/Help/_Help..."), "F1",
      help_help_cmd_callback, 0,
      "<StockItem>", GTK_STOCK_HELP },
    NULL,
    GIMP_HELP_HELP, NULL },
  { { N_("/Help/_Context Help..."), "<shift>F1",
      help_context_help_cmd_callback, 0,
      "<StockItem>", GTK_STOCK_HELP },
    NULL,
    GIMP_HELP_HELP_CONTEXT, NULL },
  { { N_("/Help/_Tip of the Day..."), NULL,
      dialogs_create_toplevel_cmd_callback, 0,
      "<StockItem>", GIMP_STOCK_INFO },
    "gimp-tips-dialog",
    GIMP_HELP_TIPS_DIALOG, NULL },
  { { N_("/Help/_About..."), NULL,
      dialogs_create_toplevel_cmd_callback, 0,
      "<StockItem>", GIMP_STOCK_WILBER },
    "gimp-about-dialog",
    GIMP_HELP_ABOUT_DIALOG, NULL }
};

gint n_toolbox_menu_entries = G_N_ELEMENTS (toolbox_menu_entries);


void
toolbox_menu_setup (GimpItemFactory *factory)
{
  static gchar *reorder_subsubmenus[] = { "/Xtns" };

  GtkWidget    *menu_item;
  GtkWidget    *menu;
  GList        *list;
  gint          i, pos;

  menus_last_opened_add (factory, factory->gimp);

  plug_in_menus_create (factory, factory->gimp->plug_in_proc_defs);

  /*  Move all menu items under "<Toolbox>/Xtns" which are not submenus or
   *  separators to the top of the menu
   */
  pos = 1;
  menu_item = gtk_item_factory_get_widget (GTK_ITEM_FACTORY (factory),
                                           "/Xtns/Module Manager...");
  if (menu_item && menu_item->parent && GTK_IS_MENU (menu_item->parent))
    {
      menu = menu_item->parent;

      for (list = g_list_nth (GTK_MENU_SHELL (menu)->children, pos);
           list;
	   list = g_list_next (list))
	{
	  menu_item = GTK_WIDGET (list->data);

	  if (! GTK_MENU_ITEM (menu_item)->submenu &&
	      GTK_IS_LABEL (GTK_BIN (menu_item)->child))
	    {
	      gtk_menu_reorder_child (GTK_MENU (menu_item->parent),
				      menu_item, pos);
	      list = g_list_nth (GTK_MENU_SHELL (menu)->children, pos);
	      pos++;
	    }
	}
    }

  for (i = 0; i < G_N_ELEMENTS (reorder_subsubmenus); i++)
    {
      menu = gtk_item_factory_get_widget (GTK_ITEM_FACTORY (factory),
					  reorder_subsubmenus[i]);

      if (menu && GTK_IS_MENU (menu))
	{
	  for (list = GTK_MENU_SHELL (menu)->children;
               list;
	       list = g_list_next (list))
	    {
	      GtkMenuItem *menu_item;

	      menu_item = GTK_MENU_ITEM (list->data);

	      if (menu_item->submenu)
		menus_filters_subdirs_to_top (GTK_MENU (menu_item->submenu));
	    }
	}
    }
}
