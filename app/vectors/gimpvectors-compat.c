/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpvectors-compat.c
 * Copyright (C) 2003 Michael Natterer <mitch@gimp.org>
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

#include "vectors-types.h"

#include "core/gimpimage.h"

#include "gimpanchor.h"
#include "gimpbezierstroke.h"
#include "gimpvectors.h"
#include "gimpvectors-compat.h"


enum
{
  GIMP_VECTORS_COMPAT_ANCHOR     = 1,
  GIMP_VECTORS_COMPAT_CONTROL    = 2,
  GIMP_VECTORS_COMPAT_NEW_STROKE = 3
};



GimpVectors *
gimp_vectors_compat_new (GimpImage              *gimage,
                         const gchar            *name,
                         GimpVectorsCompatPoint *points,
                         gint                    n_points,
                         gboolean                closed)
{
  GimpVectors *vectors;
  GimpStroke  *stroke;
  GimpCoords  *coords;
  GimpCoords  *curr_stroke;
  GimpCoords  *curr_coord;
  gint         i;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (points != NULL || n_points == 0, NULL);
  g_return_val_if_fail (n_points >= 0, NULL);

  vectors = gimp_vectors_new (gimage, name);

  coords = g_new0 (GimpCoords, n_points + 1);

  curr_stroke = curr_coord = coords;

  /*  skip the first control point, will set it later  */
  curr_coord++;

  for (i = 0; i < n_points; i++)
    {
      curr_coord->x        = points[i].x;
      curr_coord->y        = points[i].y;
      curr_coord->pressure = 1.0;
      curr_coord->xtilt    = 0.5;
      curr_coord->ytilt    = 0.5;
      curr_coord->wheel    = 0.5;

      /*  copy the first anchor to be the first control point  */
      if (curr_coord == curr_stroke + 1)
        *curr_stroke = *curr_coord;

      /*  found new stroke start  */
      if (points[i].type == GIMP_VECTORS_COMPAT_NEW_STROKE)
        {
          /*  copy the last control point to the beginning of the stroke  */
          *curr_stroke = *(curr_coord - 1);

          stroke =
            gimp_bezier_stroke_new_from_coords (curr_stroke,
                                                curr_coord - curr_stroke - 1,
                                                TRUE);
          gimp_vectors_stroke_add (vectors, stroke);
          g_object_unref (stroke);

          /*  start a new stroke  */
          curr_stroke = curr_coord - 1;

          /*  copy the first anchor to be the first control point  */
          *curr_stroke = *curr_coord;
        }

      curr_coord++;
    }

  if (closed)
    {
      /*  copy the last control point to the beginning of the stroke  */
      curr_coord--;
      *curr_stroke = *curr_coord;
    }

  stroke = gimp_bezier_stroke_new_from_coords (curr_stroke,
                                               curr_coord - curr_stroke,
                                               closed);
  gimp_vectors_stroke_add (vectors, stroke);
  g_object_unref (stroke);

  g_free (coords);

  return vectors;
}

GimpVectorsCompatPoint *
gimp_vectors_compat_get_points (GimpVectors *vectors,
                                guint32     *n_points,
                                guint32     *closed)
{
  GimpVectorsCompatPoint *points;
  GList                  *strokes;
  gint                    i;

  g_return_val_if_fail (GIMP_IS_VECTORS (vectors), NULL);
  g_return_val_if_fail (n_points != NULL, NULL);
  g_return_val_if_fail (closed != NULL, NULL);

  *n_points = 0;
  *closed   = FALSE;

  for (strokes = vectors->strokes; strokes; strokes = g_list_next (strokes))
    {
      GimpStroke *stroke = strokes->data;
      gint        n_anchors;

      if (! stroke->closed && strokes->next)
        {
          g_warning ("gimp_vectors_compat_get_points(): convert failed");

          *n_points = 0;

          return NULL;
        }

      n_anchors = g_list_length (stroke->anchors);

      if (! stroke->closed)
        n_anchors--;

      *n_points += n_anchors;

      if (! strokes->next)
        *closed = stroke->closed;
    }

  points = g_new0 (GimpVectorsCompatPoint, *n_points);

  i = 0;

  for (strokes = vectors->strokes;
       strokes;
       strokes = g_list_next (strokes))
    {
      GimpStroke *stroke = strokes->data;
      GList      *anchors;

      for (anchors = stroke->anchors;
           anchors;
           anchors = g_list_next (anchors))
        {
          GimpAnchor *anchor = anchors->data;

          /*  skip the first anchor, will add it at the end if needed  */
          if (! anchors->prev)
            continue;

          switch (anchor->type)
            {
            case GIMP_ANCHOR_ANCHOR:
              if (strokes->prev && anchors->prev == stroke->anchors)
                points[i].type = GIMP_VECTORS_COMPAT_NEW_STROKE;
              else
                points[i].type = GIMP_VECTORS_COMPAT_ANCHOR;
              break;

            case GIMP_ANCHOR_CONTROL:
              points[i].type = GIMP_VECTORS_COMPAT_CONTROL;
              break;
            }

          points[i].x = anchor->position.x;
          points[i].y = anchor->position.y;

          i++;

          /*  write the skipped control point  */
          if (! anchors->next && stroke->closed)
            {
              anchor = GIMP_ANCHOR (stroke->anchors->data);

              points[i].type = GIMP_VECTORS_COMPAT_CONTROL;
              points[i].x    = anchor->position.x;
              points[i].y    = anchor->position.y;

              i++;
            }
        }
    }

  return points;
}
