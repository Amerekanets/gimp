/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * Object properties deserialization routines
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

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib-object.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpcolor/gimpcolor.h"

#include "gimpconfig.h"
#include "gimpconfig-deserialize.h"
#include "gimpconfig-params.h"
#include "gimpconfig-substitute.h"
#include "gimpconfig-types.h"

#include "libgimp/gimpintl.h"


/*  
 *  All functions return G_TOKEN_RIGHT_PAREN on success,
 *  the GTokenType they would have expected but didn't get
 *  or G_TOKEN_NONE if they got the expected token but
 *  couldn't parse it.
 */
  
static GTokenType  gimp_config_deserialize_unknown     (GObject    *object,
                                                        GScanner   *scanner);
static GTokenType  gimp_config_deserialize_property    (GObject    *object,
                                                        GScanner   *scanner);
static GTokenType  gimp_config_deserialize_value       (GValue     *value,
                                                        GObject    *object,
                                                        GParamSpec *prop_spec,
                                                        GScanner   *scanner);
static GTokenType  gimp_config_deserialize_fundamental (GValue     *value,
                                                        GParamSpec *prop_spec,
                                                        GScanner   *scanner);
static GTokenType  gimp_config_deserialize_enum        (GValue     *value,
                                                        GParamSpec *prop_spec,
                                                        GScanner   *scanner);
static GTokenType  gimp_config_deserialize_memsize     (GValue     *value,
                                                        GParamSpec *prop_spec,
                                                        GScanner   *scanner);
static GTokenType  gimp_config_deserialize_path        (GValue     *value,
                                                        GObject    *object,
                                                        GParamSpec *prop_spec,
                                                        GScanner   *scanner);
static GTokenType  gimp_config_deserialize_color       (GValue     *value,
                                                        GParamSpec *prop_spec,
                                                        GScanner   *scanner);
static GTokenType  gimp_config_deserialize_value_array (GValue     *value,
                                                        GObject    *object,
                                                        GParamSpec *prop_spec,
                                                        GScanner   *scanner);
static GTokenType  gimp_config_deserialize_any         (GValue     *value,
                                                        GParamSpec *prop_spec,
                                                        GScanner   *scanner);

static inline gboolean  scanner_string_utf8_valid (GScanner    *scanner, 
                                                   const gchar *token_name);


/**
 * gimp_config_deserialize_properties:
 * @object: a #GObject.
 * @scanner: a #GScanner.
 * @embedded_scope: %TRUE if a trailing ')' should not trigger a parse error.
 * @store_unknown_tokens: %TRUE if you want to store unknown tokens.
 * 
 * This function uses the @scanner to configure the properties of @object.
 *
 * The store_unknown_tokens parameter is a special feature for #GimpRc.
 * If it set to %TRUE, unknown tokens (e.g. tokens that don't refer to
 * a property of @object) with string values are attached to @object as
 * unknown tokens. GimpConfig has a couple of functions to handle the
 * attached key/value pairs.
 * 
 * Return value: 
 **/
gboolean
gimp_config_deserialize_properties (GObject   *object,
                                    GScanner  *scanner,
                                    gint       nest_level,
                                    gboolean   store_unknown_tokens)
{
  GObjectClass  *klass;
  GParamSpec   **property_specs;
  guint          n_property_specs;
  guint          i;
  guint          scope_id;
  guint          old_scope_id;
  GTokenType	 token;
  GTokenType	 next;

  g_return_val_if_fail (G_IS_OBJECT (object), FALSE);

  klass = G_OBJECT_GET_CLASS (object);
  property_specs = g_object_class_list_properties (klass, &n_property_specs);

  if (!property_specs)
    return TRUE;

  scope_id = g_type_qname (G_TYPE_FROM_INSTANCE (object));
  old_scope_id = g_scanner_set_scope (scanner, scope_id);

  for (i = 0; i < n_property_specs; i++)
    {
      GParamSpec *prop_spec = property_specs[i];

      if (prop_spec->flags & GIMP_PARAM_SERIALIZE)
        {
          g_scanner_scope_add_symbol (scanner, scope_id, 
                                      prop_spec->name, prop_spec);
        }
    }

  g_free (property_specs);

  token = G_TOKEN_LEFT_PAREN;

  while (TRUE)
    {
      next = g_scanner_peek_next_token (scanner);

      if (next != token &&
         ! (store_unknown_tokens &&
            token == G_TOKEN_SYMBOL && next == G_TOKEN_IDENTIFIER))
        {
          break;
        }

      token = g_scanner_get_next_token (scanner);
      
      switch (token)
        {
        case G_TOKEN_LEFT_PAREN:
          token = G_TOKEN_SYMBOL;
          break;

        case G_TOKEN_IDENTIFIER:
          token = gimp_config_deserialize_unknown (object, scanner);
          break;

        case G_TOKEN_SYMBOL:
          token = gimp_config_deserialize_property (object, scanner);
          break;

        case G_TOKEN_RIGHT_PAREN:
          token = G_TOKEN_LEFT_PAREN;
          break;

        default: /* do nothing */
          break;
        }
    }

  g_scanner_set_scope (scanner, old_scope_id);

  return gimp_config_deserialize_return (scanner, token,
                                         nest_level, NULL);
}

