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

#include "core/gimppalette.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"

#include "widgets/gimpcontainerlistview.h"
#include "widgets/gimpdatafactoryview.h"
#include "widgets/gimpitemfactory.h"
#include "widgets/gimplistitem.h"
#include "widgets/gimppreview.h"
#include "widgets/gimpwidgets-utils.h"

#include "palette-import-dialog.h"
#include "palettes-commands.h"

#ifdef __GNUC__
#warning GTK_DISABLE_DEPRECATED
#endif
#undef GTK_DISABLE_DEPRECATED
#include <gtk/gtklist.h>

#include "libgimp/gimpintl.h"


/*  local function prototypes  */

static void   palettes_merge_palettes_query    (GimpContainerEditor *editor);
static void   palettes_merge_palettes_callback (GtkWidget           *widget,
						gchar               *palette_name,
						gpointer             data);


/*  public functionss */

void
palettes_import_palette_cmd_callback (GtkWidget *widget,
				      gpointer   data)
{
  GimpContainerEditor *editor;

  editor = (GimpContainerEditor *)  gimp_widget_get_callback_context (widget);

  if (! editor)
    return;

  palette_import_dialog_show (editor->view->context->gimp);
}

void
palettes_merge_palettes_cmd_callback (GtkWidget *widget,
				      gpointer   data)
{
  GimpContainerEditor *editor;

  editor = (GimpContainerEditor *) gimp_widget_get_callback_context (widget);

  if (! editor)
    return;

  palettes_merge_palettes_query (editor);
}

void
palettes_menu_update (GtkItemFactory *factory,
                      gpointer        data)
{
  GimpContainerEditor *editor;
  GimpPalette         *palette;
  gboolean             internal = FALSE;

  editor = GIMP_CONTAINER_EDITOR (data);

  palette = gimp_context_get_palette (editor->view->context);

  if (palette)
    internal = GIMP_DATA (palette)->internal;

#define SET_SENSITIVE(menu,condition) \
        gimp_item_factory_set_sensitive (factory, menu, (condition) != 0)

  SET_SENSITIVE ("/Duplicate Palette",
		 palette && GIMP_DATA_GET_CLASS (palette)->duplicate);
  SET_SENSITIVE ("/Edit Palette...",
		 palette && GIMP_DATA_FACTORY_VIEW (editor)->data_edit_func);
  SET_SENSITIVE ("/Delete Palette...",
		 palette && ! internal);
  SET_SENSITIVE ("/Merge Palettes...",
		 FALSE); /* FIXME palette && GIMP_IS_CONTAINER_LIST_VIEW (editor->view)); */

#undef SET_SENSITIVE
}


/*  private functions  */

static void
palettes_merge_palettes_query (GimpContainerEditor *editor)
{
  GtkWidget *qbox;

  qbox = gimp_query_string_box (_("Merge Palette"),
				gimp_standard_help_func,
				"dialogs/palette_editor/merge_palette.html",
				_("Enter a name for merged palette"),
				NULL,
				G_OBJECT (editor), "destroy",
				palettes_merge_palettes_callback,
				editor);
  gtk_widget_show (qbox);
}

static void
palettes_merge_palettes_callback (GtkWidget *widget,
				  gchar     *palette_name,
				  gpointer   data)
{
  GimpContainerEditor *editor;
  GimpPalette         *palette;
  GimpPalette         *new_palette;
  GimpPaletteEntry    *entry;
  GList               *sel_list;

  editor = (GimpContainerEditor *) data;

  sel_list = GTK_LIST (GIMP_CONTAINER_LIST_VIEW (editor->view)->gtk_list)->selection;

  if (! sel_list)
    {
      g_message ("Can't merge palettes because there are no palettes selected.");
      return;
    }

  new_palette = GIMP_PALETTE (gimp_palette_new (palette_name));

  while (sel_list)
    {
      GimpListItem *list_item;
      GList        *cols;

      list_item = GIMP_LIST_ITEM (sel_list->data);

      palette = (GimpPalette *) GIMP_PREVIEW (list_item->preview)->viewable;

      if (palette)
	{
	  for (cols = palette->colors; cols; cols = g_list_next (cols))
	    {
	      entry = (GimpPaletteEntry *) cols->data;

	      gimp_palette_add_entry (new_palette,
				      entry->name,
				      &entry->color);
	    }
	}

      sel_list = sel_list->next;
    }

  gimp_container_add (editor->view->container,
		      GIMP_OBJECT (new_palette));
}
