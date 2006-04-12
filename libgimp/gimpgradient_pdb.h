/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-2003 Peter Mattis and Spencer Kimball
 *
 * gimpgradient_pdb.h
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

/* NOTE: This file is autogenerated by pdbgen.pl */

#ifndef __GIMP_GRADIENT_PDB_H__
#define __GIMP_GRADIENT_PDB_H__

G_BEGIN_DECLS

/* For information look into the C source or the html documentation */


gchar*   gimp_gradient_new                                 (const gchar               *name);
gchar*   gimp_gradient_duplicate                           (const gchar               *name);
gboolean gimp_gradient_is_editable                         (const gchar               *name);
gchar*   gimp_gradient_rename                              (const gchar               *name,
                                                            const gchar               *new_name);
gboolean gimp_gradient_delete                              (const gchar               *name);
gboolean gimp_gradient_get_uniform_samples                 (const gchar               *name,
                                                            gint                       num_samples,
                                                            gboolean                   reverse,
                                                            gint                      *num_color_samples,
                                                            gdouble                  **color_samples);
gboolean gimp_gradient_get_custom_samples                  (const gchar               *name,
                                                            gint                       num_samples,
                                                            const gdouble             *positions,
                                                            gboolean                   reverse,
                                                            gint                      *num_color_samples,
                                                            gdouble                  **color_samples);
gboolean gimp_gradient_segment_get_left_color              (const gchar               *name,
                                                            gint                       segment,
                                                            GimpRGB                   *color,
                                                            gdouble                   *opacity);
gboolean gimp_gradient_segment_set_left_color              (const gchar               *name,
                                                            gint                       segment,
                                                            const GimpRGB             *color,
                                                            gdouble                    opacity);
gboolean gimp_gradient_segment_get_right_color             (const gchar               *name,
                                                            gint                       segment,
                                                            GimpRGB                   *color,
                                                            gdouble                   *opacity);
gboolean gimp_gradient_segment_set_right_color             (const gchar               *name,
                                                            gint                       segment,
                                                            const GimpRGB             *color,
                                                            gdouble                    opacity);
gboolean gimp_gradient_segment_get_left_pos                (const gchar               *name,
                                                            gint                       segment,
                                                            gdouble                   *pos);
gboolean gimp_gradient_segment_set_left_pos                (const gchar               *name,
                                                            gint                       segment,
                                                            gdouble                    pos,
                                                            gdouble                   *final_pos);
gboolean gimp_gradient_segment_get_middle_pos              (const gchar               *name,
                                                            gint                       segment,
                                                            gdouble                   *pos);
gboolean gimp_gradient_segment_set_middle_pos              (const gchar               *name,
                                                            gint                       segment,
                                                            gdouble                    pos,
                                                            gdouble                   *final_pos);
gboolean gimp_gradient_segment_get_right_pos               (const gchar               *name,
                                                            gint                       segment,
                                                            gdouble                   *pos);
gboolean gimp_gradient_segment_set_right_pos               (const gchar               *name,
                                                            gint                       segment,
                                                            gdouble                    pos,
                                                            gdouble                   *final_pos);
gboolean gimp_gradient_segment_get_blending_function       (const gchar               *name,
                                                            gint                       segment,
                                                            GimpGradientSegmentType   *blend_func);
gboolean gimp_gradient_segment_get_coloring_type           (const gchar               *name,
                                                            gint                       segment,
                                                            GimpGradientSegmentColor  *coloring_type);
gboolean gimp_gradient_segment_range_set_blending_function (const gchar               *name,
                                                            gint                       start_segment,
                                                            gint                       end_segment,
                                                            GimpGradientSegmentType    blending_function);
gboolean gimp_gradient_segment_range_set_coloring_type     (const gchar               *name,
                                                            gint                       start_segment,
                                                            gint                       end_segment,
                                                            GimpGradientSegmentColor   coloring_type);
gboolean gimp_gradient_segment_range_flip                  (const gchar               *name,
                                                            gint                       start_segment,
                                                            gint                       end_segment);
gboolean gimp_gradient_segment_range_replicate             (const gchar               *name,
                                                            gint                       start_segment,
                                                            gint                       end_segment,
                                                            gint                       replicate_times);
gboolean gimp_gradient_segment_range_split_midpoint        (const gchar               *name,
                                                            gint                       start_segment,
                                                            gint                       end_segment);
gboolean gimp_gradient_segment_range_split_uniform         (const gchar               *name,
                                                            gint                       start_segment,
                                                            gint                       end_segment,
                                                            gint                       split_parts);
gboolean gimp_gradient_segment_range_delete                (const gchar               *name,
                                                            gint                       start_segment,
                                                            gint                       end_segment);
gboolean gimp_gradient_segment_range_redistribute_handles  (const gchar               *name,
                                                            gint                       start_segment,
                                                            gint                       end_segment);
gboolean gimp_gradient_segment_range_blend_colors          (const gchar               *name,
                                                            gint                       start_segment,
                                                            gint                       end_segment);
gboolean gimp_gradient_segment_range_blend_opacity         (const gchar               *name,
                                                            gint                       start_segment,
                                                            gint                       end_segment);
gdouble  gimp_gradient_segment_range_move                  (const gchar               *name,
                                                            gint                       start_segment,
                                                            gint                       end_segment,
                                                            gdouble                    delta,
                                                            gboolean                   control_compress);


G_END_DECLS

#endif /* __GIMP_GRADIENT_PDB_H__ */