static GTokenType
gimp_config_deserialize_unknown (GObject  *object,
                                 GScanner *scanner)
{
  gchar *key;

  if (g_scanner_peek_next_token (scanner) != G_TOKEN_STRING)
    return G_TOKEN_STRING;

  key = g_strdup (scanner->value.v_identifier);

  g_scanner_get_next_token (scanner);
  
  if (!scanner_string_utf8_valid (scanner, key))
    {
      g_free (key);
      return G_TOKEN_NONE;
    }

  gimp_config_add_unknown_token (object, key, scanner->value.v_string);
  g_free (key);

  return G_TOKEN_RIGHT_PAREN;
}

static GTokenType
gimp_config_deserialize_property (GObject    *object,
                                  GScanner   *scanner)
{
  GTypeClass          *owner_class;
  GimpConfigInterface *gimp_config_iface;
  GimpConfigInterface *parent_iface;
  GParamSpec          *prop_spec;
  GTokenType           token = G_TOKEN_RIGHT_PAREN;
  GValue               value = { 0, };

  prop_spec = G_PARAM_SPEC (scanner->value.v_symbol);

  g_value_init (&value, prop_spec->value_type);

  owner_class = g_type_class_peek (prop_spec->owner_type);

  gimp_config_iface = g_type_interface_peek (owner_class,
                                             GIMP_TYPE_CONFIG_INTERFACE);

  /*  We must call deserialize_property() *only* if the *exact* class
   *  which implements it is param_spec->owner_type's class.
   *
   *  Therefore, we ask param_spec->owner_type's immediate parent class
   *  for it's GimpConfigInterface and check if we get a different pointer.
   *
   *  (if the pointers are the same, param_spec->owner_type's
   *   GimpConfigInterface is inherited from one of it's parent classes
   *   and thus not able to handle param_spec->owner_type's properties).
   */
  if (gimp_config_iface)
    {
      GTypeClass *owner_parent_class;

      owner_parent_class = g_type_class_peek_parent (owner_class),

      parent_iface = g_type_interface_peek (owner_parent_class,
                                            GIMP_TYPE_CONFIG_INTERFACE);
    }

  if (gimp_config_iface                       &&
      gimp_config_iface != parent_iface       && /* see comment above */
      gimp_config_iface->deserialize_property &&
      gimp_config_iface->deserialize_property (object,
                                               prop_spec->param_id,
                                               &value,
                                               prop_spec,
                                               scanner,
                                               &token))
    {
      /* nop */
    }
  else
    {
      token = gimp_config_deserialize_value (&value, object, prop_spec, scanner);
    }

  if (token == G_TOKEN_RIGHT_PAREN &&
      g_scanner_peek_next_token (scanner) == token)
    {
      g_object_set_property (object, prop_spec->name, &value);
    }
#if CONFIG_DEBUG
  else
    {
      g_warning ("couldn't deserialize property %s::%s of type %s",
                 g_type_name (G_TYPE_FROM_INSTANCE (object)),
                 prop_spec->name, 
                 g_type_name (prop_spec->value_type));
    }
#endif

  g_value_unset (&value);
  
  return token;
}

