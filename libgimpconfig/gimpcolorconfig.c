/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * GimpColorConfig class
 * Copyright (C) 2004  Stefan Döhla <stefan@doehla.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <glib-object.h>

#include "gimpconfigtypes.h"

#include "gimpcolorconfig-enums.h"

#include "gimpcolorconfig.h"
#include "gimpconfig-iface.h"
#include "gimpconfig-params.h"
#include "gimpconfig-path.h"

#include "libgimp/libgimp-intl.h"


#define DEFAULT_PROFILE_PATH  "/usr/share/color/icc"


#define COLOR_MANAGEMENT_MODE_BLURB \
  N_("Mode of operation for color management.")
#define PROFILE_PATH_BLURB \
  N_("Default folder where ICC profiles are located.")
#define DISPLAY_PROFILE_BLURB \
  N_("Sets the display color profile.")
#define RGB_PROFILE_BLURB \
  N_("Sets default RGB workspace color profile.")
#define CMYK_PROFILE_BLURB \
  N_("Sets the CMYK color profile used to convert between RGB and CMYK.")
#define PRINTER_PROFILE_BLURB \
  N_("Sets the color profile used when printing.")
#define DISPLAY_RENDERING_INTENT_BLURB \
  N_("Sets how colors are mapped for your display.")
#define SIMULATION_RENDERING_INTENT_BLURB \
  N_("Sets how colors from workspace to simulation device are converted.")
#define OPEN_BEHAVIOUR_NO_PROFILE_BLURB \
  N_("Defines what will be done if no color profile is available.")
#define OPEN_BEHAVIOUR_RGB_PROFILE_BLURB \
  N_("Defines what will be done if an RGB profile is available.")
#define OPEN_BEHAVIOUR_CMYK_PROFILE_BLURB \
  N_("Defines what will be done if a CMYK profile is available.")


enum
{
  PROP_0,
  PROP_MODE,
  PROP_PROFILE_PATH,
  PROP_RGB_PROFILE,
  PROP_CMYK_PROFILE,
  PROP_DISPLAY_PROFILE,
  PROP_PRINTER_PROFILE,
  PROP_DISPLAY_RENDERING_INTENT,
  PROP_SIMULATION_RENDERING_INTENT,
  PROP_OPEN_BEHAVIOUR_NO_PROFILE,
  PROP_OPEN_BEHAVIOUR_RGB_PROFILE,
  PROP_OPEN_BEHAVIOUR_CMYK_PROFILE
};


static void  gimp_color_config_class_init   (GimpColorConfigClass *klass);
static void  gimp_color_config_finalize     (GObject              *object);
static void  gimp_color_config_set_property (GObject              *object,
                                             guint                 property_id,
                                             const GValue         *value,
                                             GParamSpec           *pspec);
static void  gimp_color_config_get_property (GObject              *object,
                                             guint                 property_id,
                                             GValue               *value,
                                             GParamSpec           *pspec);


static GObjectClass *parent_class = NULL;


GType
gimp_color_config_get_type (void)
{
  static GType config_type = 0;

  if (! config_type)
    {
      static const GTypeInfo config_info =
      {
        sizeof (GimpColorConfigClass),
        NULL,           /* base_init      */
        NULL,           /* base_finalize  */
        (GClassInitFunc) gimp_color_config_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data     */
        sizeof (GimpColorConfig),
        0,              /* n_preallocs    */
        NULL            /* instance_init  */
      };

      static const GInterfaceInfo config_iface_info =
      {
        NULL,           /* iface_init     */
        NULL,           /* iface_finalize */
        NULL            /* iface_data     */
      };

      config_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GimpColorConfig",
                                            &config_info, 0);

      g_type_add_interface_static (config_type,
                                   GIMP_TYPE_CONFIG, &config_iface_info);
    }

  return config_type;
}

