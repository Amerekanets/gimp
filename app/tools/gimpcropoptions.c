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

#include "tools-types.h"

#include "config/gimpconfig-params.h"

#include "widgets/gimppropwidgets.h"
#include "widgets/gimpwidgets-utils.h"

#include "gimpcropoptions.h"

#include "libgimp/gimpintl.h"


enum
{
  PROP_0,
  PROP_LAYER_ONLY,
  PROP_ALLOW_ENLARGE,
  PROP_CROP_TYPE
};


static void   gimp_crop_options_init       (GimpCropOptions      *options);
static void   gimp_crop_options_class_init (GimpCropOptionsClass *options_class);

static void   gimp_crop_options_set_property (GObject      *object,
                                              guint         property_id,
                                              const GValue *value,
                                              GParamSpec   *pspec);
static void   gimp_crop_options_get_property (GObject      *object,
                                              guint         property_id,
                                              GValue       *value,
                                              GParamSpec   *pspec);


static GimpToolOptionsClass *parent_class = NULL;


GType
gimp_crop_options_get_type (void)
{
  static GType type = 0;

  if (! type)
    {
      static const GTypeInfo info =
      {
        sizeof (GimpCropOptionsClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_crop_options_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data     */
	sizeof (GimpCropOptions),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_crop_options_init,
      };

      type = g_type_register_static (GIMP_TYPE_TOOL_OPTIONS,
                                     "GimpCropOptions",
                                     &info, 0);
    }

  return type;
}

static void 
gimp_crop_options_class_init (GimpCropOptionsClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->set_property = gimp_crop_options_set_property;
  object_class->get_property = gimp_crop_options_get_property;

  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_LAYER_ONLY,
                                    "layer-only", NULL,
                                    FALSE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_ALLOW_ENLARGE,
                                    "allow-enlarge", NULL,
                                    FALSE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_ENUM (object_class, PROP_CROP_TYPE,
                                 "crop-type", NULL,
                                 GIMP_TYPE_CROP_TYPE,
                                 GIMP_CROP,
                                 0);
}

static void
gimp_crop_options_init (GimpCropOptions *options)
{
}

static void
gimp_crop_options_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GimpCropOptions *options;

  options = GIMP_CROP_OPTIONS (object);

  switch (property_id)
    {
    case PROP_LAYER_ONLY:
      options->layer_only = g_value_get_boolean (value);
      break;
    case PROP_ALLOW_ENLARGE:
      options->allow_enlarge = g_value_get_boolean (value);
      break;
    case PROP_CROP_TYPE:
      options->crop_type = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_crop_options_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GimpCropOptions *options;

  options = GIMP_CROP_OPTIONS (object);

  switch (property_id)
    {
    case PROP_LAYER_ONLY:
      g_value_set_boolean (value, options->layer_only);
      break;
    case PROP_ALLOW_ENLARGE:
      g_value_set_boolean (value, options->allow_enlarge);
      break;
    case PROP_CROP_TYPE:
      g_value_set_enum (value, options->crop_type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

void
gimp_crop_options_gui (GimpToolOptions *tool_options)
{
  GObject   *config;
  GtkWidget *vbox;
  GtkWidget *frame;
  GtkWidget *button;
  gchar     *str;

  config = G_OBJECT (tool_options);

  vbox = tool_options->main_vbox;

  /*  tool toggle  */
  str = g_strdup_printf (_("Tool Toggle  %s"), gimp_get_mod_name_control ());

  frame = gimp_prop_enum_radio_frame_new (config, "crop-type",
                                          str, 0, 0);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  g_free (str);

  /*  layer toggle  */
  button = gimp_prop_check_button_new (config, "layer-only",
                                       _("Current Layer only"));
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  /*  enlarge toggle  */
  str = g_strdup_printf (_("Allow Enlarging  %s"), gimp_get_mod_name_alt ());

  button = gimp_prop_check_button_new (config, "allow-enlarge", str);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  g_free (str);
}
