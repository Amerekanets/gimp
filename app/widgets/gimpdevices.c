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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "config.h"

#include <gtk/gtk.h>

#include "libgimpcolor/gimpcolor.h"
#include "libgimpbase/gimpbase.h"

#include "widgets-types.h"

#include "core/gimp.h"
#include "core/gimpdatafactory.h"
#include "core/gimpgradient.h"
#include "core/gimplist.h"
#include "core/gimppattern.h"
#include "core/gimptoolinfo.h"

#include "config/gimpconfig.h"

#include "gimpdeviceinfo.h"
#include "gimpdevices.h"


#define GIMP_DEVICE_MANAGER_DATA_KEY "gimp-device-manager"


typedef struct _GimpDeviceManager GimpDeviceManager;

struct _GimpDeviceManager
{
  GimpContainer          *device_info_list;
  GdkDevice              *current_device;
  GimpDeviceChangeNotify  change_notify;
};


/*  local function prototypes  */

static GimpDeviceManager * gimp_device_manager_get (Gimp *gimp);


/*  public functions  */

void 
gimp_devices_init (Gimp                   *gimp,
                   GimpDeviceChangeNotify  change_notify)
{
  GimpDeviceManager *manager;
  GdkDevice         *device;
  GimpDeviceInfo    *device_info;
  GList             *list;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  g_return_if_fail (gimp_device_manager_get (gimp) == NULL);

  manager = g_new0 (GimpDeviceManager, 1);

  g_object_set_data_full (G_OBJECT (gimp),
                          GIMP_DEVICE_MANAGER_DATA_KEY, manager,
                          (GDestroyNotify) g_free);

  manager->device_info_list = gimp_list_new (GIMP_TYPE_DEVICE_INFO,
                                             GIMP_CONTAINER_POLICY_STRONG);
  manager->current_device   = gdk_device_get_core_pointer ();
  manager->change_notify    = change_notify;

  /*  create device info structures for present devices */
  for (list = gdk_devices_list (); list; list = g_list_next (list))
    {
      device = (GdkDevice *) list->data;

      device_info = gimp_device_info_new (gimp, device->name);
      gimp_container_add (manager->device_info_list,
                          GIMP_OBJECT (device_info));
      g_object_unref (device_info);

      gimp_device_info_set_from_device (device_info, device);
    }
}

void
gimp_devices_exit (Gimp *gimp)
{
  GimpDeviceManager *manager;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  manager = gimp_device_manager_get (gimp);

  g_return_if_fail (manager != NULL);

  g_object_unref (manager->device_info_list);
  g_object_set_data (G_OBJECT (gimp), GIMP_DEVICE_MANAGER_DATA_KEY, NULL);
}

void
gimp_devices_restore (Gimp *gimp)
{
  GimpDeviceManager *manager;
  GimpDeviceInfo    *device_info;
  GimpContext       *user_context;
  gchar             *filename;
  GError            *error = NULL;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  manager = gimp_device_manager_get (gimp);

  g_return_if_fail (manager != NULL);

  filename = gimp_personal_rc_file ("devicerc");

  if (! gimp_config_deserialize (G_OBJECT (manager->device_info_list),
                                 filename,
                                 gimp,
                                 &error))
    {
      if (error->code != GIMP_CONFIG_ERROR_OPEN_ENOENT)
        g_message (error->message);

      g_error_free (error);
      /* don't bail out here */
    }

  g_free (filename);

  gimp_list_reverse (GIMP_LIST (manager->device_info_list));

  device_info = gimp_device_info_get_by_device (manager->current_device);

  g_return_if_fail (GIMP_IS_DEVICE_INFO (device_info));

  user_context = gimp_get_user_context (gimp);

  gimp_context_copy_properties (GIMP_CONTEXT (device_info), user_context,
				GIMP_DEVICE_INFO_CONTEXT_MASK);
  gimp_context_set_parent (GIMP_CONTEXT (device_info), user_context);
}

void
gimp_devices_save (Gimp *gimp)
{
  GimpDeviceManager *manager;
  gchar             *filename;
  GError            *error = NULL;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  manager = gimp_device_manager_get (gimp);

  g_return_if_fail (manager != NULL);

  filename = gimp_personal_rc_file ("devicerc");

  if (! gimp_config_serialize (G_OBJECT (manager->device_info_list),
                               filename,
                               "# GIMP devicerc\n",
                               "# end devicerc",
                               NULL,
                               &error))
    {
      g_message (error->message);
      g_error_free (error);
    }

  g_free (filename);
}

GdkDevice *
gimp_devices_get_current (Gimp *gimp)
{
  GimpDeviceManager *manager;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);

  manager = gimp_device_manager_get (gimp);

  g_return_val_if_fail (manager != NULL, NULL);

  return manager->current_device;
}

void
gimp_devices_select_device (Gimp      *gimp,
                            GdkDevice *new_device)
{
  GimpDeviceManager *manager;
  GimpDeviceInfo    *current_device_info;
  GimpDeviceInfo    *new_device_info;
  GimpContext       *user_context;

  g_return_if_fail (GIMP_IS_GIMP (gimp));
  g_return_if_fail (GDK_IS_DEVICE (new_device));

  manager = gimp_device_manager_get (gimp);

  g_return_if_fail (manager != NULL);

  current_device_info = gimp_device_info_get_by_device (manager->current_device);
  new_device_info     = gimp_device_info_get_by_device (new_device);

  g_return_if_fail (GIMP_IS_DEVICE_INFO (current_device_info));
  g_return_if_fail (GIMP_IS_DEVICE_INFO (new_device_info));

  gimp_context_unset_parent (GIMP_CONTEXT (current_device_info));

  manager->current_device = new_device;

  user_context = gimp_get_user_context (gimp);

  gimp_context_copy_properties (GIMP_CONTEXT (new_device_info), user_context,
				GIMP_DEVICE_INFO_CONTEXT_MASK);
  gimp_context_set_parent (GIMP_CONTEXT (new_device_info), user_context);

  if (manager->change_notify)
    manager->change_notify (gimp);
}

gboolean
gimp_devices_check_change (Gimp     *gimp,
                           GdkEvent *event)
{
  GimpDeviceManager *manager;
  GdkDevice         *device;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  manager = gimp_device_manager_get (gimp);

  g_return_val_if_fail (manager != NULL, FALSE);

  switch (event->type)
    {
    case GDK_MOTION_NOTIFY:
      device = ((GdkEventMotion *) event)->device;
      break;

    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      device = ((GdkEventButton *) event)->device;
      break;

    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
      device = ((GdkEventProximity *) event)->device;
      break;

    case GDK_SCROLL:
      device = ((GdkEventScroll *) event)->device;
      break;

    default:
      device = manager->current_device;
      break;
    }

  if (device != manager->current_device)
    {
      gimp_devices_select_device (gimp, device);
      return TRUE;
    }

  return FALSE;
}


/*  private functions  */

static GimpDeviceManager *
gimp_device_manager_get (Gimp *gimp)
{
  return g_object_get_data (G_OBJECT (gimp), GIMP_DEVICE_MANAGER_DATA_KEY);
}