static GTokenType
gimp_config_deserialize_value (GValue     *value,
                               GObject    *object,
                               GParamSpec *prop_spec,
                               GScanner   *scanner)
{
  if (G_TYPE_FUNDAMENTAL (prop_spec->value_type) == G_TYPE_ENUM)
    {
      return gimp_config_deserialize_enum (value, prop_spec, scanner);
    }
  else if (G_TYPE_IS_FUNDAMENTAL (prop_spec->value_type))
    {
      return gimp_config_deserialize_fundamental (value, prop_spec, scanner);
    }
  else if (prop_spec->value_type == GIMP_TYPE_MEMSIZE)
    {
      return gimp_config_deserialize_memsize (value, prop_spec, scanner);
    }
  else if (prop_spec->value_type == GIMP_TYPE_PATH)
    {
      return  gimp_config_deserialize_path (value,
                                            object, prop_spec, scanner);
    }
  else if (prop_spec->value_type == GIMP_TYPE_COLOR)
    {
      return gimp_config_deserialize_color (value, prop_spec, scanner);
    }
  else if (prop_spec->value_type == G_TYPE_VALUE_ARRAY)
    {
      return gimp_config_deserialize_value_array (value,
                                                  object, prop_spec, scanner);
    }

  /*  This fallback will only work for value_types that
   *  can be transformed from a string value.
   */
  return gimp_config_deserialize_any (value, prop_spec, scanner);
}

static GTokenType
gimp_config_deserialize_fundamental (GValue     *value,
                                     GParamSpec *prop_spec,
                                     GScanner   *scanner)
{
  GTokenType token;

  switch (G_TYPE_FUNDAMENTAL (prop_spec->value_type))
    {
    case G_TYPE_STRING:
      token = G_TOKEN_STRING;
      break;
      
    case G_TYPE_BOOLEAN:
      token = G_TOKEN_IDENTIFIER;
      break;
      
    case G_TYPE_INT:
    case G_TYPE_UINT:
    case G_TYPE_LONG:
    case G_TYPE_ULONG:
      token = G_TOKEN_INT;
      break;
      
    case G_TYPE_FLOAT:
    case G_TYPE_DOUBLE:
      token = G_TOKEN_FLOAT;
      break;
      
    default:
      token = G_TOKEN_NONE;
      g_assert_not_reached ();
      break;
    }

  if (g_scanner_peek_next_token (scanner) != token)
    return token;

  g_scanner_get_next_token (scanner);

  switch (G_TYPE_FUNDAMENTAL (prop_spec->value_type))
    {
    case G_TYPE_STRING:
      if (scanner_string_utf8_valid (scanner, prop_spec->name))
        g_value_set_static_string (value, scanner->value.v_string);
      else
        return G_TOKEN_NONE;
      break;
      
    case G_TYPE_BOOLEAN:
      if (! g_ascii_strcasecmp (scanner->value.v_identifier, "yes") ||
          ! g_ascii_strcasecmp (scanner->value.v_identifier, "true"))
        g_value_set_boolean (value, TRUE);
      else if (! g_ascii_strcasecmp (scanner->value.v_identifier, "no") ||
               ! g_ascii_strcasecmp (scanner->value.v_identifier, "false"))
        g_value_set_boolean (value, FALSE);
      else
        {
          /* don't translate 'yes' and 'no' */
          g_scanner_error 
            (scanner, 
             _("expected 'yes' or 'no' for boolean token %s, got '%s'"), 
             prop_spec->name, scanner->value.v_identifier);
          return G_TOKEN_NONE;
        }
      break;

    case G_TYPE_INT:
      g_value_set_int (value, scanner->value.v_int);
      break;
    case G_TYPE_UINT:
      g_value_set_uint (value, scanner->value.v_int);
      break;
    case G_TYPE_LONG:
      g_value_set_int (value, scanner->value.v_int);
      break;
    case G_TYPE_ULONG:
      g_value_set_uint (value, scanner->value.v_int);
      break;
    case G_TYPE_FLOAT:
      g_value_set_float (value, scanner->value.v_float);
      break;
    case G_TYPE_DOUBLE:
      g_value_set_double (value, scanner->value.v_float);
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  return G_TOKEN_RIGHT_PAREN;
}

static GTokenType
gimp_config_deserialize_enum (GValue     *value,
                              GParamSpec *prop_spec,
                              GScanner   *scanner)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  if (g_scanner_peek_next_token (scanner) != G_TOKEN_IDENTIFIER)
    return G_TOKEN_IDENTIFIER;

  g_scanner_get_next_token (scanner);

  enum_class = g_type_class_peek (G_VALUE_TYPE (value));
  enum_value = g_enum_get_value_by_nick (G_ENUM_CLASS (enum_class), 
                                         scanner->value.v_identifier);
  if (!enum_value)
    enum_value = g_enum_get_value_by_name (G_ENUM_CLASS (enum_class), 
                                           scanner->value.v_identifier);
      
  if (!enum_value)
    {
      g_scanner_error (scanner, 
                       _("invalid value '%s' for token %s"), 
                       scanner->value.v_identifier, prop_spec->name);
      return G_TOKEN_NONE;
    }

  g_value_set_enum (value, enum_value->value);

  return G_TOKEN_RIGHT_PAREN;
}

