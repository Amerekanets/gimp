/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * Measure tool
 * Copyright (C) 1999 Sven Neumann <sven@gimp.org>
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

#include "core/gimptoolinfo.h"

#include "widgets/gimppropwidgets.h"

#include "gimpmeasureoptions.h"
#include "gimptooloptions-gui.h"

#include "gimp-intl.h"


enum
{
  PROP_0,
  PROP_USE_INFO_WINDOW
};


static void   gimp_measure_options_init       (GimpMeasureOptions      *options);
static void   gimp_measure_options_class_init (GimpMeasureOptionsClass *options_class);

static void   gimp_measure_options_set_property (GObject         *object,
                                                 guint            property_id,
                                                 const GValue    *value,
                                                 GParamSpec      *pspec);
static void   gimp_measure_options_get_property (GObject         *object,
                                                 guint            property_id,
                                                 GValue          *value,
                                                 GParamSpec      *pspec);


static GimpToolOptionsClass *parent_class = NULL;


GType
gimp_measure_options_get_type (void)
{
  static GType type = 0;

  if (! type)
    {
      static const GTypeInfo info =
      {
        sizeof (GimpMeasureOptionsClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_measure_options_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data     */
	sizeof (GimpMeasureOptions),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_measure_options_init,
      };

      type = g_type_register_static (GIMP_TYPE_TOOL_OPTIONS,
                                     "GimpMeasureOptions",
                                     &info, 0);
    }

  return type;
}

static void 
gimp_measure_options_class_init (GimpMeasureOptionsClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->set_property = gimp_measure_options_set_property;
  object_class->get_property = gimp_measure_options_get_property;

  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_USE_INFO_WINDOW,
                                    "use-info-window", NULL,
                                    FALSE,
                                    0);
}

static void
gimp_measure_options_init (GimpMeasureOptions *options)
{
}

static void
gimp_measure_options_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GimpMeasureOptions *options;

  options = GIMP_MEASURE_OPTIONS (object);

  switch (property_id)
    {
    case PROP_USE_INFO_WINDOW:
      options->use_info_window = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_measure_options_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GimpMeasureOptions *options;

  options = GIMP_MEASURE_OPTIONS (object);

  switch (property_id)
    {
    case PROP_USE_INFO_WINDOW:
      g_value_set_boolean (value, options->use_info_window);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

GtkWidget *
gimp_measure_options_gui (GimpToolOptions *tool_options)
{
  GObject   *config;
  GtkWidget *vbox;
  GtkWidget *button;

  config = G_OBJECT (tool_options);

  vbox = gimp_tool_options_gui (tool_options);

  /*  the use_info_window toggle button  */
  button = gimp_prop_check_button_new (config, "use-info-window",
                                       _("Use info window"));
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  return vbox;
}
