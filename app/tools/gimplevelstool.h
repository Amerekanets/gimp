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

#ifndef __GIMP_LEVELS_TOOL_H__
#define __GIMP_LEVELS_TOOL_H__


#include "gimpimagemaptool.h"


#define GIMP_TYPE_LEVELS_TOOL            (gimp_levels_tool_get_type ())
#define GIMP_LEVELS_TOOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_LEVELS_TOOL, GimpLevelsTool))
#define GIMP_LEVELS_TOOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_LEVELS_TOOL, GimpLevelsToolClass))
#define GIMP_IS_LEVELS_TOOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_LEVELS_TOOL))
#define GIMP_IS_LEVELS_TOOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_LEVELS_TOOL))
#define GIMP_LEVELS_TOOL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_LEVELS_TOOL, GimpLevelsToolClass))


typedef struct _GimpLevelsTool      GimpLevelsTool;
typedef struct _GimpLevelsToolClass GimpLevelsToolClass;

struct _GimpLevelsTool
{
  GimpImageMapTool      parent_instance;

  GimpLut              *lut;
  gboolean              color;
  Levels               *levels;

  /* dialog */
  GimpHistogramChannel  channel;
  gint                  active_slider;
  gint                  slider_pos[5];  /*  positions for the five sliders  */

  GimpHistogram        *hist;
  GimpHistogramView    *histogram;
  GtkAdjustment        *low_input_data;
  GtkAdjustment        *gamma_data;
  GtkAdjustment        *high_input_data;
  GtkAdjustment        *low_output_data;
  GtkAdjustment        *high_output_data;
  GtkWidget            *input_levels_da[2];
  GtkWidget            *output_levels_da[2];
  GtkWidget            *channel_menu;

  GtkWidget            *file_dialog;
  gboolean              is_save;
};

struct _GimpLevelsToolClass
{
  GimpImageMapToolClass  parent_class;
};


void    gimp_levels_tool_register (GimpToolRegisterCallback  callback,
                                   gpointer                  data);

GType   gimp_levels_tool_get_type (void) G_GNUC_CONST;


#endif  /*  __GIMP_LEVELS_TOOL_H__  */
