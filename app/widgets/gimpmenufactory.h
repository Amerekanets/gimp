/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpmenufactory.h
 * Copyright (C) 2003 Michael Natterer <mitch@gimp.org>
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

#ifndef __GIMP_MENU_FACTORY_H__
#define __GIMP_MENU_FACTORY_H__


#include "core/gimpobject.h"


typedef struct _GimpMenuFactoryEntry GimpMenuFactoryEntry;

struct _GimpMenuFactoryEntry
{
  gchar                     *identifier;
  gchar                     *title;
  gchar                     *help_id;
  GimpItemFactorySetupFunc   setup_func;
  GimpItemFactoryUpdateFunc  update_func;
  gboolean                   update_on_popup;
  guint                      n_entries;
  GimpItemFactoryEntry      *entries;
};


#define GIMP_TYPE_MENU_FACTORY            (gimp_menu_factory_get_type ())
#define GIMP_MENU_FACTORY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_MENU_FACTORY, GimpMenuFactory))
#define GIMP_MENU_FACTORY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_MENU_FACTORY, GimpMenuFactoryClass))
#define GIMP_IS_MENU_FACTORY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_MENU_FACTORY))
#define GIMP_IS_MENU_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_MENU_FACTORY))
#define GIMP_MENU_FACTORY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_MENU_FACTORY, GimpMenuFactoryClass))


typedef struct _GimpMenuFactoryClass  GimpMenuFactoryClass;

struct _GimpMenuFactory
{
  GimpObject  parent_instance;

  Gimp       *gimp;
  GList      *registered_menus;
};

struct _GimpMenuFactoryClass
{
  GimpObjectClass  parent_class;
};


GType             gimp_menu_factory_get_type      (void) G_GNUC_CONST;

GimpMenuFactory * gimp_menu_factory_new           (Gimp            *gimp);

void              gimp_menu_factory_menu_register (GimpMenuFactory *factory,
                                                   const gchar     *identifier,
                                                   const gchar     *title,
                                                   const gchar     *help_id,
                                                   GimpItemFactorySetupFunc   setup_func,
                                                   GimpItemFactoryUpdateFunc  update_func,
                                                   gboolean                   update_on_popup,
                                                   guint                 n_entries,
                                                   GimpItemFactoryEntry *entries);

GimpItemFactory * gimp_menu_factory_menu_new      (GimpMenuFactory *factory,
                                                   const gchar     *identifier,
                                                   GType            container_type,
                                                   gpointer         callback_data,
                                                   gboolean         create_tearoff);


#endif  /*  __GIMP_MENU_FACTORY_H__  */
