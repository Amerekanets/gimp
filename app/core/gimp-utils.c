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

#include "core-types.h"

#include "gimp-utils.h"


gboolean
gimp_rectangle_intersect (gint  x1,
                          gint  y1,
                          gint  width1,
                          gint  height1,
                          gint  x2,
                          gint  y2,
                          gint  width2,
                          gint  height2,
                          gint *dest_x,
                          gint *dest_y,
                          gint *dest_width,
                          gint *dest_height)
{
  gint d_x, d_y;
  gint d_w, d_h;

  d_x = MAX (x1, x2);
  d_y = MAX (y1, y2);
  d_w = MIN (x1 + width1,  x2 + width2)  - d_x;
  d_h = MIN (y1 + height1, y2 + height2) - d_y;

  if (dest_x)      *dest_x      = d_x;
  if (dest_y)      *dest_y      = d_y;
  if (dest_width)  *dest_width  = d_w;
  if (dest_height) *dest_height = d_h;

  return (d_w > 0 && d_h > 0);
}

