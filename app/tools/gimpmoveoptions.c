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

#include "core/gimptoolinfo.h"

#include "widgets/gimppropwidgets.h"
#include "widgets/gimpwidgets-utils.h"

#include "gimpmoveoptions.h"
#include "gimptooloptions-gui.h"

#include "gimp-intl.h"


enum
{
  PROP_0,
  PROP_MOVE_TYPE,
  PROP_MOVE_CURRENT
};


static void   gimp_move_options_init       (GimpMoveOptions      *options);
static void   gimp_move_options_class_init (GimpMoveOptionsClass *options_class);

static void   gimp_move_options_set_property (GObject         *object,
                                              guint            property_id,
                                              const GValue    *value,
                                              GParamSpec      *pspec);
static void   gimp_move_options_get_property (GObject         *object,
                                              guint            property_id,
                                              GValue          *value,
                                              GParamSpec      *pspec);


static GimpToolOptionsClass *parent_class = NULL;


GType
gimp_move_options_get_type (void)
{
  static GType type = 0;

  if (! type)
    {
      static const GTypeInfo info =
      {
        sizeof (GimpMoveOptionsClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_move_options_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data     */
	sizeof (GimpMoveOptions),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_move_options_init,
      };

      type = g_type_register_static (GIMP_TYPE_TOOL_OPTIONS,
                                     "GimpMoveOptions",
                                     &info, 0);
    }

  return type;
}

static void
gimp_move_options_class_init (GimpMoveOptionsClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->set_property = gimp_move_options_set_property;
  object_class->get_property = gimp_move_options_get_property;

  GIMP_CONFIG_INSTALL_PROP_ENUM (object_class, PROP_MOVE_TYPE,
                                 "move-type", NULL,
                                 GIMP_TYPE_TRANSFORM_TYPE,
                                 GIMP_TRANSFORM_TYPE_LAYER,
                                 0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_MOVE_CURRENT,
                                    "move-current", NULL,
                                    FALSE,
                                    0);
}

static void
gimp_move_options_init (GimpMoveOptions *options)
{
}

static void
gimp_move_options_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GimpMoveOptions *options;

  options = GIMP_MOVE_OPTIONS (object);

  switch (property_id)
    {
    case PROP_MOVE_TYPE:
      options->move_type = g_value_get_enum (value);
      break;
    case PROP_MOVE_CURRENT:
      options->move_current = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_move_options_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GimpMoveOptions *options;

  options = GIMP_MOVE_OPTIONS (object);

  switch (property_id)
    {
    case PROP_MOVE_TYPE:
      g_value_set_enum (value, options->move_type);
      break;
    case PROP_MOVE_CURRENT:
      g_value_set_boolean (value, options->move_current);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

GtkWidget *
gimp_move_options_gui (GimpToolOptions *tool_options)
{
  GObject   *config;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *frame;
  gchar     *str;

  config = G_OBJECT (tool_options);

  vbox = gimp_tool_options_gui (tool_options);

  hbox = gimp_prop_enum_stock_box_new (config, "move-type", "gimp", 0, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new (_("Affect:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_box_reorder_child (GTK_BOX (hbox), label, 0);
  gtk_widget_show (label);

  /*  tool toggle  */
  str = g_strdup_printf (_("Tool Toggle  %s"), gimp_get_mod_name_shift ());

  frame = gimp_prop_boolean_radio_frame_new (config, "move-current",
                                             str,
                                             _("Move Current Layer or Path"),
                                             _("Pick a Layer, Path\n"
                                               "or Guide to Move"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  g_free (str);

  return vbox;
}
