/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimphistogram module Copyright (C) 1999 Jay Cox <jaycox@earthlink.net>
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

#include "libgimpmath/gimpmath.h"

#include "core-types.h"

#include "base/gimphistogram.h"
#include "base/pixel-region.h"

#include "gimpdrawable.h"
#include "gimpimage.h"


void
gimp_drawable_calculate_histogram (GimpDrawable  *drawable,
				   GimpHistogram *histogram)
{
  PixelRegion region;
  PixelRegion mask;
  gint        x1, y1, x2, y2;
  gint        off_x, off_y;
  gboolean    no_mask;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (GIMP_IS_DRAWABLE (drawable));
  g_return_if_fail (histogram != NULL);

  no_mask = (gimp_drawable_mask_bounds (drawable, &x1, &y1, &x2, &y2) == FALSE);
  pixel_region_init (&region, gimp_drawable_data (drawable), x1, y1,
		     (x2 - x1), (y2 - y1), FALSE);

  if (! no_mask)
    {
      GimpChannel *sel_mask;
      GimpImage   *gimage;

      gimage = gimp_drawable_gimage (drawable);
      sel_mask = gimp_image_get_mask (gimage);

      gimp_drawable_offsets (drawable, &off_x, &off_y);
      pixel_region_init (&mask, gimp_drawable_data (GIMP_DRAWABLE (sel_mask)),
			 x1 + off_x, y1 + off_y, (x2 - x1), (y2 - y1), FALSE);
      gimp_histogram_calculate (histogram, &region, &mask);
    }
  else
    {
      gimp_histogram_calculate (histogram, &region, NULL);
    }
}
