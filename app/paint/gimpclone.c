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

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "paint-types.h"

#include "base/pixel-region.h"
#include "base/temp-buf.h"

#include "paint-funcs/paint-funcs.h"

#include "core/gimp.h"
#include "core/gimpbrush.h"
#include "core/gimpcontext.h"
#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimpimage-mask.h"
#include "core/gimppattern.h"

#include "gimpclone.h"

#include "libgimp/gimpintl.h"


static void   gimp_clone_class_init   (GimpCloneClass       *klass);
static void   gimp_clone_init         (GimpClone            *clone);

static void   gimp_clone_paint        (GimpPaintCore        *paint_core,
                                       GimpDrawable         *drawable,
                                       PaintOptions         *paint_options,
                                       GimpPaintCoreState    paint_state);
static void   gimp_clone_motion       (GimpPaintCore        *paint_core,
                                       GimpDrawable         *drawable,
                                       GimpDrawable         *src_drawable,
                                       PaintPressureOptions *pressure_options,
                                       CloneType             type,
                                       gint                  offset_x,
                                       gint                  offset_y);
static void   gimp_clone_line_image   (GimpImage            *dest,
                                       GimpImage            *src,
                                       GimpDrawable         *d_drawable,
                                       GimpDrawable         *s_drawable,
                                       guchar               *s,
                                       guchar               *d,
                                       gint                  has_alpha,
                                       gint                  src_bytes,
                                       gint                  dest_bytes,
                                       gint                  width);
static void   gimp_clone_line_pattern (GimpImage            *dest,
                                       GimpDrawable         *drawable,
                                       GimpPattern          *pattern,
                                       guchar               *d,
                                       gint                  x,
                                       gint                  y,
                                       gint                  bytes,
                                       gint                  width);

static void   gimp_clone_set_src_drawable (GimpClone    *clone,
                                           GimpDrawable *drawable);


static gint   dest_x   = 0;     /*                         */
static gint   dest_y   = 0;     /*  position of clone src  */
static gint   offset_x = 0;     /*                         */
static gint   offset_y = 0;     /*  offset for cloning     */
static gint   first    = TRUE;

static GimpPaintCoreClass *parent_class = NULL;


