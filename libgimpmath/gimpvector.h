/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * The gimp_vector* functions were taken from:
 * GCK - The General Convenience Kit
 * Copyright (C) 1996 Tom Bech
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GIMP_VECTOR_H__
#define __GIMP_VECTOR_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* For information look into the C source or the html documentation */


/* Two dimensional vector functions */
/* ================================ */

gdouble     gimp_vector2_inner_product (GimpVector2 *vector1,
					GimpVector2 *vector2);
GimpVector2 gimp_vector2_cross_product (GimpVector2 *vector1,
					GimpVector2 *vector2);
gdouble     gimp_vector2_length        (GimpVector2 *vector);
void        gimp_vector2_normalize     (GimpVector2 *vector);
void        gimp_vector2_mul           (GimpVector2 *vector,
					gdouble      factor);
void        gimp_vector2_sub           (GimpVector2 *result,
					GimpVector2 *vector1,
					GimpVector2 *vector2);
void        gimp_vector2_set           (GimpVector2 *vector,
					gdouble      x,
					gdouble      y);
void        gimp_vector2_add           (GimpVector2 *result,
					GimpVector2 *vector1,
					GimpVector2 *vector2);
void        gimp_vector2_neg           (GimpVector2 *vector);
void        gimp_vector2_rotate        (GimpVector2 *vector,
					gdouble      alpha);

/* Three dimensional vector functions */
/* ================================== */

gdouble     gimp_vector3_inner_product (GimpVector3 *vector1,
					GimpVector3 *vector2);
GimpVector3 gimp_vector3_cross_product (GimpVector3 *vector1,
					GimpVector3 *vector2);
gdouble     gimp_vector3_length        (GimpVector3 *vector);
void        gimp_vector3_normalize     (GimpVector3 *vector);
void        gimp_vector3_mul           (GimpVector3 *vector,
					gdouble      factor);
void        gimp_vector3_sub           (GimpVector3 *result,
					GimpVector3 *vector1,
					GimpVector3 *vector2);
void        gimp_vector3_set           (GimpVector3 *vector,
					gdouble      x,
					gdouble      y,
					gdouble      z);
void        gimp_vector3_add           (GimpVector3 *result,
					GimpVector3 *vector1,
					GimpVector3 *vector2);
void        gimp_vector3_neg           (GimpVector3 *vector);
void        gimp_vector3_rotate        (GimpVector3 *vector,
					gdouble      alpha,
					gdouble      beta,
					gdouble      gamma);

/* 2d <-> 3d Vector projection functions */
/* ===================================== */

void        gimp_vector_2d_to_3d       (gint         sx,
					gint         sy,
					gint         w,
					gint         h,
					gint         x,
					gint         y,
					GimpVector3 *vp,
					GimpVector3 *p);

void        gimp_vector_3d_to_2d       (gint         sx,
					gint         sy,
					gint         w,
					gint         h,
					gdouble     *x,
					gdouble     *y,
					GimpVector3 *vp,
					GimpVector3 *p);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* __GIMP_VECTOR_H__ */
