/* LIBGIMP - The GIMP Library 
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball                
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU  
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GIMP_COLOR_TYPES_H__
#define __GIMP_COLOR_TYPES_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* For information look into the C source or the html documentation */


/*  usually we don't keep the structure definitions in the types file
 *  but GimpRGB appears in too many header files...
 */

typedef struct _GimpRGB GimpRGB;

struct _GimpRGB
{
  gdouble r, g, b, a;
};


typedef struct _GimpHSV GimpHSV;

struct _GimpHSV
{
  gdouble h, s, v, a;
};


typedef void (* GimpRenderFunc)   (gdouble   x,
                                   gdouble   y,
                                   GimpRGB  *color,
                                   gpointer  data);
typedef void (* GimpPutPixelFunc) (gint      x,
                                   gint      y,
                                   GimpRGB  *color,
                                   gpointer  data);
typedef void (* GimpProgressFunc) (gint      min,
                                   gint      max,
                                   gint      current,
                                   gpointer  data);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* __GIMP_COLOR_TYPES_H__ */
