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

#include "libgimpmath/gimpmath.h"

#include "base-types.h"

#include "gimphistogram.h"
#include "levels.h"


/*  public functions  */

void
levels_init (Levels *levels)
{
  GimpHistogramChannel channel;

  g_return_if_fail (levels != NULL);

  for (channel = GIMP_HISTOGRAM_VALUE;
       channel <= GIMP_HISTOGRAM_ALPHA;
       channel++)
    {
      levels_channel_reset (levels, channel);
    }
}

void
levels_channel_reset (Levels               *levels,
                      GimpHistogramChannel  channel)
{
  g_return_if_fail (levels != NULL);

  levels->gamma[channel]       = 1.0;
  levels->low_input[channel]   = 0;
  levels->high_input[channel]  = 255;
  levels->low_output[channel]  = 0;
  levels->high_output[channel] = 255;
}

void
levels_auto (Levels        *levels,
             GimpHistogram *hist,
             gboolean       color)
{
  GimpHistogramChannel channel;

  g_return_if_fail (levels != NULL);
  g_return_if_fail (hist != NULL);

  if (color)
    {
      /*  Set the overall value to defaults  */
      levels_channel_reset (levels, GIMP_HISTOGRAM_VALUE);

      for (channel = GIMP_HISTOGRAM_RED;
           channel <= GIMP_HISTOGRAM_BLUE;
           channel++)
	levels_channel_auto (levels, hist, channel);
    }
  else
    {
      levels_channel_auto (levels, hist, GIMP_HISTOGRAM_VALUE);
    }
}

void
levels_channel_auto (Levels               *levels,
                     GimpHistogram        *hist,
                     GimpHistogramChannel  channel)
{
  gint    i;
  gdouble count, new_count, percentage, next_percentage;

  g_return_if_fail (levels != NULL);
  g_return_if_fail (hist != NULL);

  levels->gamma[channel]       = 1.0;
  levels->low_output[channel]  = 0;
  levels->high_output[channel] = 255;

  count = gimp_histogram_get_count (hist, 0, 255);

  if (count == 0.0)
    {
      levels->low_input[channel]  = 0;
      levels->high_input[channel] = 0;
    }
  else
    {
      /*  Set the low input  */
      new_count = 0.0;

      for (i = 0; i < 255; i++)
	{
	  new_count += gimp_histogram_get_value (hist, channel, i);
	  percentage = new_count / count;
	  next_percentage =
	    (new_count + gimp_histogram_get_value (hist,
						   channel,
						   i + 1)) / count;
	  if (fabs (percentage - 0.006) < fabs (next_percentage - 0.006))
	    {
	      levels->low_input[channel] = i + 1;
	      break;
	    }
	}
      /*  Set the high input  */
      new_count = 0.0;
      for (i = 255; i > 0; i--)
	{
	  new_count += gimp_histogram_get_value (hist, channel, i);
	  percentage = new_count / count;
	  next_percentage =
	    (new_count + gimp_histogram_get_value (hist,
						   channel,
						   i - 1)) / count;
	  if (fabs (percentage - 0.006) < fabs (next_percentage - 0.006))
	    {
	      levels->high_input[channel] = i - 1;
	      break;
	    }
	}
    }
}

void
levels_calculate_transfers (Levels *levels)
{
  gdouble inten;
  gint    i, j;

  g_return_if_fail (levels != NULL);

  /*  Recalculate the levels arrays  */
  for (j = 0; j < 5; j++)
    {
      for (i = 0; i < 256; i++)
	{
	  /*  determine input intensity  */
	  if (levels->high_input[j] != levels->low_input[j])
            {
              inten = ((gdouble) (i - levels->low_input[j]) /
                       (double) (levels->high_input[j] - levels->low_input[j]));
            }
	  else
            {
              inten = (gdouble) (i - levels->low_input[j]);
            }

	  inten = CLAMP (inten, 0.0, 1.0);

	  if (levels->gamma[j] != 0.0)
	    inten = pow (inten, (1.0 / levels->gamma[j]));

	  levels->input[j][i] = (guchar) (inten * 255.0 + 0.5);
	}
    }
}

gfloat
levels_lut_func (Levels *levels,
		 gint    n_channels,
		 gint    channel,
		 gfloat  value)
{
  gdouble inten;
  gint    j;

  if (n_channels == 1)
    j = 0;
  else
    j = channel + 1;

  inten = value;

  /* For color  images this runs through the loop with j = channel +1
   * the first time and j = 0 the second time
   *
   * For bw images this runs through the loop with j = 0 the first and
   *  only time
   */
  for (; j >= 0; j -= (channel + 1))
    {
      /* don't apply the overall curve to the alpha channel */
      if (j == 0 && (n_channels == 2 || n_channels == 4)
	  && channel == n_channels -1)
	return inten;

      /*  determine input intensity  */
      if (levels->high_input[j] != levels->low_input[j])
	inten = ((gdouble) (255.0 * inten - levels->low_input[j]) /
		 (gdouble) (levels->high_input[j] - levels->low_input[j]));
      else
	inten = (gdouble) (255.0 * inten - levels->low_input[j]);

      if (levels->gamma[j] != 0.0)
	{
	  if (inten >= 0.0)
	    inten =  pow ( inten, (1.0 / levels->gamma[j]));
	  else
	    inten = -pow (-inten, (1.0 / levels->gamma[j]));
	}

      /*  determine the output intensity  */
      if (levels->high_output[j] >= levels->low_output[j])
	inten = (gdouble) (inten * (levels->high_output[j] -
                                    levels->low_output[j]) +
			   levels->low_output[j]);
      else if (levels->high_output[j] < levels->low_output[j])
	inten = (gdouble) (levels->low_output[j] - inten *
			   (levels->low_output[j] - levels->high_output[j]));

      inten /= 255.0;
    }

  return inten;
}
