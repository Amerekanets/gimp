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
#include "libgimpmath/gimpmath.h"

#include "base-types.h"

#include "colorize.h"
#include "pixel-region.h"


void
colorize_init (Colorize *colorize)
{
  gint i;

  g_return_if_fail (colorize != NULL);

  colorize->hue        = 360.0 / 2;
  colorize->lightness  = 100.0 / 2;
  colorize->saturation = 100.0 / 2;

  for (i = 0; i < 256; i ++)
    {
      colorize->lum_red_lookup[i]   = i * INTENSITY_RED;
      colorize->lum_green_lookup[i] = i * INTENSITY_GREEN;
      colorize->lum_blue_lookup[i]  = i * INTENSITY_BLUE;
    }
}

void
colorize_calculate (Colorize *colorize)
{
  GimpHSL hsl;
  GimpRGB rgb;
  gint    i;

  g_return_if_fail (colorize != NULL);

  hsl.h = colorize->hue        / 360.0;
  hsl.s = colorize->saturation / 100.0;
  hsl.l = colorize->lightness  / 100.0;

  gimp_hsl_to_rgb (&hsl, &rgb);

  /*  Calculate transfers  */
  for (i = 0; i < 256; i ++)
    {
      colorize->final_red_lookup[i]   = i * rgb.r;
      colorize->final_green_lookup[i] = i * rgb.g;
      colorize->final_blue_lookup[i]  = i * rgb.b;
    }
}

void
colorize (PixelRegion *srcPR,
          PixelRegion *destPR,
          Colorize    *colorize)
{
  guchar   *src, *s;
  guchar   *dest, *d;
  gboolean  alpha;
  gint      w, h;
  gint      lum;

  /*  Set the transfer arrays  (for speed)  */
  h     = srcPR->h;
  src   = srcPR->data;
  dest  = destPR->data;
  alpha = (srcPR->bytes == 4) ? TRUE : FALSE;

  while (h--)
    {
      w = srcPR->w;
      s = src;
      d = dest;

      while (w--)
	{
          lum = (colorize->lum_red_lookup[s[RED_PIX]] +
                 colorize->lum_green_lookup[s[GREEN_PIX]] +
                 colorize->lum_blue_lookup[s[BLUE_PIX]]); /* luminosity */

          d[RED_PIX]   = colorize->final_red_lookup[lum];
          d[GREEN_PIX] = colorize->final_green_lookup[lum];
          d[BLUE_PIX]  = colorize->final_blue_lookup[lum];

	  if (alpha)
	    d[ALPHA_PIX] = s[ALPHA_PIX];

	  s += srcPR->bytes;
	  d += destPR->bytes;
	}

      src  += srcPR->rowstride;
      dest += destPR->rowstride;
    }
}
