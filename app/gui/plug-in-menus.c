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

#include "string.h"

#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"

#include "gui-types.h"

#include "plug-in/plug-in.h"
#include "plug-in/plug-ins.h"
#include "plug-in/plug-in-def.h"
#include "plug-in/plug-in-proc.h"

#include "widgets/gimpitemfactory.h"

#include "plug-in-commands.h"
#include "plug-in-menus.h"

#include "libgimpbase/gimpenv.h"
#include "libgimp/gimpintl.h"


typedef struct _PlugInMenuEntry PlugInMenuEntry;

struct _PlugInMenuEntry
{
  PlugInProcDef *proc_def;
  const gchar   *domain;
  const gchar   *help_path;
};


/*  local function prototypes  */

static gboolean plug_in_menu_tree_traverse_func (gpointer         foo,
                                                 PlugInMenuEntry *menu_entry,
                                                 GimpItemFactory *image_factory);
static gchar  * plug_in_escape_uline            (const gchar     *menu_path);


/*  public functions  */

void
plug_in_menus_init (GSList      *plug_in_defs,
                    const gchar *std_plugins_domain)
{
  PlugInDef *plug_in_def;
  GSList    *domains = NULL;
  GSList    *tmp;

  g_return_if_fail (plug_in_defs != NULL);
  g_return_if_fail (std_plugins_domain != NULL);

  domains = g_slist_append (domains, (gpointer) std_plugins_domain);

  bindtextdomain (std_plugins_domain, gimp_locale_directory ());
  bind_textdomain_codeset (std_plugins_domain, "UTF-8");

  for (tmp = plug_in_defs; tmp; tmp = g_slist_next (tmp))
    {
      const gchar *locale_domain;
      const gchar *locale_path;
      GSList      *list;

      plug_in_def = tmp->data;

      if (! plug_in_def->proc_defs)
        continue;

      locale_domain = plug_ins_locale_domain (plug_in_def->prog,
                                              &locale_path);

      for (list = domains; list; list = list->next)
        {
          if (! strcmp (locale_domain, (gchar *) list->data))
            break;
        }

      if (! list)
        {
          domains = g_slist_append (domains, (gpointer) locale_domain);

          bindtextdomain (locale_domain, locale_path);
          bind_textdomain_codeset (locale_domain, "UTF-8");
        }
    }

  g_slist_free (domains);
}

void
plug_in_make_menu (GimpItemFactory *image_factory,
                   GSList          *proc_defs)
{
  PlugInProcDef   *proc_def;
  GSList          *procs;
  GTree           *menu_entries;

  g_return_if_fail (image_factory == NULL ||
                    GIMP_IS_ITEM_FACTORY (image_factory));
  g_return_if_fail (proc_defs != NULL);

  menu_entries = g_tree_new_full ((GCompareDataFunc) g_utf8_collate, NULL,
                                  NULL, g_free);

  for (procs = proc_defs; procs; procs = procs->next)
    {
      proc_def = procs->data;

      if (proc_def->prog         &&
          proc_def->menu_path    &&
          ! proc_def->extensions &&
          ! proc_def->prefixes   &&
          ! proc_def->magics)
        {
          PlugInMenuEntry *menu_entry;
          const gchar     *locale_domain;

          locale_domain = plug_ins_locale_domain (proc_def->prog, NULL);

          menu_entry = g_new0 (PlugInMenuEntry, 1);

          menu_entry->proc_def  = proc_def;
          menu_entry->domain    = locale_domain;
          menu_entry->help_path = plug_ins_help_path (proc_def->prog);

          g_tree_insert (menu_entries, 
                         dgettext (locale_domain, proc_def->menu_path),
                         menu_entry);
        }
    }

  g_tree_foreach (menu_entries, 
                  (GTraverseFunc) plug_in_menu_tree_traverse_func,
                  image_factory);
  g_tree_destroy (menu_entries);
}

void
plug_in_make_menu_entry (GimpItemFactory *image_factory,
                         PlugInProcDef   *proc_def,
                         const gchar     *domain,
                         const gchar     *help_path)
{
  GimpItemFactoryEntry  entry;
  gchar                *menu_path;
  gchar                *help_page;
  gchar                *basename;
  gchar                *lowercase_page;

  g_return_if_fail (image_factory == NULL ||
                    GIMP_IS_ITEM_FACTORY (image_factory));

  basename = g_path_get_basename (proc_def->prog);

  if (help_path)
    {
      help_page = g_strconcat (help_path,
			       "@",   /* HACK: locale subdir */
			       basename,
			       ".html",
			       NULL);
    }
  else
    {
      help_page = g_strconcat ("filters/",  /* _not_ G_DIR_SEPARATOR_S */
			       basename,
			       ".html",
			       NULL);
    }

  g_free (basename);

  lowercase_page = g_ascii_strdown (help_page, -1);

  g_free (help_page);

  menu_path = plug_in_escape_uline (strstr (proc_def->menu_path, "/"));

  entry.entry.path            = menu_path;
  entry.entry.accelerator     = proc_def->accelerator;
  entry.entry.callback        = plug_in_run_cmd_callback;
  entry.entry.callback_action = 0;
  entry.entry.item_type       = NULL;
  entry.quark_string          = NULL;
  entry.help_page             = lowercase_page;
  entry.description           = NULL;

  if (image_factory)
    {
      if (! strncmp (proc_def->menu_path, "<Image>", 7))
        {
          gimp_item_factory_create_item (image_factory,
                                         &entry,
                                         domain,
                                         &proc_def->db_info, 2,
                                         TRUE, FALSE);
        }
    }
  else
    {
      GList *list;

      for (list = gimp_item_factories_from_path (proc_def->menu_path);
           list;
           list = g_list_next (list))
        {
          GimpItemFactory *item_factory = list->data;

          gimp_item_factory_create_item (item_factory,
                                         &entry,
                                         domain,
                                         &proc_def->db_info, 2,
                                         TRUE, FALSE);
        }
    }

  g_free (menu_path);
  g_free (lowercase_page);
}