static GTokenType
gimp_config_deserialize_memsize (GValue     *value,
                                 GParamSpec *prop_spec,
                                 GScanner   *scanner)
{
  gchar *orig_cset_first = scanner->config->cset_identifier_first;
  gchar *orig_cset_nth   = scanner->config->cset_identifier_nth;

  scanner->config->cset_identifier_first = G_CSET_DIGITS;
  scanner->config->cset_identifier_nth   = G_CSET_DIGITS "gGmMkKbB";
  
  if (g_scanner_peek_next_token (scanner) != G_TOKEN_IDENTIFIER)
    return G_TOKEN_IDENTIFIER;

  g_scanner_get_next_token (scanner);

  scanner->config->cset_identifier_first = orig_cset_first;
  scanner->config->cset_identifier_nth   = orig_cset_nth;
  
  if (gimp_memsize_set_from_string (value, scanner->value.v_identifier))
    return G_TOKEN_RIGHT_PAREN;
  else
    return G_TOKEN_NONE;
}

static GTokenType
gimp_config_deserialize_path (GValue     *value,
                              GObject    *object,
                              GParamSpec *prop_spec,
                              GScanner   *scanner)
{
  gchar *path;

  if (g_scanner_peek_next_token (scanner) != G_TOKEN_STRING)
    return G_TOKEN_STRING;

  g_scanner_get_next_token (scanner);

  if (!scanner_string_utf8_valid (scanner, prop_spec->name))
    return G_TOKEN_NONE;

  path = gimp_config_substitute_path (object, scanner->value.v_string, TRUE);

  if (!path)
    return G_TOKEN_NONE;

  g_value_set_string_take_ownership (value, path);

  return G_TOKEN_RIGHT_PAREN;
}

enum
{
  COLOR_RGB  = 1,
  COLOR_RGBA,
  COLOR_HSV,
  COLOR_HSVA
};

