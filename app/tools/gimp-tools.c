/* The GIMP -- an image manipulation program
 * Copyright (C) 1995-2001 Spencer Kimball, Peter Mattis and others
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

#include "libgimpbase/gimpbase.h"

#include "tools-types.h"

#include "config/gimpconfig.h"

#include "core/gimp.h"
#include "core/gimplist.h"
#include "core/gimptoolinfo.h"
#include "core/gimptooloptions.h"

#include "gimp-tools.h"
#include "gimptooloptions-gui.h"
#include "tool_manager.h"

#include "gimpairbrushtool.h"
#include "gimpblendtool.h"
#include "gimpbrightnesscontrasttool.h"
#include "gimpbucketfilltool.h"
#include "gimpbycolorselecttool.h"
#include "gimpclonetool.h"
#include "gimpcolorbalancetool.h"
#include "gimpcolorizetool.h"
#include "gimpcolorpickertool.h"
#include "gimpconvolvetool.h"
#include "gimpcroptool.h"
#include "gimpcurvestool.h"
#include "gimpdodgeburntool.h"
#include "gimpellipseselecttool.h"
#include "gimperasertool.h"
#include "gimpfliptool.h"
#include "gimpfreeselecttool.h"
#include "gimpfuzzyselecttool.h"
#include "gimphistogramtool.h"
#include "gimphuesaturationtool.h"
#include "gimpinktool.h"
#include "gimpiscissorstool.h"
#include "gimplevelstool.h"
#include "gimpmagnifytool.h"
#include "gimpmeasuretool.h"
#include "gimpmovetool.h"
#include "gimppaintbrushtool.h"
#include "gimppenciltool.h"
#include "gimpperspectivetool.h"
#include "gimpposterizetool.h"
#include "gimprectselecttool.h"
#include "gimpthresholdtool.h"
#include "gimprotatetool.h"
#include "gimpscaletool.h"
#include "gimpsheartool.h"
#include "gimpsmudgetool.h"
#include "gimptexttool.h"
#include "gimpvectortool.h"

#include "gimp-intl.h"


/*  local function prototypes  */

static void   gimp_tools_register (GType                   tool_type,
                                   GType                   tool_options_type,
                                   GimpToolOptionsGUIFunc  options_gui_func,
                                   GimpContextPropMask     context_props,
                                   const gchar            *identifier,
                                   const gchar            *blurb,
                                   const gchar            *help,
                                   const gchar            *menu_path,
                                   const gchar            *menu_accel,
                                   const gchar            *help_domain,
                                   const gchar            *help_data,
                                   const gchar            *stock_id,
                                   gpointer                data);


/*  public functions  */

void
gimp_tools_init (Gimp *gimp)
{
  GimpToolRegisterFunc register_funcs[] =
  {
    /*  register tools in reverse order  */

    /*  color tools  */
    gimp_posterize_tool_register,
    gimp_curves_tool_register,
    gimp_levels_tool_register,
    gimp_threshold_tool_register,
    gimp_brightness_contrast_tool_register,
    gimp_colorize_tool_register,
    gimp_hue_saturation_tool_register,
    gimp_color_balance_tool_register,

    /*  paint tools  */

    gimp_dodgeburn_tool_register,
    gimp_smudge_tool_register,
    gimp_convolve_tool_register,
    gimp_clone_tool_register,
    gimp_ink_tool_register,
    gimp_airbrush_tool_register,
    gimp_eraser_tool_register,
    gimp_paintbrush_tool_register,
    gimp_pencil_tool_register,
    gimp_blend_tool_register,
    gimp_bucket_fill_tool_register,
    gimp_text_tool_register,

    /*  transform tools  */

    gimp_flip_tool_register,
    gimp_perspective_tool_register,
    gimp_shear_tool_register,
    gimp_scale_tool_register,
    gimp_rotate_tool_register,
    gimp_crop_tool_register,
    gimp_move_tool_register,

    /*  non-modifying tools  */

    gimp_measure_tool_register,
    gimp_magnify_tool_register,
    gimp_histogram_tool_register,
    gimp_color_picker_tool_register,

    /*  path tool */

    gimp_vector_tool_register,

    /*  selection tools */

    gimp_iscissors_tool_register,
    gimp_by_color_select_tool_register,
    gimp_fuzzy_select_tool_register,
    gimp_free_select_tool_register,
    gimp_ellipse_select_tool_register,
    gimp_rect_select_tool_register
  };

  gint i;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  tool_manager_init (gimp);

  gimp_container_freeze (gimp->tool_info_list);

  for (i = 0; i < G_N_ELEMENTS (register_funcs); i++)
    {
      register_funcs[i] (gimp_tools_register, gimp);
    }

  gimp_container_thaw (gimp->tool_info_list);
}

void
gimp_tools_exit (Gimp *gimp)
{
  g_return_if_fail (GIMP_IS_GIMP (gimp));

  tool_manager_exit (gimp);
}

