/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * ParamSpecs for config objects
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <glib-object.h>

#include "libgimpbase/gimpbase.h"

#include "gimpconfig-types.h"


static GimpRGB * color_copy        (const GimpRGB *color);
static void      color_free        (GimpRGB       *color);

static void      memsize_to_string (const GValue  *src_value,
                                    GValue        *dest_value);
static void      string_to_memsize (const GValue  *src_value,
                                    GValue        *dest_value);

static void      unit_to_string    (const GValue  *src_value,
                                    GValue        *dest_value);
static void      string_to_unit    (const GValue  *src_value,
                                    GValue        *dest_value);


GType
gimp_color_get_type (void)
{
  static GType color_type = 0;

  if (!color_type)
    color_type = g_boxed_type_register_static ("GimpColor",
                                               (GBoxedCopyFunc) color_copy,
                                               (GBoxedFreeFunc) color_free);

  return color_type;
}

GType
gimp_memsize_get_type (void)
{
  static GType memsize_type = 0;

  if (!memsize_type)
    {
      static const GTypeInfo type_info = { 0, };

      memsize_type = g_type_register_static (G_TYPE_ULONG, "GimpMemsize", 
                                             &type_info, 0);

      g_value_register_transform_func (memsize_type, G_TYPE_STRING,
                                       memsize_to_string);
      g_value_register_transform_func (G_TYPE_STRING, memsize_type,
                                       string_to_memsize);
    }
  
  return memsize_type;
}

gboolean
gimp_memsize_set_from_string (GValue      *value,
                              const gchar *string)
{
  gchar  *end;
  gulong  size;

  g_return_val_if_fail (GIMP_VALUE_HOLDS_MEMSIZE (value), FALSE);
  g_return_val_if_fail (string != NULL, FALSE);

  size = strtoul (string, &end, 0);

  if (size == ULONG_MAX && errno == ERANGE)
    return FALSE;

  if (end && *end)
    {
      guint shift;
      
      switch (g_ascii_tolower (*end))
        {
        case 'b':
          shift = 0;
          break;
        case 'k':
          shift = 10;
          break;
        case 'm':
          shift = 20;
          break;
        case 'g':
          shift = 30;
          break;
        default:
          return FALSE;
        }

      /* protect against overflow */
      if (shift)
        {
          gulong limit = G_MAXULONG >> (shift);
      
          if (size != (size & limit))
            return FALSE;

          size <<= shift;
        }
    }
  
  g_value_set_ulong (value, size);
  
  return TRUE;
}

GType
gimp_path_get_type (void)
{
  static GType path_type = 0;

  if (!path_type)
    {
      static const GTypeInfo type_info = { 0, };

      path_type = g_type_register_static (G_TYPE_STRING, "GimpPath", 
                                          &type_info, 0);
    }
  
  return path_type;
}

GType
gimp_unit_get_type (void)
{
  static GType unit_type = 0;

  if (!unit_type)
    {
      static const GTypeInfo type_info = { 0, };

      unit_type = g_type_register_static (G_TYPE_INT, "GimpUnit", 
                                          &type_info, 0);

      g_value_register_transform_func (unit_type, G_TYPE_STRING,
                                       unit_to_string);
      g_value_register_transform_func (G_TYPE_STRING, unit_type,
                                       string_to_unit);
    }
  
  return unit_type;
}


static GimpRGB *
color_copy (const GimpRGB *color)
{
  return (GimpRGB *) g_memdup (color, sizeof (GimpRGB));
}

static void
color_free (GimpRGB *color)
{
  g_free (color);
}


static void
memsize_to_string (const GValue *src_value,
                   GValue       *dest_value)
{
  gulong  size;
  gchar  *str;

  size = g_value_get_ulong (src_value);

  if (size > (1 << 30) && size % (1 << 30) == 0)
    str = g_strdup_printf ("%luG", size >> 30);
  else if (size > (1 << 20) && size % (1 << 20) == 0)
    str = g_strdup_printf ("%luM", size >> 20);
  else if (size > (1 << 10) && size % (1 << 10) == 0)
    str = g_strdup_printf ("%luk", size >> 10);
  else
    str = g_strdup_printf ("%lu", size);

  g_value_set_string_take_ownership (dest_value, str);
}

static void
string_to_memsize (const GValue *src_value,
                   GValue       *dest_value)
{
  const gchar *str = g_value_get_string (src_value);

  if (!str || !gimp_memsize_set_from_string (dest_value, str))
    g_warning ("Can't convert string to GimpMemsize.");
}


static void
unit_to_string (const GValue *src_value,
                GValue       *dest_value)
{
  GimpUnit unit = (GimpUnit) g_value_get_int (src_value);

  g_value_set_string (dest_value, gimp_unit_get_identifier (unit));
}

static void
string_to_unit (const GValue *src_value,
                GValue       *dest_value)
{
  const gchar *str;
  gint         num_units;
  gint         i;

  str = g_value_get_string (src_value);

  if (!str || !*str)
    goto error;

  num_units = gimp_unit_get_number_of_units ();

  for (i = GIMP_UNIT_PIXEL; i < num_units; i++)
    if (strcmp (str, gimp_unit_get_identifier (i)) == 0)
      break;

  if (i == num_units)
    goto error;
    
  g_value_set_int (dest_value, i);
  return;

 error:
  g_warning ("Can't convert string to GimpUnit.");
}
