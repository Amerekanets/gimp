/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * Object properties serialization routines
 * Copyright (C) 2001-2002  Sven Neumann <sven@gimp.org>
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

#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpcolor/gimpcolor.h"

#include "gimpconfigwriter.h"
#include "gimpconfig-iface.h"
#include "gimpconfig-params.h"
#include "gimpconfig-serialize.h"
#include "gimpconfig-utils.h"


/**
 * gimp_config_serialize_properties:
 * @config: a #GimpConfig.
 * @writer: a #GimpConfigWriter.
 *
 * This function writes all object properties to the @writer.
 *
 * Returns: %TRUE if serialization succeeded, %FALSE otherwise
 **/
gboolean
gimp_config_serialize_properties (GimpConfig          *config,
                                  GimpConfigWriter *writer)
{
  GObjectClass  *klass;
  GParamSpec   **property_specs;
  guint          n_property_specs;
  guint          i;

  g_return_val_if_fail (G_IS_OBJECT (config), FALSE);

  klass = G_OBJECT_GET_CLASS (config);

  property_specs = g_object_class_list_properties (klass, &n_property_specs);

  if (! property_specs)
    return TRUE;

  for (i = 0; i < n_property_specs; i++)
    {
      GParamSpec *prop_spec = property_specs[i];

      if (! (prop_spec->flags & GIMP_PARAM_SERIALIZE))
        continue;

      if (! gimp_config_serialize_property (config, prop_spec, writer))
        return FALSE;
    }

  g_free (property_specs);

  return TRUE;
}

/**
 * gimp_config_serialize_changed_properties:
 * @config: a #GimpConfig.
 * @writer: a #GimpConfigWriter.
 *
 * This function writes all object properties that have been changed from
 * their default values to the @writer.
 *
 * Returns: %TRUE if serialization succeeded, %FALSE otherwise
 **/
gboolean
gimp_config_serialize_changed_properties (GimpConfig       *config,
                                          GimpConfigWriter *writer)
{
  GObjectClass  *klass;
  GParamSpec   **property_specs;
  guint          n_property_specs;
  guint          i;
  GValue         value = { 0, };

  g_return_val_if_fail (G_IS_OBJECT (config), FALSE);

  klass = G_OBJECT_GET_CLASS (config);

  property_specs = g_object_class_list_properties (klass, &n_property_specs);

  if (! property_specs)
    return TRUE;

  for (i = 0; i < n_property_specs; i++)
    {
      GParamSpec *prop_spec = property_specs[i];

      if (! (prop_spec->flags & GIMP_PARAM_SERIALIZE))
        continue;

      g_value_init (&value, prop_spec->value_type);
      g_object_get_property (G_OBJECT (config), prop_spec->name, &value);

      if (! g_param_value_defaults (prop_spec, &value))
        {
          if (! gimp_config_serialize_property (config, prop_spec, writer))
            return FALSE;
        }

      g_value_unset (&value);
    }

  g_free (property_specs);

  return TRUE;
}