void
gimp_tools_restore (Gimp *gimp)
{
  GList        *list;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  for (list = GIMP_LIST (gimp->tool_info_list)->list;
       list;
       list = g_list_next (list))
    {
      GimpToolInfo           *tool_info;
      GimpToolOptionsGUIFunc  options_gui_func;
      GtkWidget              *options_gui;

      tool_info = GIMP_TOOL_INFO (list->data);

      gimp_tool_options_deserialize (tool_info->tool_options, NULL, NULL);

      options_gui_func = g_object_get_data (G_OBJECT (tool_info),
                                            "gimp-tool-options-gui-func");

      if (options_gui_func)
        {
          options_gui = (* options_gui_func) (tool_info->tool_options);
        }
      else
        {
          GtkWidget *label;

          options_gui = gimp_tool_options_gui (tool_info->tool_options);

          label = gtk_label_new (_("This tool has no options."));
          gtk_box_pack_start (GTK_BOX (options_gui), label, FALSE, FALSE, 6);
          gtk_widget_show (label);
        }

      g_object_set_data (G_OBJECT (tool_info->tool_options),
                         "gimp-tool-options-gui", options_gui);

      if (tool_info->options_presets)
        {
          gchar *filename;
          GList *list;

          filename = gimp_tool_options_build_filename (tool_info->tool_options,
                                                       "presets");
          gimp_config_deserialize_file (GIMP_CONFIG (tool_info->options_presets),
                                        filename,
                                        gimp, NULL);
          g_free (filename);

          gimp_list_reverse (GIMP_LIST (tool_info->options_presets));

          for (list = GIMP_LIST (tool_info->options_presets)->list;
               list;
               list = g_list_next (list))
            {
              g_object_set (list->data, "tool-info", tool_info, NULL);
            }
        }
    }
}

void
gimp_tools_save (Gimp *gimp)
{
  GList        *list;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  for (list = GIMP_LIST (gimp->tool_info_list)->list;
       list;
       list = g_list_next (list))
    {
      GimpToolInfo *tool_info = GIMP_TOOL_INFO (list->data);

      gimp_tool_options_serialize (tool_info->tool_options, NULL, NULL);

      if (tool_info->options_presets)
        {
          gchar *filename;
          gchar *header;
          gchar *footer;

          filename = gimp_tool_options_build_filename (tool_info->tool_options,
                                                       "presets");

          header = g_strdup_printf ("GIMP %s options presets",
                                    GIMP_OBJECT (tool_info)->name);
          footer = g_strdup_printf ("end of %s options presets",
                                    GIMP_OBJECT (tool_info)->name);

          gimp_config_serialize_to_file (GIMP_CONFIG (tool_info->options_presets),
                                         filename, header, footer,
                                         NULL, NULL);

          g_free (filename);
          g_free (header);
          g_free (footer);
        }
    }
}


/*  private functions  */

static void
gimp_tools_register (GType                   tool_type,
                     GType                   tool_options_type,
                     GimpToolOptionsGUIFunc  options_gui_func,
                     GimpContextPropMask     context_props,
                     const gchar            *identifier,
                     const gchar            *blurb,
                     const gchar            *help,
                     const gchar            *menu_path,
                     const gchar            *menu_accel,
                     const gchar            *help_domain,
                     const gchar            *help_data,
                     const gchar            *stock_id,
                     gpointer                data)
{
  Gimp         *gimp = (Gimp *) data;
  GimpToolInfo *tool_info;
  const gchar  *paint_core_name;

  g_return_if_fail (GIMP_IS_GIMP (gimp));
  g_return_if_fail (g_type_is_a (tool_type, GIMP_TYPE_TOOL));
  g_return_if_fail (tool_options_type == G_TYPE_NONE ||
                    g_type_is_a (tool_options_type, GIMP_TYPE_TOOL_OPTIONS));

  if (tool_options_type == G_TYPE_NONE)
    tool_options_type = GIMP_TYPE_TOOL_OPTIONS;

  if (tool_type == GIMP_TYPE_PENCIL_TOOL)
    {
      paint_core_name = "GimpPencil";
    }
  else if (tool_type == GIMP_TYPE_PAINTBRUSH_TOOL)
    {
      paint_core_name = "GimpPaintbrush";
    }
  else if (tool_type == GIMP_TYPE_ERASER_TOOL)
    {
      paint_core_name = "GimpEraser";
    }
  else if (tool_type == GIMP_TYPE_AIRBRUSH_TOOL)
    {
      paint_core_name = "GimpAirbrush";
    }
  else if (tool_type == GIMP_TYPE_CLONE_TOOL)
    {
      paint_core_name = "GimpClone";
    }
  else if (tool_type == GIMP_TYPE_CONVOLVE_TOOL)
    {
      paint_core_name = "GimpConvolve";
    }
  else if (tool_type == GIMP_TYPE_SMUDGE_TOOL)
    {
      paint_core_name = "GimpSmudge";
    }
  else if (tool_type == GIMP_TYPE_DODGEBURN_TOOL)
    {
      paint_core_name = "GimpDodgeBurn";
    }
  else
    {
      paint_core_name = "GimpPaintbrush";
    }

  tool_info = gimp_tool_info_new (gimp,
				  tool_type,
                                  tool_options_type,
				  context_props,
				  identifier,
				  blurb,
				  help,
				  menu_path,
				  menu_accel,
				  help_domain,
				  help_data,
                                  paint_core_name,
				  stock_id);

  if (g_type_is_a (tool_type, GIMP_TYPE_IMAGE_MAP_TOOL))
    tool_info->in_toolbox = FALSE;

  g_object_set_data (G_OBJECT (tool_info), "gimp-tool-options-gui-func",
                     options_gui_func);

  if (tool_info->context_props)
    {
      gimp_context_define_properties (GIMP_CONTEXT (tool_info->tool_options),
                                      tool_info->context_props, FALSE);

      gimp_context_copy_properties (gimp_get_user_context (gimp),
                                    GIMP_CONTEXT (tool_info->tool_options),
                                    GIMP_CONTEXT_ALL_PROPS_MASK);
    }

  gimp_container_add (gimp->tool_info_list, GIMP_OBJECT (tool_info));
  g_object_unref (tool_info);

  if (tool_type == GIMP_TYPE_RECT_SELECT_TOOL)
    gimp_tool_info_set_standard (gimp, tool_info);
}
