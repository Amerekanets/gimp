/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimppluginaction.c
 * Copyright (C) 2004 Michael Natterer <mitch@gimp.org>
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

#include "widgets-types.h"

#include "core/gimpmarshal.h"

#include "gimppluginaction.h"


enum
{
  SELECTED,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_PROC_DEF
};


static void   gimp_plug_in_action_init       (GimpPlugInAction      *action);
static void   gimp_plug_in_action_class_init (GimpPlugInActionClass *klass);

static void   gimp_plug_in_action_set_property (GObject      *object,
                                                guint         prop_id,
                                                const GValue *value,
                                                GParamSpec   *pspec);
static void   gimp_plug_in_action_get_property (GObject      *object,
                                                guint         prop_id,
                                                GValue       *value,
                                                GParamSpec   *pspec);

static void   gimp_plug_in_action_activate     (GtkAction    *action);


static GtkActionClass *parent_class                = NULL;
static guint           action_signals[LAST_SIGNAL] = { 0 };


GType
gimp_plug_in_action_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo type_info =
      {
        sizeof (GimpPlugInActionClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gimp_plug_in_action_class_init,
        (GClassFinalizeFunc) NULL,
        NULL,
        sizeof (GimpPlugInAction),
        0, /* n_preallocs */
        (GInstanceInitFunc) gimp_plug_in_action_init,
      };

      type = g_type_register_static (GTK_TYPE_ACTION,
                                     "GimpPlugInAction",
                                     &type_info, 0);
    }

  return type;
}

static void
gimp_plug_in_action_class_init (GimpPlugInActionClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkActionClass *action_class = GTK_ACTION_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->set_property = gimp_plug_in_action_set_property;
  object_class->get_property = gimp_plug_in_action_get_property;

  action_class->activate = gimp_plug_in_action_activate;

  g_object_class_install_property (object_class, PROP_PROC_DEF,
                                   g_param_spec_pointer ("proc-def",
                                                         NULL, NULL,
                                                         G_PARAM_READWRITE));

  action_signals[SELECTED] =
    g_signal_new ("selected",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GimpPlugInActionClass, selected),
		  NULL, NULL,
                  gimp_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1,
                  G_TYPE_POINTER);
}

static void
gimp_plug_in_action_init (GimpPlugInAction *action)
{
  action->proc_def = NULL;
}

static void
gimp_plug_in_action_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GimpPlugInAction *action = GIMP_PLUG_IN_ACTION (object);

  switch (prop_id)
    {
    case PROP_PROC_DEF:
      g_value_set_pointer (value, action->proc_def);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gimp_plug_in_action_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GimpPlugInAction *action = GIMP_PLUG_IN_ACTION (object);

  switch (prop_id)
    {
    case PROP_PROC_DEF:
      action->proc_def = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

GimpPlugInAction *
gimp_plug_in_action_new (const gchar   *name,
                         const gchar   *label,
                         const gchar   *tooltip,
                         const gchar   *stock_id,
                         PlugInProcDef *proc_def)
{
  return g_object_new (GIMP_TYPE_PLUG_IN_ACTION,
                       "name",     name,
                       "label",    label,
                       "tooltip",  tooltip,
                       "stock_id", stock_id,
                       "proc-def", proc_def,
                       NULL);
}

static void
gimp_plug_in_action_activate (GtkAction *action)
{
  GimpPlugInAction *plug_in_action = GIMP_PLUG_IN_ACTION (action);

  gimp_plug_in_action_selected (plug_in_action);
}

void
gimp_plug_in_action_selected (GimpPlugInAction *action)
{
  g_return_if_fail (GIMP_IS_PLUG_IN_ACTION (action));

  g_signal_emit (action, action_signals[SELECTED], 0, action->proc_def);
}