static void
gimp_color_config_class_init (GimpColorConfigClass *klass)
{
  GObjectClass *object_class;

  parent_class = g_type_class_peek_parent (klass);

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize     = gimp_color_config_finalize;
  object_class->set_property = gimp_color_config_set_property;
  object_class->get_property = gimp_color_config_get_property;

  GIMP_CONFIG_INSTALL_PROP_ENUM (object_class, PROP_MODE,
                                 "mode", COLOR_MANAGEMENT_MODE_BLURB,
                                 GIMP_TYPE_COLOR_MANAGEMENT_MODE,
                                 GIMP_COLOR_MANAGEMENT_DISPLAY,
                                 0);
  GIMP_CONFIG_INSTALL_PROP_PATH (object_class, PROP_PROFILE_PATH,
                                 "profile-path", PROFILE_PATH_BLURB,
                                 GIMP_CONFIG_PATH_DIR, DEFAULT_PROFILE_PATH,
                                 0);
  GIMP_CONFIG_INSTALL_PROP_PATH (object_class, PROP_RGB_PROFILE,
                                 "rgb-profile", RGB_PROFILE_BLURB,
                                 GIMP_CONFIG_PATH_FILE, NULL,
                                 0);
  GIMP_CONFIG_INSTALL_PROP_PATH (object_class, PROP_CMYK_PROFILE,
                                 "cmyk-profile", CMYK_PROFILE_BLURB,
                                 GIMP_CONFIG_PATH_FILE, NULL,
                                 0);
  GIMP_CONFIG_INSTALL_PROP_PATH (object_class, PROP_DISPLAY_PROFILE,
                                 "display-profile", DISPLAY_PROFILE_BLURB,
                                 GIMP_CONFIG_PATH_FILE, NULL,
                                 0);
  GIMP_CONFIG_INSTALL_PROP_PATH (object_class, PROP_PRINTER_PROFILE,
                                 "printer-profile", PRINTER_PROFILE_BLURB,
                                 GIMP_CONFIG_PATH_FILE, NULL,
                                 0);
  GIMP_CONFIG_INSTALL_PROP_ENUM (object_class, PROP_DISPLAY_RENDERING_INTENT,
                                 "display-rendering-intent",
                                 DISPLAY_RENDERING_INTENT_BLURB,
                                 GIMP_TYPE_COLOR_RENDERING_INTENT,
                                 GIMP_COLOR_RENDERING_INTENT_PERCEPTUAL,
                                 0);
  GIMP_CONFIG_INSTALL_PROP_ENUM (object_class, PROP_SIMULATION_RENDERING_INTENT,
                                 "simulation-rendering-intent",
                                 SIMULATION_RENDERING_INTENT_BLURB,
                                 GIMP_TYPE_COLOR_RENDERING_INTENT,
                                 GIMP_COLOR_RENDERING_INTENT_PERCEPTUAL,
                                 0);
  GIMP_CONFIG_INSTALL_PROP_ENUM (object_class, PROP_OPEN_BEHAVIOUR_NO_PROFILE,
                                 "open-behaviour-no-profile",
                                 OPEN_BEHAVIOUR_NO_PROFILE_BLURB,
                                 GIMP_TYPE_COLOR_FILE_OPEN_BEHAVIOUR,
                                 GIMP_COLOR_FILE_OPEN_ASK,
                                 0);
  GIMP_CONFIG_INSTALL_PROP_ENUM (object_class, PROP_OPEN_BEHAVIOUR_RGB_PROFILE,
                                 "open-behaviour-rgb-profile",
                                 OPEN_BEHAVIOUR_RGB_PROFILE_BLURB,
                                 GIMP_TYPE_COLOR_FILE_OPEN_BEHAVIOUR,
                                 GIMP_COLOR_FILE_OPEN_ASK,
                                 0);
  GIMP_CONFIG_INSTALL_PROP_ENUM (object_class, PROP_OPEN_BEHAVIOUR_CMYK_PROFILE,
                                 "open-behaviour-cmyk-profile",
                                 OPEN_BEHAVIOUR_CMYK_PROFILE_BLURB,
                                 GIMP_TYPE_COLOR_FILE_OPEN_BEHAVIOUR,
                                 GIMP_COLOR_FILE_OPEN_ASK,
                                 0);
}

