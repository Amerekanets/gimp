/* The GIMP -- an image manipulation program
 * Copyright (C) 1995-1997 Spencer Kimball and Peter Mattis
 *
 * Utitility functions for GimpConfig.
 * Copyright (C) 2001  Sven Neumann <sven@gimp.org>
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

#include <glib-object.h>

#include "libgimpbase/gimpenv.h"

#include "gimpconfig-utils.h"


gboolean
gimp_config_values_equal (const GValue *a,
                          const GValue *b)
{
  g_return_val_if_fail (G_VALUE_TYPE (a) == G_VALUE_TYPE (b), FALSE);

  if (g_value_fits_pointer (a))
    {
      if (a->data[0].v_pointer == b->data[0].v_pointer)
        return TRUE;

      if (G_VALUE_HOLDS_STRING (a))
        {
          const gchar *a_str = g_value_get_string (a);
          const gchar *b_str = g_value_get_string (b);

          if (a_str && b_str)
            return (strcmp (a_str, b_str) == 0);
          else
            return FALSE;
        }
      else
        {
          g_warning ("%s: Can not compare values of type %s.", 
                     G_STRLOC, G_VALUE_TYPE_NAME (a));
          return FALSE;
        }
    }
  else
    {
      return (a->data[0].v_uint64 == b->data[0].v_uint64); 
    }
}

gchar *
gimp_config_build_data_path (const gchar *name)
{
  return g_strconcat (gimp_directory (), G_DIR_SEPARATOR_S, name,
                      G_SEARCHPATH_SEPARATOR_S,
                      gimp_data_directory (), G_DIR_SEPARATOR_S, name,
                      NULL);
}

gchar * 
gimp_config_build_plug_in_path (const gchar *name)
{
  return g_strconcat (gimp_directory (), G_DIR_SEPARATOR_S, name,
                      G_SEARCHPATH_SEPARATOR_S,
                      gimp_plug_in_directory (), G_DIR_SEPARATOR_S, name,
                      NULL);
}