static GTokenType
gimp_config_deserialize_color (GValue     *value,
                               GParamSpec *prop_spec,
                               GScanner   *scanner)
{
  guint      scope_id;
  guint      old_scope_id;
  GTokenType token;

  scope_id = g_quark_from_static_string ("gimp_config_deserialize_color");
  old_scope_id = g_scanner_set_scope (scanner, scope_id);

  if (! g_scanner_scope_lookup_symbol (scanner, scope_id, "color-rgb"))
    {
      g_scanner_scope_add_symbol (scanner, scope_id, 
                                  "color-rgb", GINT_TO_POINTER (COLOR_RGB));
      g_scanner_scope_add_symbol (scanner, scope_id, 
                                  "color-rgba", GINT_TO_POINTER (COLOR_RGBA));
      g_scanner_scope_add_symbol (scanner, scope_id, 
                                  "color-hsv", GINT_TO_POINTER (COLOR_HSV));
      g_scanner_scope_add_symbol (scanner, scope_id, 
                                  "color-hsva", GINT_TO_POINTER (COLOR_HSVA));
    }

  token = G_TOKEN_LEFT_PAREN;

  while (g_scanner_peek_next_token (scanner) == token)
    {
      token = g_scanner_get_next_token (scanner);

      switch (token)
        {
        case G_TOKEN_LEFT_PAREN:
          token = G_TOKEN_SYMBOL;
          break;

        case G_TOKEN_SYMBOL:
          {
            gdouble  col[4] = { 0.0, 0.0, 0.0, 1.0 };
            GimpRGB  color;
            gint     n_channels = 4;
            gboolean is_hsv     = FALSE;
            gint     i;

            switch (GPOINTER_TO_INT (scanner->value.v_symbol))
              {
              case COLOR_RGB:
                n_channels = 3;
                /* fallthrough */
              case COLOR_RGBA:
                break;

              case COLOR_HSV:
                n_channels = 3;
                /* fallthrough */
              case COLOR_HSVA:
                is_hsv = TRUE;
                break;
              }

            token = G_TOKEN_FLOAT;

            for (i = 0; i < n_channels; i++)
              {
                if (g_scanner_peek_next_token (scanner) != token)
                  goto finish;

                token = g_scanner_get_next_token (scanner);

                col[i] = scanner->value.v_float;
              }

            if (is_hsv)
              {
                GimpHSV hsv;

                gimp_hsva_set (&hsv, col[0], col[1], col[2], col[3]);
                gimp_hsv_clamp (&hsv);

                gimp_hsv_to_rgb (&hsv, &color);
              }
            else
              {
                gimp_rgba_set (&color, col[0], col[1], col[2], col[3]);
                gimp_rgb_clamp (&color);
              }

            g_value_set_boxed (value, &color);
          }
          token = G_TOKEN_RIGHT_PAREN;
          break;

        case G_TOKEN_RIGHT_PAREN:
          goto finish;

        default: /* do nothing */
          break;
        }
    }

 finish:

  g_scanner_set_scope (scanner, old_scope_id);

  return token;
}

static GTokenType
gimp_config_deserialize_value_array (GValue     *value,
                                     GObject    *object,
                                     GParamSpec *prop_spec,
                                     GScanner   *scanner)
{
  GParamSpecValueArray *array_spec;
  GValueArray          *array;
  GValue                array_value = { 0, };
  gint                  n_values;
  GTokenType            token;
  gint                  i;

  array_spec = G_PARAM_SPEC_VALUE_ARRAY (prop_spec);

  if (g_scanner_peek_next_token (scanner) != G_TOKEN_INT)
    return G_TOKEN_INT;

  g_scanner_get_next_token (scanner);
  n_values = scanner->value.v_int;

  array = g_value_array_new (n_values);

  for (i = 0; i < n_values; i++)
    {
      g_value_init (&array_value, array_spec->element_spec->value_type);

      token = gimp_config_deserialize_value (&array_value,
                                             object,
                                             array_spec->element_spec,
                                             scanner);

      if (token == G_TOKEN_RIGHT_PAREN)
        g_value_array_append (array, &array_value);

      g_value_unset (&array_value);

      if (token != G_TOKEN_RIGHT_PAREN)
        return token;
    }

  g_value_set_boxed_take_ownership (value, array);

  return G_TOKEN_RIGHT_PAREN;
}

static GTokenType
gimp_config_deserialize_any (GValue     *value,
                             GParamSpec *prop_spec,
                             GScanner   *scanner)
{
  GValue src = { 0, };

  if (!g_value_type_transformable (G_TYPE_STRING, prop_spec->value_type))
    {
      g_warning ("%s: %s can not be transformed from a string",
                 G_STRLOC, g_type_name (prop_spec->value_type));
      return G_TOKEN_NONE;
    }

  if (g_scanner_peek_next_token (scanner) != G_TOKEN_IDENTIFIER)
    return G_TOKEN_IDENTIFIER;

  g_scanner_get_next_token (scanner);

  g_value_init (&src, G_TYPE_STRING);
  g_value_set_static_string (&src, scanner->value.v_identifier);
  g_value_transform (&src, value);
  g_value_unset (&src);

  return G_TOKEN_RIGHT_PAREN;
}

static inline gboolean
scanner_string_utf8_valid (GScanner    *scanner, 
                           const gchar *token_name)
{
  if (g_utf8_validate (scanner->value.v_string, -1, NULL))
    return TRUE;

  g_scanner_error (scanner, 
                   _("value for token %s is not a valid UTF-8 string"), 
                   token_name);

  return FALSE;
}