void
plug_in_delete_menu_entry (const gchar *menu_path)
{
  GList *list;

  g_return_if_fail (menu_path != NULL);

  for (list = gimp_item_factories_from_path (menu_path);
       list;
       list = g_list_next (list))
    {
      GtkItemFactory *item_factory = list->data;

      gtk_item_factory_delete_item (GTK_ITEM_FACTORY (item_factory), menu_path);
    }
}

void
plug_in_set_menu_sensitivity (GimpItemFactory *image_factory,
                              GimpImageType    type)
{
  PlugInProcDef *proc_def;
  GSList        *tmp;
  gboolean       sensitive = FALSE;

  g_return_if_fail (image_factory == NULL ||
                    GIMP_IS_ITEM_FACTORY (image_factory));

  for (tmp = proc_defs; tmp; tmp = g_slist_next (tmp))
    {
      proc_def = tmp->data;

      if (proc_def->image_types_val && proc_def->menu_path)
        {
          switch (type)
            {
            case GIMP_RGB_IMAGE:
              sensitive = proc_def->image_types_val & PLUG_IN_RGB_IMAGE;
              break;
            case GIMP_RGBA_IMAGE:
              sensitive = proc_def->image_types_val & PLUG_IN_RGBA_IMAGE;
              break;
            case GIMP_GRAY_IMAGE:
              sensitive = proc_def->image_types_val & PLUG_IN_GRAY_IMAGE;
              break;
            case GIMP_GRAYA_IMAGE:
              sensitive = proc_def->image_types_val & PLUG_IN_GRAYA_IMAGE;
              break;
            case GIMP_INDEXED_IMAGE:
              sensitive = proc_def->image_types_val & PLUG_IN_INDEXED_IMAGE;
              break;
            case GIMP_INDEXEDA_IMAGE:
              sensitive = proc_def->image_types_val & PLUG_IN_INDEXEDA_IMAGE;
              break;
            default:
              sensitive = FALSE;
              break;
            }

          if (image_factory && ! strncmp (proc_def->menu_path, "<Image>", 7))
            gimp_item_factory_set_sensitive (GTK_ITEM_FACTORY (image_factory),
                                             proc_def->menu_path,
                                             sensitive);
          else
            gimp_item_factories_set_sensitive (proc_def->menu_path,
                                               proc_def->menu_path,
                                               sensitive);

          if (last_plug_in && (last_plug_in == &proc_def->db_info))
            {
              gchar *basename;
              gchar *ellipses;
              gchar *repeat;
              gchar *reshow;

              basename = g_path_get_basename (proc_def->menu_path);

              ellipses = strstr (basename, "...");

              if (ellipses && ellipses == (basename + strlen (basename) - 3))
                *ellipses = '\0';

              repeat = g_strdup_printf (_("Repeat \"%s\""), basename);
              reshow = g_strdup_printf (_("Re-show \"%s\""), basename);

              g_free (basename);

              gimp_item_factories_set_label ("<Image>",
                                             "/Filters/Repeat Last", repeat);
              gimp_item_factories_set_label ("<Image>",
                                             "/Filters/Re-Show Last", reshow);

              g_free (repeat);
              g_free (reshow);

	      gimp_item_factories_set_sensitive ("<Image>",
                                                 "/Filters/Repeat Last",
                                                 sensitive);
	      gimp_item_factories_set_sensitive ("<Image>",
                                                 "/Filters/Re-Show Last",
                                                 sensitive);
	    }
	}
    }

  if (! last_plug_in)
    {
      gimp_item_factories_set_label ("<Image>",
                                     "/Filters/Repeat Last",
                                     _("Repeat Last"));
      gimp_item_factories_set_label ("<Image>",
                                     "/Filters/Re-Show Last",
                                     _("Re-Show Last"));

      gimp_item_factories_set_sensitive ("<Image>",
                                         "/Filters/Repeat Last", FALSE);
      gimp_item_factories_set_sensitive ("<Image>",
                                         "/Filters/Re-Show Last", FALSE);
    }
}


/*  private functions  */

static gboolean
plug_in_menu_tree_traverse_func (gpointer         foo,
                                 PlugInMenuEntry *menu_entry,
                                 GimpItemFactory *image_factory)
{
  plug_in_make_menu_entry (image_factory,
                           menu_entry->proc_def,
                           menu_entry->domain,
                           menu_entry->help_path);

  return FALSE;
}

static gchar *
plug_in_escape_uline (const gchar *menu_path)
{
  gchar *uline;
  gchar *escaped;
  gchar *tmp;

  escaped = g_strdup (menu_path);

  uline = strchr (escaped, '_');

  while (uline)
    {
      tmp = escaped;
      escaped = g_new (gchar, strlen (tmp) + 2);

      if (uline > tmp)
        strncpy (escaped, tmp, (uline - tmp));

      escaped[uline - tmp] = '_';
      strcpy (&escaped[uline - tmp + 1], uline);

      uline = strchr (escaped + (uline - tmp) + 2, '_');

      g_free (tmp);
    }

  return escaped;
}
