/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * GimpCoreConfig class
 * Copyright (C) 2001  Sven Neumann <sven@gimp.org>
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

#ifndef __GIMP_CORE_CONFIG_H__
#define __GIMP_CORE_CONFIG_H__

#include "core/core-enums.h"

#include "config/gimpbaseconfig.h"


#define GIMP_TYPE_CORE_CONFIG            (gimp_core_config_get_type ())
#define GIMP_CORE_CONFIG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_CORE_CONFIG, GimpCoreConfig))
#define GIMP_CORE_CONFIG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_CORE_CONFIG, GimpCoreConfigClass))
#define GIMP_IS_CORE_CONFIG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_CORE_CONFIG))
#define GIMP_IS_CORE_CONFIG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_CORE_CONFIG))


typedef struct _GimpCoreConfigClass GimpCoreConfigClass;

struct _GimpCoreConfig
{
  GimpBaseConfig         parent_instance;

  GimpInterpolationType  interpolation_type;
  gchar                 *plug_in_path;
  gchar                 *module_path;
  gchar                 *environ_path;
  gchar                 *brush_path;
  gchar                 *pattern_path;
  gchar                 *palette_path;
  gchar                 *gradient_path;
  gchar                 *default_brush;
  gchar                 *default_pattern;
  gchar                 *default_palette;
  gchar                 *default_gradient;
  gchar                 *default_font;
  gchar                 *default_comment;
  GimpImageBaseType      default_image_type;
  gint                   default_image_width;
  gint                   default_image_height;
  GimpUnit               default_unit;
  gdouble                default_xresolution;
  gdouble                default_yresolution;
  GimpUnit               default_resolution_unit;
  gint                   levels_of_undo;
  gulong                 undo_size;
  gchar                 *plug_in_rc_path;
  gchar                 *module_load_inhibit;
  gboolean               layer_previews;
  GimpPreviewSize        layer_preview_size;
  GimpThumbnailSize      thumbnail_size;
  gdouble                gamma_val;
  gboolean               install_cmap;
  gint                   min_colors;
};

struct _GimpCoreConfigClass
{
  GimpBaseConfigClass  parent_class;
};


GType  gimp_core_config_get_type (void) G_GNUC_CONST;


#endif /* GIMP_CORE_CONFIG_H__ */