gboolean
gimp_config_serialize_property (GimpConfig       *config,
                                GParamSpec       *param_spec,
                                GimpConfigWriter *writer)
{
  GTypeClass          *owner_class;
  GimpConfigInterface *config_iface;
  GimpConfigInterface *parent_iface = NULL;
  GValue               value   = { 0, };
  gboolean             success = FALSE;

  if (! (param_spec->flags & GIMP_PARAM_SERIALIZE))
    return FALSE;

  if (param_spec->flags & GIMP_PARAM_IGNORE)
    return TRUE;

  g_value_init (&value, param_spec->value_type);
  g_object_get_property (G_OBJECT (config), param_spec->name, &value);

  if (param_spec->flags & GIMP_PARAM_DEFAULTS &&
      g_param_value_defaults (param_spec, &value))
    {
      g_value_unset (&value);
      return TRUE;
    }

  owner_class = g_type_class_peek (param_spec->owner_type);

  config_iface = g_type_interface_peek (owner_class, GIMP_TYPE_CONFIG);

  /*  We must call serialize_property() *only* if the *exact* class
   *  which implements it is param_spec->owner_type's class.
   *
   *  Therefore, we ask param_spec->owner_type's immediate parent class
   *  for it's GimpConfigInterface and check if we get a different pointer.
   *
   *  (if the pointers are the same, param_spec->owner_type's
   *   GimpConfigInterface is inherited from one of it's parent classes
   *   and thus not able to handle param_spec->owner_type's properties).
   */
  if (config_iface)
    {
      GTypeClass *owner_parent_class;

      owner_parent_class = g_type_class_peek_parent (owner_class),

      parent_iface = g_type_interface_peek (owner_parent_class,
                                            GIMP_TYPE_CONFIG);
    }

  if (config_iface                     &&
      config_iface != parent_iface     && /* see comment above */
      config_iface->serialize_property &&
      config_iface->serialize_property (config,
                                        param_spec->param_id,
                                        (const GValue *) &value,
                                        param_spec,
                                        writer))
    {
      success = TRUE;
    }

  /*  If there is no serialize_property() method *or* if it returned
   *  FALSE, continue with the default implementation
   */

  if (! success)
    {
      if (G_VALUE_HOLDS_OBJECT (&value))
        {
          GimpConfigInterface *config_iface = NULL;
          GimpConfig          *prop_object;

          prop_object = g_value_get_object (&value);

          if (prop_object)
            config_iface = GIMP_CONFIG_GET_INTERFACE (prop_object);
          else
            success = TRUE;

          if (config_iface)
            {
              gimp_config_writer_open (writer, param_spec->name);

              /*  if the object property is not GIMP_PARAM_AGGREGATE,
               *  deserializing will need to know the exact type
               *  in order to create the object
               */
              if (! (param_spec->flags & GIMP_PARAM_AGGREGATE))
                {
                  GType object_type = G_TYPE_FROM_INSTANCE (prop_object);

                  gimp_config_writer_string (writer, g_type_name (object_type));
                }

              success = config_iface->serialize (prop_object, writer, NULL);

              if (success)
                gimp_config_writer_close (writer);
              else
                gimp_config_writer_revert (writer);
            }
        }
      else if (! G_VALUE_HOLDS_OBJECT (&value))
        {
	  GString *str = g_string_new (NULL);

          success = gimp_config_serialize_value (&value, str, TRUE);

	  if (success)
            {
              gimp_config_writer_open (writer, param_spec->name);
              gimp_config_writer_print (writer, str->str, str->len);
              gimp_config_writer_close (writer);
            }

	  g_string_free (str, TRUE);
	}

      if (! success)
        {
          /* don't warn for empty string properties */
          if (G_VALUE_HOLDS_STRING (&value))
            {
              success = TRUE;
            }
          else
            {
              g_warning ("couldn't serialize property %s::%s of type %s",
                         g_type_name (G_TYPE_FROM_INSTANCE (config)),
                         param_spec->name,
                         g_type_name (param_spec->value_type));
            }
        }
    }

  g_value_unset (&value);

  return success;
}

/**
 * gimp_config_serialize_value:
 * @value: a #GValue.
 * @str: a #Gstring.
 * @escaped: whether to escape string values.
 *
 * This utility function appends a string representation of #GValue to @str.
 *
 * Return value: %TRUE if serialization succeeded, %FALSE otherwise.
 **/