static void
gimp_color_config_finalize (GObject *object)
{
  GimpColorConfig *color_config = GIMP_COLOR_CONFIG (object);

  g_free (color_config->profile_path);
  g_free (color_config->rgb_profile);
  g_free (color_config->cmyk_profile);
  g_free (color_config->display_profile);
  g_free (color_config->printer_profile);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_color_config_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GimpColorConfig *color_config = GIMP_COLOR_CONFIG (object);

  switch (property_id)
    {
    case PROP_MODE:
      color_config->mode = g_value_get_enum (value);
      break;
    case PROP_PROFILE_PATH:
      g_free (color_config->profile_path);
      color_config->profile_path = g_value_dup_string (value);
      break;
    case PROP_RGB_PROFILE:
      g_free (color_config->rgb_profile);
      color_config->rgb_profile = g_value_dup_string (value);
      break;
    case PROP_CMYK_PROFILE:
      g_free (color_config->cmyk_profile);
      color_config->cmyk_profile = g_value_dup_string (value);
      break;
    case PROP_DISPLAY_PROFILE:
      g_free (color_config->display_profile);
      color_config->display_profile = g_value_dup_string (value);
      break;
    case PROP_PRINTER_PROFILE:
      g_free (color_config->printer_profile);
      color_config->printer_profile = g_value_dup_string (value);
      break;
    case PROP_DISPLAY_RENDERING_INTENT:
      color_config->display_rendering_intent = g_value_get_enum (value);
      break;
    case PROP_SIMULATION_RENDERING_INTENT:
      color_config->simulation_rendering_intent = g_value_get_enum (value);
      break;
    case PROP_OPEN_BEHAVIOUR_NO_PROFILE:
      color_config->open_behaviour_no_profile = g_value_get_enum (value);
      break;
    case PROP_OPEN_BEHAVIOUR_RGB_PROFILE:
      color_config->open_behaviour_rgb_profile = g_value_get_enum (value);
      break;
    case PROP_OPEN_BEHAVIOUR_CMYK_PROFILE:
      color_config->open_behaviour_cmyk_profile = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_color_config_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GimpColorConfig *color_config = GIMP_COLOR_CONFIG (object);

  switch (property_id)
    {
    case PROP_MODE:
      g_value_set_enum (value, color_config->mode);
      break;
    case PROP_PROFILE_PATH:
      g_value_set_string (value, color_config->profile_path);
      break;
    case PROP_RGB_PROFILE:
      g_value_set_string (value, color_config->rgb_profile);
      break;
    case PROP_CMYK_PROFILE:
      g_value_set_string (value, color_config->cmyk_profile);
      break;
    case PROP_DISPLAY_PROFILE:
      g_value_set_string (value, color_config->display_profile);
      break;
    case PROP_PRINTER_PROFILE:
      g_value_set_string (value, color_config->printer_profile);
      break;
    case PROP_DISPLAY_RENDERING_INTENT:
      g_value_set_enum (value, color_config->display_rendering_intent);
      break;
    case PROP_SIMULATION_RENDERING_INTENT:
      g_value_set_enum (value, color_config->simulation_rendering_intent);
      break;
    case PROP_OPEN_BEHAVIOUR_NO_PROFILE:
      g_value_set_enum (value, color_config->open_behaviour_no_profile);
      break;
    case PROP_OPEN_BEHAVIOUR_RGB_PROFILE:
      g_value_set_enum (value, color_config->open_behaviour_rgb_profile);
      break;
    case PROP_OPEN_BEHAVIOUR_CMYK_PROFILE:
      g_value_set_enum (value, color_config->open_behaviour_cmyk_profile);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}
