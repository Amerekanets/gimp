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

#include <glib-object.h>

#include "libgimpcolor/gimpcolor.h"

#include "paint-types.h"

#include "base/temp-buf.h"

#include "paint-funcs/paint-funcs.h"

#include "core/gimp.h"
#include "core/gimpdrawable.h"
#include "core/gimpimage.h"

#include "gimperaser.h"
#include "gimperaseroptions.h"

#include "gimp-intl.h"


static void   gimp_eraser_class_init    (GimpEraserClass    *klass);
static void   gimp_eraser_init          (GimpEraser         *eraser);

static void   gimp_eraser_paint         (GimpPaintCore      *paint_core,
                                         GimpDrawable       *drawable,
                                         GimpPaintOptions   *paint_options,
                                         GimpPaintCoreState  paint_state);

static void   gimp_eraser_motion        (GimpPaintCore      *paint_core,
                                         GimpDrawable       *drawable,
                                         GimpPaintOptions   *paint_options);


static GimpPaintCoreClass *parent_class = NULL;


void
gimp_eraser_register (Gimp                      *gimp,
                      GimpPaintRegisterCallback  callback)
{
  (* callback) (gimp,
                GIMP_TYPE_ERASER,
                GIMP_TYPE_ERASER_OPTIONS,
                _("Eraser"));
}

GType
gimp_eraser_get_type (void)
{
  static GType type = 0;

  if (! type)
    {
      static const GTypeInfo info =
      {
        sizeof (GimpEraserClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_eraser_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data     */
	sizeof (GimpEraser),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_eraser_init,
      };

      type = g_type_register_static (GIMP_TYPE_PAINT_CORE,
                                     "GimpEraser", 
                                     &info, 0);
    }

  return type;
}

static void
gimp_eraser_class_init (GimpEraserClass *klass)
{
  GimpPaintCoreClass *paint_core_class;

  paint_core_class = GIMP_PAINT_CORE_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  paint_core_class->paint = gimp_eraser_paint;
}

static void
gimp_eraser_init (GimpEraser *eraser)
{
  GimpPaintCore *paint_core;

  paint_core = GIMP_PAINT_CORE (eraser);

  paint_core->flags |= CORE_HANDLES_CHANGING_BRUSH;
}

static void
gimp_eraser_paint (GimpPaintCore      *paint_core,
                   GimpDrawable       *drawable,
                   GimpPaintOptions   *paint_options,
                   GimpPaintCoreState  paint_state)
{
  switch (paint_state)
    {
    case MOTION_PAINT:
      gimp_eraser_motion (paint_core, drawable, paint_options);
      break;

    default:
      break;
    }
}

static void
gimp_eraser_motion (GimpPaintCore    *paint_core,
                    GimpDrawable     *drawable,
                    GimpPaintOptions *paint_options)
{
  GimpEraserOptions        *options;
  GimpPressureOptions      *pressure_options;
  GimpContext              *context;
  GimpImage                *gimage;
  gdouble                   opacity;
  TempBuf                  *area;
  guchar                    col[MAX_CHANNELS];
  gdouble                   scale;
  GimpBrushApplicationMode  brush_mode;

  if (! (gimage = gimp_item_get_image (GIMP_ITEM (drawable))))
    return;

  options = GIMP_ERASER_OPTIONS (paint_options);
  context = GIMP_CONTEXT (paint_options);

  pressure_options = paint_options->pressure_options;

  gimp_image_get_background (gimage, drawable, col);

  if (pressure_options->size)
    scale = paint_core->cur_coords.pressure;
  else
    scale = 1.0;

  /*  Get a region which can be used to paint to  */
  if (! (area = gimp_paint_core_get_paint_area (paint_core, drawable, scale)))
    return;

  /*  set the alpha channel  */
  col[area->bytes - 1] = OPAQUE_OPACITY;

  /*  color the pixels  */
  color_pixels (temp_buf_data (area), col,
		area->width * area->height, area->bytes);

  opacity = gimp_context_get_opacity (context);

  if (pressure_options->opacity)
    opacity = opacity * 2.0 * paint_core->cur_coords.pressure;

  /* paste the newly painted canvas to the gimage which is being
   * worked on
   */
  
  if (options->hard)
    brush_mode = GIMP_BRUSH_HARD;
  else
    brush_mode = (pressure_options->pressure ? 
                  GIMP_BRUSH_PRESSURE : GIMP_BRUSH_SOFT);

  gimp_paint_core_paste_canvas (paint_core, drawable, 
				MIN (opacity, GIMP_OPACITY_OPAQUE),
				gimp_context_get_opacity (context),
				(options->anti_erase ? 
                                 GIMP_ANTI_ERASE_MODE : GIMP_ERASE_MODE),
				brush_mode,
				scale,
				paint_options->application_mode);
}