gboolean
gimp_config_serialize_value (const GValue *value,
                             GString      *str,
                             gboolean      escaped)
{
  if (G_VALUE_HOLDS_BOOLEAN (value))
    {
      gboolean bool;

      bool = g_value_get_boolean (value);
      g_string_append (str, bool ? "yes" : "no");
      return TRUE;
    }

  if (G_VALUE_HOLDS_ENUM (value))
    {
      GEnumClass *enum_class = g_type_class_peek (G_VALUE_TYPE (value));
      GEnumValue *enum_value = g_enum_get_value (enum_class,
                                                 g_value_get_enum (value));

      if (enum_value && enum_value->value_nick)
        {
          g_string_append (str, enum_value->value_nick);
          return TRUE;
        }
      else
        {
          g_warning ("Couldn't get nick for enum_value of %s",
                     G_ENUM_CLASS_TYPE_NAME (enum_class));
          return FALSE;
        }
    }

  if (G_VALUE_HOLDS_STRING (value))
    {
      const gchar *cstr = g_value_get_string (value);

      if (!cstr)
        return FALSE;

      if (escaped)
        gimp_config_string_append_escaped (str, cstr);
      else
        g_string_append (str, cstr);

      return TRUE;
    }

  if (G_VALUE_HOLDS_DOUBLE (value) || G_VALUE_HOLDS_FLOAT (value))
    {
      gdouble v_double;
      gchar   buf[G_ASCII_DTOSTR_BUF_SIZE];

      if (G_VALUE_HOLDS_DOUBLE (value))
        v_double = g_value_get_double (value);
      else
        v_double = (gdouble) g_value_get_float (value);

      g_ascii_formatd (buf, sizeof (buf), "%f", v_double);
      g_string_append (str, buf);
      return TRUE;
    }

  if (GIMP_VALUE_HOLDS_RGB (value))
    {
      GimpRGB *rgb;
      gchar    buf[4][G_ASCII_DTOSTR_BUF_SIZE];

      rgb = g_value_get_boxed (value);

      g_ascii_formatd (buf[0], G_ASCII_DTOSTR_BUF_SIZE, "%f", rgb->r);
      g_ascii_formatd (buf[1], G_ASCII_DTOSTR_BUF_SIZE, "%f", rgb->g);
      g_ascii_formatd (buf[2], G_ASCII_DTOSTR_BUF_SIZE, "%f", rgb->b);
      g_ascii_formatd (buf[3], G_ASCII_DTOSTR_BUF_SIZE, "%f", rgb->a);

      g_string_append_printf (str, "(color-rgba %s %s %s %s)",
                              buf[0], buf[1], buf[2], buf[3]);
      return TRUE;
    }

  if (GIMP_VALUE_HOLDS_MATRIX2 (value))
    {
      GimpMatrix2 *trafo;
      gchar        buf[4][G_ASCII_DTOSTR_BUF_SIZE];
      gint         i, j, k;

      trafo = g_value_get_boxed (value);

      for (i = 0, k = 0; i < 2; i++)
        for (j = 0; j < 2; j++, k++)
          g_ascii_formatd (buf[k],
                           G_ASCII_DTOSTR_BUF_SIZE, "%f", trafo->coeff[i][j]);

      g_string_append_printf (str, "(matrix %s %s %s %s)",
			      buf[0], buf[1], buf[2], buf[3]);
      return TRUE;
    }

  if (G_VALUE_TYPE (value) == G_TYPE_VALUE_ARRAY)
    {
      GValueArray *array;

      array = g_value_get_boxed (value);

      if (array)
        {
          gint i;

          g_string_append_printf (str, "%d", array->n_values);

          for (i = 0; i < array->n_values; i++)
            {
              g_string_append (str, " ");

              if (! gimp_config_serialize_value (g_value_array_get_nth (array,
									i),
                                                 str, TRUE))
                return FALSE;
            }
        }
      else
        {
          g_string_append (str, "NULL");
        }

      return TRUE;
    }

  if (g_value_type_transformable (G_VALUE_TYPE (value), G_TYPE_STRING))
    {
      GValue  tmp_value = { 0, };

      g_value_init (&tmp_value, G_TYPE_STRING);
      g_value_transform (value, &tmp_value);

      g_string_append (str, g_value_get_string (&tmp_value));

      g_value_unset (&tmp_value);
      return TRUE;
    }

  return FALSE;
}
