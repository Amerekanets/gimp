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

#include <string.h>

#include <glib-object.h>

#include "core-types.h"

#include "base/temp-buf.h"

#include "gimp.h"
#include "gimpcontainer.h"
#include "gimpcontext.h"
#include "gimplist.h"
#include "gimppaintinfo.h"
#include "gimptoolinfo.h"
#include "gimptooloptions.h"


static void    gimp_tool_info_class_init      (GimpToolInfoClass *klass);
static void    gimp_tool_info_init            (GimpToolInfo      *tool_info);

static void    gimp_tool_info_finalize        (GObject           *object);
static gchar * gimp_tool_info_get_description (GimpViewable      *viewable,
                                               gchar            **tooltip);


static GimpDataClass *parent_class = NULL;


GType
gimp_tool_info_get_type (void)
{
  static GType tool_info_type = 0;

  if (! tool_info_type)
    {
      static const GTypeInfo tool_info_info =
      {
        sizeof (GimpToolInfoClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_tool_info_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data     */
	sizeof (GimpToolInfo),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_tool_info_init,
      };

      tool_info_type = g_type_register_static (GIMP_TYPE_DATA,
					       "GimpToolInfo",
					       &tool_info_info, 0);
    }

  return tool_info_type;
}

static void
gimp_tool_info_class_init (GimpToolInfoClass *klass)
{
  GObjectClass      *object_class;
  GimpViewableClass *viewable_class;

  object_class   = G_OBJECT_CLASS (klass);
  viewable_class = GIMP_VIEWABLE_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize          = gimp_tool_info_finalize;

  viewable_class->get_description = gimp_tool_info_get_description;
}

static void
gimp_tool_info_init (GimpToolInfo *tool_info)
{
  tool_info->gimp              = NULL;

  tool_info->tool_type         = G_TYPE_NONE;
  tool_info->tool_options_type = G_TYPE_NONE;
  tool_info->context_props     = 0;

  tool_info->blurb             = NULL;
  tool_info->help              = NULL;

  tool_info->menu_path         = NULL;
  tool_info->menu_accel        = NULL;

  tool_info->help_domain       = NULL;
  tool_info->help_id           = NULL;

  tool_info->in_toolbox        = TRUE;
  tool_info->tool_options      = NULL;
  tool_info->paint_info        = NULL;
}

static void
gimp_tool_info_finalize (GObject *object)
{
  GimpToolInfo *tool_info;

  tool_info = GIMP_TOOL_INFO (object);

  if (tool_info->blurb)
    {
      g_free (tool_info->blurb);
      tool_info->blurb = NULL;
    }
  if (tool_info->help)
    {
      g_free (tool_info->help);
      tool_info->blurb = NULL;
    }

  if (tool_info->menu_path)
    {
      g_free (tool_info->menu_path);
      tool_info->menu_path = NULL;
    }
  if (tool_info->menu_accel)
    {
      g_free (tool_info->menu_accel);
      tool_info->menu_accel = NULL;
    }

  if (tool_info->help_domain)
    {
      g_free (tool_info->help_domain);
      tool_info->help_domain = NULL;
    }
  if (tool_info->help_id)
    {
      g_free (tool_info->help_id);
      tool_info->help_id = NULL;
    }

  if (tool_info->tool_options)
    {
      g_object_unref (tool_info->tool_options);
      tool_info->tool_options = NULL;
    }

  if (tool_info->options_presets)
    {
      g_object_unref (tool_info->options_presets);
      tool_info->options_presets = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gchar *
gimp_tool_info_get_description (GimpViewable  *viewable,
                                gchar        **tooltip)
{
  GimpToolInfo *tool_info;

  tool_info = GIMP_TOOL_INFO (viewable);

  if (tooltip)
    *tooltip = NULL;

  return g_strdup (tool_info->blurb);
}

GimpToolInfo *
gimp_tool_info_new (Gimp                *gimp,
		    GType                tool_type,
                    GType                tool_options_type,
                    GimpContextPropMask  context_props,
		    const gchar         *identifier,
		    const gchar         *blurb,
		    const gchar         *help,
		    const gchar         *menu_path,
		    const gchar         *menu_accel,
		    const gchar         *help_domain,
		    const gchar         *help_id,
                    const gchar         *paint_core_name,
		    const gchar         *stock_id)
{
  GimpPaintInfo *paint_info;
  GimpToolInfo  *tool_info;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);
  g_return_val_if_fail (identifier != NULL, NULL);
  g_return_val_if_fail (blurb != NULL, NULL);
  g_return_val_if_fail (help != NULL, NULL);
  g_return_val_if_fail (menu_path != NULL, NULL);
  g_return_val_if_fail (help_id != NULL, NULL);
  g_return_val_if_fail (paint_core_name != NULL, NULL);
  g_return_val_if_fail (stock_id != NULL, NULL);

  paint_info = (GimpPaintInfo *)
    gimp_container_get_child_by_name (gimp->paint_info_list, paint_core_name);

  g_return_val_if_fail (GIMP_IS_PAINT_INFO (paint_info), NULL);

  tool_info = g_object_new (GIMP_TYPE_TOOL_INFO,
                            "name",     identifier,
                            "stock-id", stock_id,
                            NULL);

  tool_info->gimp              = gimp;
  tool_info->tool_type         = tool_type;
  tool_info->tool_options_type = tool_options_type;
  tool_info->context_props     = context_props;

  tool_info->blurb             = g_strdup (blurb);
  tool_info->help              = g_strdup (help);

  tool_info->menu_path         = g_strdup (menu_path);
  tool_info->menu_accel        = g_strdup (menu_accel);

  tool_info->help_domain       = g_strdup (help_domain);
  tool_info->help_id           = g_strdup (help_id);

  tool_info->paint_info        = paint_info;

  if (tool_info->tool_options_type == paint_info->paint_options_type)
    {
      gimp_viewable_set_stock_id (GIMP_VIEWABLE (paint_info), stock_id);

      tool_info->tool_options = g_object_ref (paint_info->paint_options);
    }
  else
    {
      tool_info->tool_options = g_object_new (tool_info->tool_options_type,
                                              "gimp", gimp,
                                              NULL);
    }

  g_object_set (tool_info->tool_options, "tool-info", tool_info, NULL);

  if (tool_info->tool_options_type != GIMP_TYPE_TOOL_OPTIONS)
    {
      tool_info->options_presets = gimp_list_new (tool_info->tool_options_type,
                                                  GIMP_CONTAINER_POLICY_STRONG);
    }

  return tool_info;
}

void
gimp_tool_info_set_standard (Gimp         *gimp,
			     GimpToolInfo *tool_info)
{
  g_return_if_fail (GIMP_IS_GIMP (gimp));
  g_return_if_fail (! tool_info || GIMP_IS_TOOL_INFO (tool_info));

  if (tool_info != gimp->standard_tool_info)
    {
      if (gimp->standard_tool_info)
        g_object_unref (gimp->standard_tool_info);

      gimp->standard_tool_info = tool_info;

      if (gimp->standard_tool_info)
        g_object_ref (gimp->standard_tool_info);
    }
}

GimpToolInfo *
gimp_tool_info_get_standard (Gimp *gimp)
{
  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);

  return gimp->standard_tool_info;
}
