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

#ifndef __HUE_SATURATION_H__
#define __HUE_SATURATION_H__

#include "image_map.h"
#include "tools.h"


typedef enum
{
  ALL_HUES,
  RED_HUES,
  YELLOW_HUES,
  GREEN_HUES,
  CYAN_HUES,
  BLUE_HUES,
  MAGENTA_HUES
} HueRange;

typedef struct _HueSaturationDialog HueSaturationDialog;

struct _HueSaturationDialog
{
  GtkWidget     *shell;

  GtkWidget     *hue_partition_da[6];

  GtkAdjustment *hue_data;
  GtkAdjustment *lightness_data;
  GtkAdjustment *saturation_data;

  GimpDrawable  *drawable;
  ImageMap       image_map;

  gdouble        hue[7];
  gdouble        lightness[7];
  gdouble        saturation[7];

  HueRange       hue_partition;
  gboolean       preview;
};

Tool * tools_new_hue_saturation          (void);
void   tools_free_hue_saturation         (Tool                 *tool);

void   hue_saturation_initialize         (GDisplay             *gdisp);
void   hue_saturation_free               (void);
void   hue_saturation_dialog_hide	 (void);
void   hue_saturation                    (PixelRegion          *srcPR,
					  PixelRegion          *destPR,
					  void                 *data);

void   hue_saturation_calculate_transfers (HueSaturationDialog *hsd);

#endif  /*  __HUE_SATURATION_H__  */
