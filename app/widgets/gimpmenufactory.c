/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpmenufactory.c
 * Copyright (C) 2001 Michael Natterer <mitch@gimp.org>
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

#include "widgets-types.h"

#include "core/gimp.h"

#include "gimpmenufactory.h"
#include "gimpitemfactory.h"


static void   gimp_menu_factory_class_init (GimpMenuFactoryClass *klass);
static void   gimp_menu_factory_init       (GimpMenuFactory      *factory);

static void   gimp_menu_factory_finalize   (GObject              *object);


static GimpObjectClass *parent_class = NULL;


GType
gimp_menu_factory_get_type (void)
{
  static GType factory_type = 0;

  if (! factory_type)
    {
      static const GTypeInfo factory_info =
      {
        sizeof (GimpMenuFactoryClass),
        NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) gimp_menu_factory_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GimpMenuFactory),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gimp_menu_factory_init,
      };

      factory_type = g_type_register_static (GIMP_TYPE_OBJECT,
					     "GimpMenuFactory",
					     &factory_info, 0);
    }

  return factory_type;
}

static void
gimp_menu_factory_class_init (GimpMenuFactoryClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gimp_menu_factory_finalize;
}

static void
gimp_menu_factory_init (GimpMenuFactory *factory)
{
  factory->gimp             = NULL;
  factory->registered_menus = NULL;
}

static void
gimp_menu_factory_finalize (GObject *object)
{
  GimpMenuFactory *factory;
  GList           *list;

  factory = GIMP_MENU_FACTORY (object);

  for (list = factory->registered_menus; list; list = g_list_next (list))
    {
      GimpMenuFactoryEntry *entry;

      entry = (GimpMenuFactoryEntry *) list->data;

      g_free (entry->identifier);
      g_free (entry->title);
      g_free (entry->help_id);
      g_free (entry);
    }

  g_list_free (factory->registered_menus);
  factory->registered_menus = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

GimpMenuFactory *
gimp_menu_factory_new (Gimp *gimp)
{
  GimpMenuFactory *factory;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);

  factory = g_object_new (GIMP_TYPE_MENU_FACTORY, NULL);

  factory->gimp = gimp;

  return factory;
}

void
gimp_menu_factory_menu_register (GimpMenuFactory           *factory,
                                 const gchar               *identifier,
                                 const gchar               *title,
                                 const gchar               *help_id,
                                 GimpItemFactorySetupFunc   setup_func,
                                 GimpItemFactoryUpdateFunc  update_func,
                                 gboolean                   update_on_popup,
                                 guint                      n_entries,
                                 GimpItemFactoryEntry      *entries)
{
  GimpMenuFactoryEntry *entry;

  g_return_if_fail (GIMP_IS_MENU_FACTORY (factory));
  g_return_if_fail (identifier != NULL);
  g_return_if_fail (title != NULL);
  g_return_if_fail (help_id != NULL);
  g_return_if_fail (n_entries > 0);
  g_return_if_fail (entries != NULL);

  entry = g_new0 (GimpMenuFactoryEntry, 1);

  entry->identifier      = g_strdup (identifier);
  entry->title           = g_strdup (title);
  entry->help_id         = g_strdup (help_id);
  entry->setup_func      = setup_func;
  entry->update_func     = update_func;
  entry->update_on_popup = update_on_popup ? TRUE : FALSE;
  entry->n_entries       = n_entries;
  entry->entries         = entries;

  factory->registered_menus = g_list_prepend (factory->registered_menus, entry);
}

GimpItemFactory *
gimp_menu_factory_menu_new (GimpMenuFactory *factory,
                            const gchar     *identifier,
                            GType            container_type,
                            gpointer         callback_data,
                            gboolean         create_tearoff)
{
  GList *list;

  g_return_val_if_fail (GIMP_IS_MENU_FACTORY (factory), NULL);
  g_return_val_if_fail (identifier != NULL, NULL);

  for (list = factory->registered_menus; list; list = g_list_next (list))
    {
      GimpMenuFactoryEntry *entry = list->data;

      if (! strcmp (entry->identifier, identifier))
        {
          GimpItemFactory *item_factory;

          item_factory = gimp_item_factory_new (factory->gimp,
                                                container_type,
                                                entry->identifier,
                                                entry->title,
                                                entry->help_id,
                                                entry->update_func,
                                                entry->update_on_popup,
                                                entry->n_entries,
                                                entry->entries,
                                                callback_data,
                                                create_tearoff);

          if (entry->setup_func)
            entry->setup_func (item_factory);

          return item_factory;
        }
    }

  g_warning ("%s: no entry registered for \"%s\"",
             G_GNUC_FUNCTION, identifier);

  return NULL;
}