GType
gimp_clone_get_type (void)
{
  static GType type = 0;

  if (! type)
    {
      static const GTypeInfo info =
      {
        sizeof (GimpCloneClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_clone_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data     */
	sizeof (GimpClone),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_clone_init,
      };

      type = g_type_register_static (GIMP_TYPE_PAINT_CORE,
                                     "GimpClone",
                                     &info, 0);
    }

  return type;
}

static void
gimp_clone_class_init (GimpCloneClass *klass)
{
  GimpPaintCoreClass *paint_core_class;

  paint_core_class = GIMP_PAINT_CORE_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  paint_core_class->paint = gimp_clone_paint;
}

static void
gimp_clone_init (GimpClone *clone)
{
  GimpPaintCore *paint_core;

  paint_core = GIMP_PAINT_CORE (clone);

  paint_core->flags |= CORE_CAN_HANDLE_CHANGING_BRUSH;
  paint_core->flags |= CORE_TRACES_ON_WINDOW;

  clone->set_source         = FALSE;

  clone->src_drawable       = NULL;
  clone->src_x              = 0.0;
  clone->src_y              = 0.0;

  clone->init_callback      = NULL;
  clone->finish_callback    = NULL;
  clone->pretrace_callback  = NULL;
  clone->posttrace_callback = NULL;
  clone->callback_data      = NULL;
}

static void
gimp_clone_paint (GimpPaintCore      *paint_core,
                  GimpDrawable       *drawable,
                  PaintOptions       *paint_options,
                  GimpPaintCoreState  paint_state)
{
  static gint   orig_src_x = 0;
  static gint   orig_src_y = 0;

  GimpClone    *clone;
  CloneOptions *options;
  GimpContext  *context;

  clone = GIMP_CLONE (paint_core);

  options = (CloneOptions *) paint_options;

  context = gimp_get_current_context (drawable->gimage->gimp);

  switch (paint_state)
    {
    case PRETRACE_PAINT:
      if (clone->pretrace_callback)
        clone->pretrace_callback (clone, clone->callback_data);
      break;

    case POSTTRACE_PAINT:
      if (clone->posttrace_callback)
        clone->posttrace_callback (clone, clone->callback_data);
      break;

    case MOTION_PAINT:
      if (clone->set_source)
	{
          /*  If the control key is down, move the src target and return */

	  clone->src_x = paint_core->cur_coords.x;
	  clone->src_y = paint_core->cur_coords.y;
	  first = TRUE;
	}
      else
	{
          /*  otherwise, update the target  */

	  dest_x = paint_core->cur_coords.x;
	  dest_y = paint_core->cur_coords.y;

          if (options->aligned == ALIGN_REGISTERED)
            {
	      offset_x = 0;
	      offset_y = 0;
            }
          else if (first)
	    {
	      offset_x = clone->src_x - dest_x;
	      offset_y = clone->src_y - dest_y;
	      first = FALSE;
	    }

	  clone->src_x = dest_x + offset_x;
	  clone->src_y = dest_y + offset_y;

	  gimp_clone_motion (paint_core, drawable,
                             clone->src_drawable, 
                             options->paint_options.pressure_options, 
                             options->type,
                             offset_x, offset_y);
	}
      break;

    case INIT_PAINT:
      if (clone->set_source)
	{
	  gimp_clone_set_src_drawable (clone, drawable);

	  clone->src_x = paint_core->cur_coords.x;
	  clone->src_y = paint_core->cur_coords.y;
	  first = TRUE;
	}
      else if (options->aligned == ALIGN_NO)
	{
	  first = TRUE;
	  orig_src_x = clone->src_x;
	  orig_src_y = clone->src_y;
	}

      if (clone->init_callback)
        clone->init_callback (clone, clone->callback_data);

      if (options->type == PATTERN_CLONE)
	if (! gimp_context_get_pattern (context))
	  g_message (_("No patterns available for this operation."));
      break;

    case FINISH_PAINT:
      if (clone->finish_callback)
        clone->finish_callback (clone, clone->callback_data);

      if (options->aligned == ALIGN_NO && !first)
	{
	  clone->src_x = orig_src_x;
	  clone->src_y = orig_src_y;
	}
      break;

    default:
      break;
    }
}

static void
gimp_clone_motion (GimpPaintCore        *paint_core,
                   GimpDrawable         *drawable,
                   GimpDrawable         *src_drawable,
                   PaintPressureOptions *pressure_options,
                   CloneType             type,
                   gint                  offset_x,
                   gint                  offset_y)
{
  GimpImage   *gimage;
  GimpImage   *src_gimage = NULL;
  GimpContext *context;
  guchar      *s;
  guchar      *d;
  TempBuf     *orig;
  TempBuf     *area;
  gpointer     pr;
  gint         y;
  gint         x1, y1, x2, y2;
  gint         has_alpha = -1;
  PixelRegion  srcPR, destPR;
  GimpPattern *pattern;
  gint         opacity;
  gdouble      scale;

  pr      = NULL;
  pattern = NULL;

  /*  Make sure we still have a source if we are doing image cloning */
  if (type == IMAGE_CLONE)
    {
      if (!src_drawable)
	return;
      if (! (src_gimage = gimp_drawable_gimage (src_drawable)))
	return;
      /*  Determine whether the source image has an alpha channel  */
      has_alpha = gimp_drawable_has_alpha (src_drawable);
    }

  /*  We always need a destination image */
  if (! (gimage = gimp_drawable_gimage (drawable)))
    return;

  context = gimp_get_current_context (gimage->gimp);

  if (pressure_options->size)
    scale = paint_core->cur_coords.pressure;
  else
    scale = 1.0;

  /*  Get a region which can be used to paint to  */
  if (! (area = gimp_paint_core_get_paint_area (paint_core, drawable, scale)))
    return;

  switch (type)
    {
    case IMAGE_CLONE:
      /*  Set the paint area to transparent  */
      temp_buf_data_clear (area);

      /*  If the source gimage is different from the destination,
       *  then we should copy straight from the destination image
       *  to the canvas.
       *  Otherwise, we need a call to get_orig_image to make sure
       *  we get a copy of the unblemished (offset) image
       */
      if (src_drawable != drawable)
	{
	  x1 = CLAMP (area->x + offset_x, 0, gimp_drawable_width (src_drawable));
	  y1 = CLAMP (area->y + offset_y, 0, gimp_drawable_height (src_drawable));
	  x2 = CLAMP (area->x + offset_x + area->width,
		      0, gimp_drawable_width (src_drawable));
	  y2 = CLAMP (area->y + offset_y + area->height,
		      0, gimp_drawable_height (src_drawable));

	  if (!(x2 - x1) || !(y2 - y1))
	    return;

	  pixel_region_init (&srcPR, gimp_drawable_data (src_drawable),
			     x1, y1, (x2 - x1), (y2 - y1), FALSE);
	}
      else
	{
	  x1 = CLAMP (area->x + offset_x, 0, gimp_drawable_width (drawable));
	  y1 = CLAMP (area->y + offset_y, 0, gimp_drawable_height (drawable));
	  x2 = CLAMP (area->x + offset_x + area->width,
		      0, gimp_drawable_width (drawable));
	  y2 = CLAMP (area->y + offset_y + area->height,
		      0, gimp_drawable_height (drawable));

	  if (!(x2 - x1) || !(y2 - y1))
	    return;

	  /*  get the original image  */
	  orig = gimp_paint_core_get_orig_image (paint_core, drawable, x1, y1, x2, y2);

	  srcPR.bytes = orig->bytes;
	  srcPR.x = 0; srcPR.y = 0;
	  srcPR.w = x2 - x1;
	  srcPR.h = y2 - y1;
	  srcPR.rowstride = srcPR.bytes * orig->width;
	  srcPR.data = temp_buf_data (orig);
	}

      offset_x = x1 - (area->x + offset_x);
      offset_y = y1 - (area->y + offset_y);

      /*  configure the destination  */
      destPR.bytes = area->bytes;
      destPR.x = 0; destPR.y = 0;
      destPR.w = srcPR.w;
      destPR.h = srcPR.h;
      destPR.rowstride = destPR.bytes * area->width;
      destPR.data = temp_buf_data (area) + offset_y * destPR.rowstride +
	offset_x * destPR.bytes;

      pr = pixel_regions_register (2, &srcPR, &destPR);
      break;

    case PATTERN_CLONE:
      pattern = gimp_context_get_pattern (context);

      if (!pattern)
	return;

      destPR.bytes = area->bytes;
      destPR.x = 0; destPR.y = 0;
      destPR.w = area->width;
      destPR.h = area->height;
      destPR.rowstride = destPR.bytes * area->width;
      destPR.data = temp_buf_data (area);

      pr = pixel_regions_register (1, &destPR);
      break;
    }

  for (; pr != NULL; pr = pixel_regions_process (pr))
    {
      s = srcPR.data;
      d = destPR.data;
      for (y = 0; y < destPR.h; y++)
	{
	  switch (type)
	    {
	    case IMAGE_CLONE:
	      gimp_clone_line_image (gimage, src_gimage,
                                     drawable, src_drawable,
                                     s, d, has_alpha, 
                                     srcPR.bytes, destPR.bytes, destPR.w);
	      s += srcPR.rowstride;
	      break;
	    case PATTERN_CLONE:
	      gimp_clone_line_pattern (gimage, drawable,
                                       pattern, d,
                                       area->x + offset_x, 
                                       area->y + y + offset_y,
                                       destPR.bytes, destPR.w);
	      break;
	    }

	  d += destPR.rowstride;
	}
    }

  opacity = 255.0 * gimp_context_get_opacity (context);
  if (pressure_options->opacity)
    opacity = opacity * 2.0 * paint_core->cur_coords.pressure;

  /*  paste the newly painted canvas to the gimage which is being worked on  */
  gimp_paint_core_paste_canvas (paint_core, drawable, 
				MIN (opacity, 255),
				(gint) (gimp_context_get_opacity (context) * 255),
				gimp_context_get_paint_mode (context),
				pressure_options->pressure ? PRESSURE : SOFT, 
				scale, CONSTANT);
}

static void
gimp_clone_line_image (GimpImage    *dest,
                       GimpImage    *src,
                       GimpDrawable *d_drawable,
                       GimpDrawable *s_drawable,
                       guchar       *s,
                       guchar       *d,
                       gint          has_alpha,
                       gint          src_bytes,
                       gint          dest_bytes,
                       gint          width)
{
  guchar rgb[3];
  gint   src_alpha, dest_alpha;

  src_alpha  = src_bytes - 1;
  dest_alpha = dest_bytes - 1;

  while (width--)
    {
      gimp_image_get_color (src, gimp_drawable_type (s_drawable), rgb, s);
      gimp_image_transform_color (dest, d_drawable, rgb, d, GIMP_RGB);

      if (has_alpha)
	d[dest_alpha] = s[src_alpha];
      else
	d[dest_alpha] = OPAQUE_OPACITY;

      s += src_bytes;
      d += dest_bytes;
    }
}

static void
gimp_clone_line_pattern (GimpImage    *dest,
                         GimpDrawable *drawable,
                         GimpPattern  *pattern,
                         guchar       *d,
                         gint          x,
                         gint          y,
                         gint          bytes,
                         gint          width)
{
  guchar            *pat, *p;
  GimpImageBaseType  color_type;
  gint               alpha;
  gint               i;

  /*  Make sure x, y are positive  */
  while (x < 0)
    x += pattern->mask->width;
  while (y < 0)
    y += pattern->mask->height;

  /*  Get a pointer to the appropriate scanline of the pattern buffer  */
  pat = temp_buf_data (pattern->mask) +
    (y % pattern->mask->height) * pattern->mask->width * pattern->mask->bytes;

  color_type = (pattern->mask->bytes == 3) ? GIMP_RGB : GIMP_GRAY;

  alpha = bytes - 1;

  for (i = 0; i < width; i++)
    {
      p = pat + ((i + x) % pattern->mask->width) * pattern->mask->bytes;

      gimp_image_transform_color (dest, drawable, p, d, color_type);

      d[alpha] = OPAQUE_OPACITY;

      d += bytes;
    }
}

static void
gimp_clone_src_drawable_disconnect_cb (GimpDrawable *drawable,
                                       GimpClone    *clone)
{
  if (drawable == clone->src_drawable)
    {
      clone->src_drawable = NULL;
    }
}

static void
gimp_clone_set_src_drawable (GimpClone    *clone,
                             GimpDrawable *drawable)
{
  if (clone->src_drawable == drawable)
    return;

  if (clone->src_drawable)
    g_signal_handlers_disconnect_by_func (G_OBJECT (clone->src_drawable),
                                          gimp_clone_src_drawable_disconnect_cb, 
                                          clone);

  clone->src_drawable = drawable;

  if (clone->src_drawable)
    {
      g_signal_connect (G_OBJECT (clone->src_drawable), "disconnect",
                        G_CALLBACK (gimp_clone_src_drawable_disconnect_cb),
                        clone);
    }
}
