/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpbezierstroke.c
 * Copyright (C) 2002 Simon Budig  <simon@gimp.org>
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

#include "gimpanchor.h"
#include "gimpbezierstroke.h"

#include "gimpcoordmath.h"


/* local prototypes */

static void gimp_bezier_stroke_class_init (GimpBezierStrokeClass *klass);
static void gimp_bezier_stroke_init       (GimpBezierStroke      *bezier_stroke);
static gdouble gimp_bezier_stroke_nearest_point_get (const GimpStroke *stroke,
                                                     const GimpCoords *coord,
                                                     const gdouble     precision,
                                                     GimpCoords       *ret_point,
                                                     GimpAnchor      **ret_segment_start,
                                                     gdouble          *ret_pos);
static gdouble
       gimp_bezier_stroke_segment_nearest_point_get (const GimpCoords  *beziercoords,
                                                     const GimpCoords  *coord,
                                                     const gdouble      precision,
                                                     GimpCoords        *ret_point,
                                                     gdouble           *ret_pos,
                                                     gint               depth);
static void gimp_bezier_stroke_anchor_move_relative (GimpStroke        *stroke,
                                                     GimpAnchor        *anchor,
                                                     const GimpCoords  *deltacoord,
                                                     GimpAnchorFeatureType feature);
static void gimp_bezier_stroke_anchor_move_absolute (GimpStroke        *stroke,
                                                     GimpAnchor        *anchor,
                                                     const GimpCoords  *coord,
                                                     GimpAnchorFeatureType feature);
static void gimp_bezier_stroke_anchor_convert   (GimpStroke            *stroke,
                                                 GimpAnchor            *anchor,
                                                 GimpAnchorFeatureType  feature);
static void gimp_bezier_stroke_anchor_delete        (GimpStroke        *stroke,
                                                     GimpAnchor        *anchor);
static gboolean gimp_bezier_stroke_point_is_movable
                                                (GimpStroke            *stroke,
                                                 GimpAnchor            *predec,
                                                 gdouble                position);
static void gimp_bezier_stroke_point_move_relative
                                              (GimpStroke            *stroke,
                                               GimpAnchor            *predec,
                                               gdouble                position,
                                               const GimpCoords      *deltacoord,
                                               GimpAnchorFeatureType  feature);
static void gimp_bezier_stroke_point_move_absolute
                                              (GimpStroke            *stroke,
                                               GimpAnchor            *predec,
                                               gdouble                position,
                                               const GimpCoords      *coord,
                                               GimpAnchorFeatureType  feature);

static void gimp_bezier_stroke_close          (GimpStroke            *stroke);

static GimpStroke * gimp_bezier_stroke_open     (GimpStroke        *stroke,
                                                 GimpAnchor        *end_anchor);
static gboolean gimp_bezier_stroke_anchor_is_insertable
                                                    (GimpStroke        *stroke,
                                                     GimpAnchor        *predec,
                                                     gdouble            position);
static GimpAnchor * gimp_bezier_stroke_anchor_insert (GimpStroke       *stroke,
                                                      GimpAnchor       *predec,
                                                      gdouble           position);
static gboolean gimp_bezier_stroke_is_extendable    (GimpStroke      *stroke,
                                                     GimpAnchor      *neighbor);
static gboolean gimp_bezier_stroke_connect_stroke   (GimpStroke      *stroke,
                                                     GimpAnchor      *anchor,
                                                     GimpStroke      *extension,
                                                     GimpAnchor      *neighbor);
static GArray *   gimp_bezier_stroke_interpolate (const GimpStroke  *stroke,
                                                  const gdouble      precision,
                                                  gboolean          *closed);

static void gimp_bezier_stroke_finalize   (GObject               *object);


static GList * gimp_bezier_stroke_get_anchor_listitem (GList     *list);

static void gimp_bezier_coords_subdivide  (const GimpCoords      *beziercoords,
                                           const gdouble          precision,
                                           GArray               **ret_coords);
static void gimp_bezier_coords_subdivide2 (const GimpCoords      *beziercoords,
                                           const gdouble          precision,
                                           GArray               **ret_coords,
                                           gint                   depth);

static gboolean gimp_bezier_coords_is_straight (const GimpCoords *beziercoords,
                                                const gdouble     precision);


/*  private variables  */

static GimpStrokeClass *parent_class = NULL;


GType
gimp_bezier_stroke_get_type (void)
{
  static GType bezier_stroke_type = 0;

  if (! bezier_stroke_type)
    {
      static const GTypeInfo bezier_stroke_info =
      {
        sizeof (GimpBezierStrokeClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gimp_bezier_stroke_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data     */
        sizeof (GimpBezierStroke),
        0,              /* n_preallocs    */
        (GInstanceInitFunc) gimp_bezier_stroke_init,
      };

      bezier_stroke_type = g_type_register_static (GIMP_TYPE_STROKE,
                                                   "GimpBezierStroke",
                                                   &bezier_stroke_info, 0);
    }

  return bezier_stroke_type;
}

static void
gimp_bezier_stroke_class_init (GimpBezierStrokeClass *klass)
{
  GObjectClass    *object_class;
  GimpStrokeClass *stroke_class;

  object_class = G_OBJECT_CLASS (klass);
  stroke_class = GIMP_STROKE_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize             = gimp_bezier_stroke_finalize;

  stroke_class->nearest_point_get    = gimp_bezier_stroke_nearest_point_get;
  stroke_class->anchor_move_relative = gimp_bezier_stroke_anchor_move_relative;
  stroke_class->anchor_move_absolute = gimp_bezier_stroke_anchor_move_absolute;
  stroke_class->anchor_convert       = gimp_bezier_stroke_anchor_convert;
  stroke_class->anchor_delete        = gimp_bezier_stroke_anchor_delete;
  stroke_class->point_is_movable     = gimp_bezier_stroke_point_is_movable;
  stroke_class->point_move_relative  = gimp_bezier_stroke_point_move_relative;
  stroke_class->point_move_absolute  = gimp_bezier_stroke_point_move_absolute;
  stroke_class->close                = gimp_bezier_stroke_close;
  stroke_class->open                 = gimp_bezier_stroke_open;
  stroke_class->anchor_is_insertable = gimp_bezier_stroke_anchor_is_insertable;
  stroke_class->anchor_insert        = gimp_bezier_stroke_anchor_insert;
  stroke_class->is_extendable        = gimp_bezier_stroke_is_extendable;
  stroke_class->extend               = gimp_bezier_stroke_extend;
  stroke_class->connect_stroke       = gimp_bezier_stroke_connect_stroke;
  stroke_class->interpolate          = gimp_bezier_stroke_interpolate;
}

static void
gimp_bezier_stroke_init (GimpBezierStroke *bezier_stroke)
{
  /* pass */
}

static void
gimp_bezier_stroke_finalize (GObject *object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}


/* Bezier specific functions */

GimpStroke *
gimp_bezier_stroke_new (void)
{
  GimpBezierStroke *bezier_stroke;
  GimpStroke       *stroke;

  bezier_stroke = g_object_new (GIMP_TYPE_BEZIER_STROKE, NULL);

  stroke = GIMP_STROKE (bezier_stroke);

  stroke->anchors = NULL;
  stroke->closed = FALSE;

  return stroke;
}


GimpStroke *
gimp_bezier_stroke_new_from_coords (const GimpCoords *coords,
                                    gint              n_coords,
                                    gboolean          closed)
{
  GimpBezierStroke *bezier_stroke;
  GimpStroke       *stroke;
  GimpAnchor       *last_anchor;
  gint              count;

  g_return_val_if_fail (coords != NULL, NULL);
  g_return_val_if_fail (n_coords >= 3, NULL);
  g_return_val_if_fail ((n_coords % 3) == 0, NULL);

  stroke = gimp_bezier_stroke_new ();

  bezier_stroke = GIMP_BEZIER_STROKE (stroke);

  last_anchor = NULL;

  count = 0;

  while (count < n_coords)
    last_anchor = gimp_bezier_stroke_extend (stroke,
                                             &coords[count++],
                                             last_anchor,
                                             EXTEND_SIMPLE);

  stroke->closed = closed ? TRUE : FALSE;

  return stroke;
}

static void
gimp_bezier_stroke_anchor_delete (GimpStroke        *stroke,
                                  GimpAnchor        *anchor)
{
  GList *list, *list2;
  gint i;

  /* Anchors always are surrounded by two handles that have to
   * be deleted too */

  g_return_if_fail (GIMP_IS_BEZIER_STROKE (stroke));
  g_return_if_fail (anchor && anchor->type == GIMP_ANCHOR_ANCHOR);

  list2 = g_list_find (stroke->anchors, anchor);
  list = g_list_previous(list2);

  for (i=0; i < 3; i++)
    {
      g_return_if_fail (list != NULL);
      list2 = g_list_next (list);
      gimp_anchor_free (GIMP_ANCHOR (list->data));
      stroke->anchors = g_list_delete_link (stroke->anchors, list);
      list = list2;
    }
}

static GimpStroke *
gimp_bezier_stroke_open (GimpStroke *stroke,
                         GimpAnchor *end_anchor)
{
  GList *list, *list2;
  GimpStroke *new_stroke = NULL;

  g_return_val_if_fail (GIMP_IS_BEZIER_STROKE (stroke), FALSE);
  g_return_val_if_fail (end_anchor &&
                        end_anchor->type == GIMP_ANCHOR_ANCHOR, FALSE);

  list = g_list_find (stroke->anchors, end_anchor);

  g_return_val_if_fail (list != NULL && list->next != NULL, FALSE);

  list = g_list_next (list);  /* protect the handle... */

  list2 = list->next;
  list->next = NULL;

  if (list2 != NULL)
    {
      list2->prev = NULL;

      if (stroke->closed)
        {
          stroke->anchors = g_list_concat (list2, stroke->anchors);
        }
      else
        {
          new_stroke = gimp_bezier_stroke_new ();
          new_stroke->anchors = list2;
        }
    }
  stroke->closed = FALSE;
  return new_stroke;
}

static gboolean
gimp_bezier_stroke_anchor_is_insertable (GimpStroke *stroke,
                                         GimpAnchor *predec,
                                         gdouble     position)
{
  g_return_val_if_fail (GIMP_IS_BEZIER_STROKE (stroke), FALSE);

  return (g_list_find (stroke->anchors, predec) != NULL);
}


static GimpAnchor *
gimp_bezier_stroke_anchor_insert (GimpStroke *stroke,
                                  GimpAnchor *predec,
                                  gdouble     position)
{
  GList *segment_start, *list, *list2;
  GimpCoords subdivided[8];
  GimpCoords beziercoords[4];
  gint i;

  g_return_val_if_fail (GIMP_IS_BEZIER_STROKE (stroke), NULL);
  g_return_val_if_fail (predec->type == GIMP_ANCHOR_ANCHOR, NULL);

  segment_start = g_list_find (stroke->anchors, predec);

  if (! segment_start)
    return NULL;

  list = segment_start;

  for (i=0; i <= 3; i++)
    {
      beziercoords[i] = GIMP_ANCHOR (list->data)->position;
      list = g_list_next (list);
      if (!list)
        list = stroke->anchors;
    }

  subdivided[0] = beziercoords[0];
  subdivided[6] = beziercoords[3];

  gimp_bezier_coords_mix (1-position, &(beziercoords[0]),
                          position,   &(beziercoords[1]),
                          &(subdivided[1]));

  gimp_bezier_coords_mix (1-position, &(beziercoords[1]),
                          position,   &(beziercoords[2]),
                          &(subdivided[7]));

  gimp_bezier_coords_mix (1-position, &(beziercoords[2]),
                          position,   &(beziercoords[3]),
                          &(subdivided[5]));

  gimp_bezier_coords_mix (1-position, &(subdivided[1]),
                          position,   &(subdivided[7]),
                          &(subdivided[2]));

  gimp_bezier_coords_mix (1-position, &(subdivided[7]),
                          position,   &(subdivided[5]),
                          &(subdivided[4]));

  gimp_bezier_coords_mix (1-position, &(subdivided[2]),
                          position,   &(subdivided[4]),
                          &(subdivided[3]));

  /* subdivided 0-6 contains the bezier segement subdivided at <position> */

  list = segment_start;

  for (i=0; i <= 6; i++)
    {
      if (i >= 2 && i <= 4)
        {
          list2 = g_list_append (NULL, 
                                 gimp_anchor_new ((i == 3 ?
                                                    GIMP_ANCHOR_ANCHOR:
                                                    GIMP_ANCHOR_CONTROL),
                                                  &(subdivided[i])));
          /* insert it *before* list manually. */
          list2->next = list;
          list2->prev = list->prev;
          if (list->prev)
            list->prev->next = list2;
          list->prev = list2;
          
          list = list2;

          if (i == 3)
            segment_start = list;

        }
      else
        {
          GIMP_ANCHOR (list->data)->position = subdivided[i];
        }

      list = g_list_next (list);
      if (!list)
        list = stroke->anchors;

    }
  
  stroke->anchors = g_list_first (stroke->anchors);

  return GIMP_ANCHOR (segment_start->data);
}


static gboolean
gimp_bezier_stroke_point_is_movable (GimpStroke *stroke,
                                     GimpAnchor *predec,
                                     gdouble     position)
{
  g_return_val_if_fail (GIMP_IS_BEZIER_STROKE (stroke), FALSE);

  return (g_list_find (stroke->anchors, predec) != NULL);
}


static void
gimp_bezier_stroke_point_move_relative (GimpStroke            *stroke,
                                        GimpAnchor            *predec,
                                        gdouble                position,
                                        const GimpCoords      *deltacoord,
                                        GimpAnchorFeatureType  feature)
{
  GimpCoords offsetcoords[2];
  GList      *segment_start, *list;
  gint        i;
  gdouble feel_good;

  g_return_if_fail (GIMP_IS_BEZIER_STROKE (stroke));

  segment_start = g_list_find (stroke->anchors, predec);

  g_return_if_fail (segment_start != NULL);

  if (position <= 0.5)
    feel_good = (pow(2 * position, 3)) / 2;
  else
    feel_good = (1 - pow((1-position)*2, 3)) / 2 + 0.5;

  gimp_bezier_coords_scale ((1-feel_good)/(3*position*
                                           (1-position)*(1-position)),
                            deltacoord,
                            &(offsetcoords[0]));
  gimp_bezier_coords_scale (feel_good/(3*position*position*(1-position)),
                            deltacoord,
                            &(offsetcoords[1]));

  list = segment_start;
  list = g_list_next (list);
  if (!list)
    list = stroke->anchors;

  for (i=0; i <= 1; i++)
    {
      gimp_stroke_anchor_move_relative (stroke, GIMP_ANCHOR (list->data),
                                        &(offsetcoords[i]), feature);
      list = g_list_next (list);
      if (!list)
        list = stroke->anchors;
    }
}


static void
gimp_bezier_stroke_point_move_absolute (GimpStroke            *stroke,
                                        GimpAnchor            *predec,
                                        gdouble                position,
                                        const GimpCoords      *coord,
                                        GimpAnchorFeatureType  feature)
{
  GimpCoords  deltacoord;
  GimpCoords  tmp1, tmp2, abs_pos;
  GimpCoords  beziercoords[4];
  GList      *segment_start, *list;
  gint        i;

  g_return_if_fail (GIMP_IS_BEZIER_STROKE (stroke));

  segment_start = g_list_find (stroke->anchors, predec);

  g_return_if_fail (segment_start != NULL);

  list = segment_start;

  for (i=0; i <= 3; i++)
    {
      beziercoords[i] = GIMP_ANCHOR (list->data)->position;
      list = g_list_next (list);
      if (!list)
        list = stroke->anchors;
    }

  gimp_bezier_coords_mix ((1-position)*(1-position)*(1-position), &(beziercoords[0]),
                          3*(1-position)*(1-position)*position, &(beziercoords[1]),
                          &tmp1);
  gimp_bezier_coords_mix (3*(1-position)*position*position, &(beziercoords[2]),
                          position*position*position, &(beziercoords[3]),
                          &tmp2);
  gimp_bezier_coords_add (&tmp1, &tmp2, &abs_pos);

  gimp_bezier_coords_difference (coord, &abs_pos, &deltacoord);

  gimp_bezier_stroke_point_move_relative (stroke, predec, position,
                                          &deltacoord, feature);
}

static void
gimp_bezier_stroke_close (GimpStroke *stroke)
{
  GList *start, *end;
  GimpAnchor *anchor;

  g_return_if_fail (stroke->anchors != NULL);

  stroke->closed = TRUE;

  start = g_list_first (stroke->anchors);
  end = g_list_last (stroke->anchors);

  g_return_if_fail (start->next != NULL && end->prev != NULL);

  if (start->next != end->prev)
    {
      if (gimp_bezier_coords_equal (&(GIMP_ANCHOR (start->next->data)->position),
                                    &(GIMP_ANCHOR (start->data)->position)) &&
          gimp_bezier_coords_equal (&(GIMP_ANCHOR (start->data)->position),
                                    &(GIMP_ANCHOR (end->data)->position)) &&
          gimp_bezier_coords_equal (&(GIMP_ANCHOR (end->data)->position),
                                    &(GIMP_ANCHOR (end->prev->data)->position)))
        {
          /* redundant segment */

          anchor = GIMP_ANCHOR (stroke->anchors->data);
          stroke->anchors = g_list_delete_link (stroke->anchors,
                                                stroke->anchors);
          gimp_anchor_free (anchor);

          anchor = GIMP_ANCHOR (stroke->anchors->data);
          stroke->anchors = g_list_delete_link (stroke->anchors,
                                                stroke->anchors);
          gimp_anchor_free (anchor);

          anchor = GIMP_ANCHOR (stroke->anchors->data);
          stroke->anchors = g_list_delete_link (stroke->anchors,
                                                stroke->anchors);

          end = g_list_last (stroke->anchors);
          gimp_anchor_free (GIMP_ANCHOR (end->data));
          end->data = anchor;
        }
    }
}

static gdouble
gimp_bezier_stroke_nearest_point_get (const GimpStroke     *stroke,
                                      const GimpCoords     *coord,
                                      const gdouble         precision,
                                      GimpCoords           *ret_point,
                                      GimpAnchor          **ret_segment_start,
                                      gdouble              *ret_pos)
{
  gdouble     min_dist, dist, pos;
  GimpCoords  point;
  GimpCoords  segmentcoords[4];
  GList      *anchorlist;
  GimpAnchor *segment_start, *anchor;
  gint        count;

  g_return_val_if_fail (GIMP_IS_BEZIER_STROKE (stroke), - 1.0);

  if (!stroke->anchors)
    return -1.0;

  count = 0;
  min_dist = -1;

  for (anchorlist = stroke->anchors;
       anchorlist && GIMP_ANCHOR (anchorlist->data)->type != GIMP_ANCHOR_ANCHOR;
       anchorlist = g_list_next (anchorlist));

  segment_start = anchorlist->data;

  for ( ; anchorlist; anchorlist = g_list_next (anchorlist))
    {
      anchor = anchorlist->data;

      segmentcoords[count] = anchor->position;
      count++;

      if (count == 4)
        {
          dist = gimp_bezier_stroke_segment_nearest_point_get (segmentcoords,
                                                               coord, precision,
                                                               &point, &pos,
                                                               10);

          if (dist < min_dist || min_dist < 0)
            {
              min_dist = dist;
              if (ret_pos)
                *ret_pos = pos;
              if (ret_point)
                *ret_point = point;
              if (ret_segment_start)
                *ret_segment_start = segment_start;
            }
          segment_start = anchorlist->data;
          segmentcoords[0] = segmentcoords[3];
          count = 1;
        }
    }

  if (stroke->closed && stroke->anchors)
    {
      anchorlist = stroke->anchors;

      while (count < 3)
        {
          segmentcoords[count] = GIMP_ANCHOR (anchorlist->data)->position;
          count++;
        }
      anchorlist = g_list_next (anchorlist);
      if (anchorlist)
        segmentcoords[3] = GIMP_ANCHOR (anchorlist->data)->position;

      dist = gimp_bezier_stroke_segment_nearest_point_get (segmentcoords,
                                                           coord, precision,
                                                           &point, &pos,
                                                           10);

      if (dist < min_dist || min_dist < 0)
        {
          min_dist = dist;
          if (ret_pos)
            *ret_pos = pos;
          if (ret_point)
            *ret_point = point;
          if (ret_segment_start)
            *ret_segment_start = segment_start;
        }
    }

  return min_dist;
}


static gdouble
gimp_bezier_stroke_segment_nearest_point_get (const GimpCoords  *beziercoords,
                                              const GimpCoords  *coord,
                                              const gdouble      precision,
                                              GimpCoords        *ret_point,
                                              gdouble           *ret_pos,
                                              gint               depth)
{
  /*
   * beziercoords has to contain four GimpCoords with the four control points
   * of the bezier segment. We subdivide it at the parameter 0.5.
   */

  GimpCoords subdivided[8];
  gdouble dist1, dist2;
  GimpCoords point1, point2;
  gdouble pos1, pos2;

  gimp_bezier_coords_difference (&beziercoords[1], &beziercoords[0], &point1);
  gimp_bezier_coords_difference (&beziercoords[3], &beziercoords[2], &point2);

  if (!depth || (gimp_bezier_coords_is_straight (beziercoords, precision)
                 && gimp_bezier_coords_length2 (&point1) < precision 
                 && gimp_bezier_coords_length2 (&point2) < precision))
    {
      GimpCoords line, dcoord;
      gdouble length2, scalar;
      gint i;
      
      gimp_bezier_coords_difference (&(beziercoords[3]),
                                     &(beziercoords[0]),
                                     &line);

      gimp_bezier_coords_difference (coord,
                                     &(beziercoords[0]),
                                     &dcoord);

      length2 = gimp_bezier_coords_scalarprod (&line, &line);
      scalar = gimp_bezier_coords_scalarprod (&line, &dcoord) / length2;

      scalar = CLAMP (scalar, 0.0, 1.0);

      /* lines look the same as bezier curves where the handles
       * sit on the anchors, however, they are parametrized
       * differently. Hence we have to do some weird approximation.  */

      pos1 = pos2 = 0.5;

      for (i=0; i <= 15; i++)
        {
          pos2 *= 0.5;
          if (3*pos1*pos1*(1-pos1) + pos1*pos1*pos1 < scalar)
            pos1 += pos2;
          else
            pos1 -= pos2;
        }

      *ret_pos = pos1;

      gimp_bezier_coords_mix (1.0, &(beziercoords[0]),
                              scalar, &line,
                              ret_point);

      gimp_bezier_coords_difference (coord, ret_point, &dcoord);

      return gimp_bezier_coords_length (&dcoord);
    }

  /* ok, we have to subdivide */

  subdivided[0] = beziercoords[0];
  subdivided[6] = beziercoords[3];

  if (!depth) g_printerr ("Hit rekursion depth limit!\n");

  gimp_bezier_coords_average (&(beziercoords[0]), &(beziercoords[1]),
                              &(subdivided[1]));

  gimp_bezier_coords_average (&(beziercoords[1]), &(beziercoords[2]),
                              &(subdivided[7]));

  gimp_bezier_coords_average (&(beziercoords[2]), &(beziercoords[3]),
                              &(subdivided[5]));

  gimp_bezier_coords_average (&(subdivided[1]), &(subdivided[7]),
                              &(subdivided[2]));

  gimp_bezier_coords_average (&(subdivided[7]), &(subdivided[5]),
                              &(subdivided[4]));

  gimp_bezier_coords_average (&(subdivided[2]), &(subdivided[4]),
                              &(subdivided[3]));

  /*
   * We now have the coordinates of the two bezier segments in
   * subdivided [0-3] and subdivided [3-6]
   */

  dist1 = gimp_bezier_stroke_segment_nearest_point_get (&(subdivided[0]),
                                                        coord, precision,
                                                        &point1, &pos1,
                                                        depth - 1);

  dist2 = gimp_bezier_stroke_segment_nearest_point_get (&(subdivided[3]),
                                                        coord, precision,
                                                        &point2, &pos2,
                                                        depth - 1);

  if (dist1 <= dist2)
    {
      *ret_point = point1;
      *ret_pos = 0.5 * pos1;
      return dist1;
    }
  else
    {
      *ret_point = point2;
      *ret_pos = 0.5 + 0.5 * pos2;
      return dist2;
    }
}


static gboolean
gimp_bezier_stroke_is_extendable (GimpStroke *stroke,
                                  GimpAnchor *neighbor)
{
  GimpBezierStroke *bezier_stroke;
  GList            *listneighbor;
  gint              loose_end;

  g_return_val_if_fail (GIMP_IS_BEZIER_STROKE (stroke), FALSE);
  bezier_stroke = GIMP_BEZIER_STROKE (stroke);

  if (stroke->closed)
    return FALSE;

  if (stroke->anchors == NULL)
    return TRUE;
  
  /* assure that there is a neighbor specified */
  g_return_val_if_fail (neighbor != NULL, FALSE);

  loose_end = 0;
  listneighbor = g_list_last (stroke->anchors);

  /* Check if the neighbor is at an end of the control points */
  if (listneighbor->data == neighbor)
    {
      loose_end = 1;
    }
  else
    {
      listneighbor = g_list_first (stroke->anchors);
      if (listneighbor->data == neighbor)
        {
          loose_end = -1;
        }
      else
        {
          /*
           * it isnt. if we are on a handle go to the nearest
           * anchor and see if we can find an end from it.
           * Yes, this is tedious.
           */

          listneighbor = g_list_find (stroke->anchors, neighbor);

          if (listneighbor && neighbor->type == GIMP_ANCHOR_CONTROL)
            {
              if (listneighbor->prev &&
                  GIMP_ANCHOR (listneighbor->prev->data)->type == GIMP_ANCHOR_ANCHOR)
                {
                  listneighbor = listneighbor->prev;
                }
              else if (listneighbor->next &&
                       GIMP_ANCHOR (listneighbor->next->data)->type == GIMP_ANCHOR_ANCHOR)
                {
                  listneighbor = listneighbor->next;
                }
              else
                {
                  loose_end = 0;
                  listneighbor = NULL;
                }
            }

          if (listneighbor)
            /* we found a suitable ANCHOR_ANCHOR now, lets
             * search for its loose end.
             */
            {
              if (listneighbor->prev &&
                  listneighbor->prev->prev == NULL)
                {
                  loose_end = -1;
                  listneighbor = listneighbor->prev;
                }
              else if (listneighbor->next &&
                       listneighbor->next->next == NULL)
                {
                  loose_end = 1;
                  listneighbor = listneighbor->next;
                }
            }
        }
    }

  return (loose_end != 0);
}

GimpAnchor *
gimp_bezier_stroke_extend (GimpStroke           *stroke,
                           const GimpCoords     *coords,
                           GimpAnchor           *neighbor,
                           GimpVectorExtendMode  extend_mode)
{
  GimpAnchor       *anchor = NULL;
  GimpBezierStroke *bezier_stroke;
  GList            *listneighbor;
  gint              loose_end, control_count;

  g_return_val_if_fail (GIMP_IS_BEZIER_STROKE (stroke), NULL);
  bezier_stroke = GIMP_BEZIER_STROKE (stroke);

  g_return_val_if_fail (!stroke->closed, NULL);

  if (stroke->anchors == NULL)
    {
      /* assure that there is no neighbor specified */
      g_return_val_if_fail (neighbor == NULL, NULL);

      anchor = gimp_anchor_new (GIMP_ANCHOR_CONTROL, coords);

      stroke->anchors = g_list_append (stroke->anchors, anchor);

      switch (extend_mode)
        {
        case EXTEND_SIMPLE:
          break;

        case EXTEND_EDITABLE:
          anchor = gimp_bezier_stroke_extend (stroke,
                                              coords, anchor,
                                              EXTEND_SIMPLE);

          /* we return the GIMP_ANCHOR_ANCHOR */
          gimp_bezier_stroke_extend (stroke,
                                     coords, anchor,
                                     EXTEND_SIMPLE);

          break;

        default:
          anchor = NULL;
        }

      return anchor;
    }
  else
    {
      /* assure that there is a neighbor specified */
      g_return_val_if_fail (neighbor != NULL, NULL);

      loose_end = 0;
      listneighbor = g_list_last (stroke->anchors);

      /* Check if the neighbor is at an end of the control points */
      if (listneighbor->data == neighbor)
        {
          loose_end = 1;
        }
      else
        {
          listneighbor = g_list_first (stroke->anchors);
          if (listneighbor->data == neighbor)
            {
              loose_end = -1;
            }
          else
            {
              /*
               * it isnt. if we are on a handle go to the nearest
               * anchor and see if we can find an end from it.
               * Yes, this is tedious.
               */

              listneighbor = g_list_find (stroke->anchors, neighbor);

              if (listneighbor && neighbor->type == GIMP_ANCHOR_CONTROL)
                {
                  if (listneighbor->prev &&
                      GIMP_ANCHOR (listneighbor->prev->data)->type == GIMP_ANCHOR_ANCHOR)
                    {
                      listneighbor = listneighbor->prev;
                    }
                  else if (listneighbor->next &&
                           GIMP_ANCHOR (listneighbor->next->data)->type == GIMP_ANCHOR_ANCHOR)
                    {
                      listneighbor = listneighbor->next;
                    }
                  else
                    {
                      loose_end = 0;
                      listneighbor = NULL;
                    }
                }

              if (listneighbor)
                /* we found a suitable ANCHOR_ANCHOR now, lets
                 * search for its loose end.
                 */
                {
                  if (listneighbor->prev &&
                      listneighbor->prev->prev == NULL)
                    {
                      loose_end = -1;
                      listneighbor = listneighbor->prev;
                    }
                  else if (listneighbor->next &&
                           listneighbor->next->next == NULL)
                    {
                      loose_end = 1;
                      listneighbor = listneighbor->next;
                    }
                }
            }
        }

      if (loose_end)
        {
          GimpAnchorType  type;

          /* We have to detect the type of the point to add... */

          control_count = 0;

          if (loose_end == 1)
            {
              while (listneighbor &&
                     GIMP_ANCHOR (listneighbor->data)->type == GIMP_ANCHOR_CONTROL)
                {
                  control_count++;
                  listneighbor = listneighbor->prev;
                }
            }
          else
            {
              while (listneighbor &&
                     GIMP_ANCHOR (listneighbor->data)->type == GIMP_ANCHOR_CONTROL)
                {
                  control_count++;
                  listneighbor = listneighbor->next;
                }
            }

          switch (extend_mode)
            {
            case EXTEND_SIMPLE:
              switch (control_count)
                {
                case 0:
                  type = GIMP_ANCHOR_CONTROL;
                  break;
                case 1:
                  if (listneighbor)  /* only one handle in the path? */
                    type = GIMP_ANCHOR_CONTROL;
                  else
                    type = GIMP_ANCHOR_ANCHOR;
                  break;
                case 2:
                  type = GIMP_ANCHOR_ANCHOR;
                  break;
                default:
                  g_warning ("inconsistent bezier curve: "
                             "%d successive control handles", control_count);
                  type = GIMP_ANCHOR_ANCHOR;
                }

              anchor = gimp_anchor_new (type, coords);

              if (loose_end == 1)
                stroke->anchors = g_list_append (stroke->anchors, anchor);

              if (loose_end == -1)
                stroke->anchors = g_list_prepend (stroke->anchors, anchor);
              break;

            case EXTEND_EDITABLE:
              switch (control_count)
                {
                case 0:
                  neighbor = gimp_bezier_stroke_extend (stroke,
                                                        &(neighbor->position),
                                                        neighbor,
                                                        EXTEND_SIMPLE);
                case 1:
                  neighbor = gimp_bezier_stroke_extend (stroke,
                                                        coords,
                                                        neighbor,
                                                        EXTEND_SIMPLE);
                case 2:
                  anchor = gimp_bezier_stroke_extend (stroke,
                                                      coords,
                                                      neighbor,
                                                      EXTEND_SIMPLE);

                  neighbor = gimp_bezier_stroke_extend (stroke,
                                                        coords,
                                                        anchor,
                                                        EXTEND_SIMPLE);
                  break;
                default:
                  g_warning ("inconsistent bezier curve: "
                             "%d successive control handles", control_count);
                }
            }

          return anchor;
        }
      return NULL;
    }
}

static gboolean
gimp_bezier_stroke_connect_stroke (GimpStroke *stroke,
                                   GimpAnchor *anchor,
                                   GimpStroke *extension,
                                   GimpAnchor *neighbor)
{
  GList *list1, *list2;
  
  g_return_val_if_fail (stroke->closed == FALSE &&
                        extension->closed == FALSE, FALSE);

  list1 = g_list_find (stroke->anchors, anchor);
  list1 = gimp_bezier_stroke_get_anchor_listitem (list1);
  list2 = g_list_find (extension->anchors, neighbor);
  list2 = gimp_bezier_stroke_get_anchor_listitem (list2);

  g_return_val_if_fail (list1 != NULL && list2 != NULL, FALSE);

  if (stroke == extension)
    {
      g_return_val_if_fail ((list1->prev && list1->prev->prev == NULL &&
                             list2->next && list2->next->next == NULL) || 
                            (list1->next && list1->next->next == NULL &&
                             list2->prev && list2->prev->prev == NULL), FALSE);
      gimp_stroke_close (stroke);
      return TRUE;
    }

  if (list1->prev && list1->prev->prev == NULL)
    {
      stroke->anchors = g_list_reverse (stroke->anchors);
    }

  g_return_val_if_fail (list1->next && list1->next->next == NULL, FALSE);

  if (list2->next && list2->next->next == NULL)
    {
      extension->anchors = g_list_reverse (extension->anchors);
    }

  g_return_val_if_fail (list2->prev && list2->prev->prev == NULL, FALSE);

  stroke->anchors = g_list_concat (stroke->anchors, extension->anchors);
  extension->anchors = NULL;

  return TRUE;
}


static void
gimp_bezier_stroke_anchor_move_relative (GimpStroke            *stroke,
                                         GimpAnchor            *anchor,
                                         const GimpCoords      *deltacoord,
                                         GimpAnchorFeatureType  feature)
{
  GimpCoords  delta, coord1, coord2;
  GList      *anchor_list;

  delta = *deltacoord;
  delta.pressure = 0;
  delta.xtilt    = 0;
  delta.ytilt    = 0;
  delta.wheel    = 0;

  gimp_bezier_coords_add (&(anchor->position), &delta, &coord1);
  anchor->position = coord1;

  anchor_list = g_list_find (stroke->anchors, anchor);
  g_return_if_fail (anchor_list != NULL);

  if (anchor->type == GIMP_ANCHOR_ANCHOR)
    {
      if (g_list_previous (anchor_list))
        {
          coord2 = GIMP_ANCHOR (g_list_previous (anchor_list)->data)->position;
          gimp_bezier_coords_add (&coord2, &delta, &coord1);
          GIMP_ANCHOR (g_list_previous (anchor_list)->data)->position = coord1;
        }

      if (g_list_next (anchor_list))
        {
          coord2 = GIMP_ANCHOR (g_list_next (anchor_list)->data)->position;
          gimp_bezier_coords_add (&coord2, &delta, &coord1);
          GIMP_ANCHOR (g_list_next (anchor_list)->data)->position = coord1;
        }
    }
  else
    {
      if (feature == GIMP_ANCHOR_FEATURE_SYMMETRIC)
        {
          GList *neighbour = NULL, *opposite = NULL;

          /* search for opposite control point. Sigh. */
          neighbour = g_list_previous (anchor_list);
          if (neighbour &&
              GIMP_ANCHOR (neighbour->data)->type == GIMP_ANCHOR_ANCHOR)
            {
              opposite = g_list_previous (neighbour);
            }
          else
            {
              neighbour = g_list_next (anchor_list);
              if (neighbour &&
                  GIMP_ANCHOR (neighbour->data)->type == GIMP_ANCHOR_ANCHOR)
                {
                  opposite = g_list_next (neighbour);
                }
            }
          if (opposite &&
              GIMP_ANCHOR (opposite->data)->type == GIMP_ANCHOR_CONTROL)
            {
              gimp_bezier_coords_difference (&(GIMP_ANCHOR (neighbour->data)->position),
                                             &(anchor->position), &delta);
              gimp_bezier_coords_add (&(GIMP_ANCHOR (neighbour->data)->position),
                                      &delta, &coord1);
              GIMP_ANCHOR (opposite->data)->position = coord1;
            }
        }
    }
}


static void
gimp_bezier_stroke_anchor_move_absolute (GimpStroke            *stroke,
                                         GimpAnchor            *anchor,
                                         const GimpCoords      *coord,
                                         GimpAnchorFeatureType  feature)
{
  GimpCoords deltacoord;

  gimp_bezier_coords_difference (coord, &anchor->position, &deltacoord);
  gimp_bezier_stroke_anchor_move_relative (stroke, anchor,
                                           &deltacoord, feature);
}

static void
gimp_bezier_stroke_anchor_convert (GimpStroke            *stroke,
                                   GimpAnchor            *anchor,
                                   GimpAnchorFeatureType  feature)
{
  GList *anchor_list;

  anchor_list = g_list_find (stroke->anchors, anchor);

  g_return_if_fail (anchor_list != NULL);

  switch (feature)
    {
    case GIMP_ANCHOR_FEATURE_EDGE:
      if (anchor->type == GIMP_ANCHOR_ANCHOR)
        {
          if (g_list_previous (anchor_list))
            GIMP_ANCHOR (g_list_previous (anchor_list)->data)->position =
              anchor->position;

          if (g_list_next (anchor_list))
            GIMP_ANCHOR (g_list_next (anchor_list)->data)->position =
              anchor->position;
        }
      else
        {
          if (g_list_previous (anchor_list) &&
              GIMP_ANCHOR (g_list_previous (anchor_list)->data)->type == GIMP_ANCHOR_ANCHOR)
            anchor->position = GIMP_ANCHOR (g_list_previous (anchor_list)->data)->position;
          if (g_list_next (anchor_list) &&
              GIMP_ANCHOR (g_list_next (anchor_list)->data)->type == GIMP_ANCHOR_ANCHOR)
            anchor->position = GIMP_ANCHOR (g_list_next (anchor_list)->data)->position;
        }

      break;

    default:
      g_warning ("gimp_bezier_stroke_anchor_convert: "
                 "unimplemented anchor conversion %d\n", feature);
    }
}

static GArray *
gimp_bezier_stroke_interpolate (const GimpStroke  *stroke,
                                gdouble            precision,
                                gboolean          *ret_closed)
{
  GArray     *ret_coords;
  GimpAnchor *anchor;
  GList      *anchorlist;
  GimpCoords  segmentcoords[4];
  gint        count;
  gboolean    need_endpoint = FALSE;

  g_return_val_if_fail (GIMP_IS_BEZIER_STROKE (stroke), NULL);
  g_return_val_if_fail (ret_closed != NULL, NULL);

  if (!stroke->anchors)
    {
      *ret_closed = FALSE;
      return NULL;
    }

  ret_coords = g_array_new (FALSE, FALSE, sizeof (GimpCoords));

  count = 0;

  for (anchorlist = stroke->anchors;
       anchorlist && GIMP_ANCHOR (anchorlist->data)->type != GIMP_ANCHOR_ANCHOR;
       anchorlist = g_list_next (anchorlist));

  for ( ; anchorlist; anchorlist = g_list_next (anchorlist))
    {
      anchor = anchorlist->data;

      segmentcoords[count] = anchor->position;
      count++;

      if (count == 4)
        {
          gimp_bezier_coords_subdivide (segmentcoords, precision, &ret_coords);
          segmentcoords[0] = segmentcoords[3];
          count = 1;
          need_endpoint = TRUE;
        }
    }

  if (stroke->closed && stroke->anchors)
    {
      anchorlist = stroke->anchors;

      while (count < 3)
        {
          segmentcoords[count] = GIMP_ANCHOR (anchorlist->data)->position;
          count++;
        }
      anchorlist = g_list_next (anchorlist);
      if (anchorlist)
        segmentcoords[3] = GIMP_ANCHOR (anchorlist->data)->position;

      gimp_bezier_coords_subdivide (segmentcoords, precision, &ret_coords);
      need_endpoint = TRUE;

    }

  if (need_endpoint)
    ret_coords = g_array_append_val (ret_coords, segmentcoords[3]);

  *ret_closed = stroke->closed;

  if (ret_coords->len == 0)
    {
      g_array_free (ret_coords, TRUE);
      ret_coords = NULL;
    }

  return ret_coords;
}

GimpStroke *
gimp_bezier_stroke_new_moveto (const GimpCoords *start)
{
  GimpStroke *stroke;

  stroke = gimp_bezier_stroke_new ();

  stroke->anchors = g_list_prepend (stroke->anchors,
                                    gimp_anchor_new (GIMP_ANCHOR_CONTROL,
                                                     start));
  stroke->anchors = g_list_prepend (stroke->anchors,
                                    gimp_anchor_new (GIMP_ANCHOR_ANCHOR,
                                                     start));
  stroke->anchors = g_list_prepend (stroke->anchors,
                                    gimp_anchor_new (GIMP_ANCHOR_CONTROL,
                                                     start));
  return stroke;
}

void
gimp_bezier_stroke_lineto (GimpStroke       *stroke,
                           const GimpCoords *end)
{
  g_return_if_fail (GIMP_IS_BEZIER_STROKE (stroke));
  g_return_if_fail (stroke->closed == FALSE);
  g_return_if_fail (stroke->anchors != NULL);

  stroke->anchors = g_list_prepend (stroke->anchors,
                                    gimp_anchor_new (GIMP_ANCHOR_CONTROL,
                                                     end));
  stroke->anchors = g_list_prepend (stroke->anchors,
                                    gimp_anchor_new (GIMP_ANCHOR_ANCHOR,
                                                     end));
  stroke->anchors = g_list_prepend (stroke->anchors,
                                    gimp_anchor_new (GIMP_ANCHOR_CONTROL,
                                                     end));
}

void
gimp_bezier_stroke_conicto (GimpStroke       *stroke,
                            const GimpCoords *control,
                            const GimpCoords *end)
{
  GimpCoords start, coords;

  g_return_if_fail (GIMP_IS_BEZIER_STROKE (stroke));
  g_return_if_fail (stroke->closed == FALSE);
  g_return_if_fail (stroke->anchors != NULL);
  g_return_if_fail (stroke->anchors->next != NULL);

  start = GIMP_ANCHOR (stroke->anchors->next->data)->position;

  gimp_bezier_coords_mix (2.0 / 3.0, control, 1.0 / 3.0, &start, &coords);

  GIMP_ANCHOR (stroke->anchors->data)->position = coords;

  gimp_bezier_coords_mix (2.0 / 3.0, control, 1.0 / 3.0, end, &coords);
  
  stroke->anchors = g_list_prepend (stroke->anchors,
                                    gimp_anchor_new (GIMP_ANCHOR_CONTROL,
                                                     &coords));
  stroke->anchors = g_list_prepend (stroke->anchors,
                                    gimp_anchor_new (GIMP_ANCHOR_ANCHOR,
                                                     end));
  stroke->anchors = g_list_prepend (stroke->anchors,
                                    gimp_anchor_new (GIMP_ANCHOR_CONTROL,
                                                     end));
}

void
gimp_bezier_stroke_cubicto (GimpStroke       *stroke,
                            const GimpCoords *control1,
                            const GimpCoords *control2,
                            const GimpCoords *end)
{
  g_return_if_fail (GIMP_IS_BEZIER_STROKE (stroke));
  g_return_if_fail (stroke->closed == FALSE);
  g_return_if_fail (stroke->anchors != NULL);

  GIMP_ANCHOR (stroke->anchors->data)->position = *control1;

  stroke->anchors = g_list_prepend (stroke->anchors,
                                    gimp_anchor_new (GIMP_ANCHOR_CONTROL,
                                                     control2));
  stroke->anchors = g_list_prepend (stroke->anchors,
                                    gimp_anchor_new (GIMP_ANCHOR_ANCHOR,
                                                     end));
  stroke->anchors = g_list_prepend (stroke->anchors,
                                    gimp_anchor_new (GIMP_ANCHOR_CONTROL,
                                                     end));
}


/* helper function to get the associated anchor of a listitem */

static GList *
gimp_bezier_stroke_get_anchor_listitem (GList *list)
{
  if (!list)
    return NULL;

  if (GIMP_ANCHOR (list->data)->type == GIMP_ANCHOR_ANCHOR)
    return list;

  if (list->prev && GIMP_ANCHOR (list->prev->data)->type == GIMP_ANCHOR_ANCHOR)
    return list->prev;

  if (list->next && GIMP_ANCHOR (list->next->data)->type == GIMP_ANCHOR_ANCHOR)
    return list->next;

  g_return_val_if_fail (/* bezier stroke inconsistent! */ FALSE, NULL);
  return NULL;
}


/*
 * a helper function that determines if a bezier segment is "straight
 * enough" to be approximated by a line.
 *
 * Needs four GimpCoords in an array.
 */

static gboolean
gimp_bezier_coords_is_straight (const GimpCoords *beziercoords,
                                gdouble           precision)
{
  GimpCoords line, tan1, tan2, d1, d2;
  gdouble    l2, s1, s2;

  gimp_bezier_coords_difference (&(beziercoords[3]),
                                 &(beziercoords[0]),
                                 &line);

  if (gimp_bezier_coords_length2 (&line) < precision * precision)
    {
      gimp_bezier_coords_difference (&(beziercoords[1]),
                                     &(beziercoords[0]),
                                     &tan1);
      gimp_bezier_coords_difference (&(beziercoords[2]),
                                     &(beziercoords[3]),
                                     &tan2);
      if ((gimp_bezier_coords_length2 (&tan1) < precision * precision) &&
          (gimp_bezier_coords_length2 (&tan2) < precision * precision))
        {
          return 1;
        }
      else
        {
          /* Tangents are too big for the small baseline */
          return 0;
        }
    }
  else
    {
      gimp_bezier_coords_difference (&(beziercoords[1]),
                                     &(beziercoords[0]),
                                     &tan1);
      gimp_bezier_coords_difference (&(beziercoords[2]),
                                     &(beziercoords[0]),
                                     &tan2);

      l2 = gimp_bezier_coords_scalarprod (&line, &line);
      s1 = gimp_bezier_coords_scalarprod (&line, &tan1) / l2;
      s2 = gimp_bezier_coords_scalarprod (&line, &tan2) / l2;

      if (s1 < 0 || s1 > 1 || s2 < 0 || s2 > 1 || s2 < s1)
        {
          /* The tangents get projected outside the baseline */
          return 0;
        }

      gimp_bezier_coords_mix (1.0, &tan1, - s1, &line, &d1);
      gimp_bezier_coords_mix (1.0, &tan2, - s2, &line, &d2);

      if ((gimp_bezier_coords_length2 (&d1) > precision * precision) ||
          (gimp_bezier_coords_length2 (&d2) > precision * precision))
        {
          /* The control points are too far away from the baseline */
          return 0;
        }

      return 1;
    }
}


/* local helper functions for bezier subdivision */

static void
gimp_bezier_coords_subdivide (const GimpCoords  *beziercoords,
                              const gdouble      precision,
                              GArray           **ret_coords)
{
  gimp_bezier_coords_subdivide2 (beziercoords, precision, ret_coords, 10);
}


static void
gimp_bezier_coords_subdivide2 (const GimpCoords *beziercoords,
                               const gdouble     precision,
                               GArray          **ret_coords,
                               gint              depth)
{
  /*
   * beziercoords has to contain four GimpCoords with the four control points
   * of the bezier segment. We subdivide it at the parameter 0.5.
   */

  GimpCoords subdivided[8];

  subdivided[0] = beziercoords[0];
  subdivided[6] = beziercoords[3];

  if (!depth) g_printerr ("Hit rekursion depth limit!\n");

  gimp_bezier_coords_average (&(beziercoords[0]), &(beziercoords[1]),
                              &(subdivided[1]));

  gimp_bezier_coords_average (&(beziercoords[1]), &(beziercoords[2]),
                              &(subdivided[7]));

  gimp_bezier_coords_average (&(beziercoords[2]), &(beziercoords[3]),
                              &(subdivided[5]));

  gimp_bezier_coords_average (&(subdivided[1]), &(subdivided[7]),
                              &(subdivided[2]));

  gimp_bezier_coords_average (&(subdivided[7]), &(subdivided[5]),
                              &(subdivided[4]));

  gimp_bezier_coords_average (&(subdivided[2]), &(subdivided[4]),
                              &(subdivided[3]));

  /*
   * We now have the coordinates of the two bezier segments in
   * subdivided [0-3] and subdivided [3-6]
   */

  /*
   * Here we need to check, if we have sufficiently subdivided, i.e.
   * if the stroke is sufficiently close to a straight line.
   */

  if (!depth || gimp_bezier_coords_is_straight (&(subdivided[0]),
                                                precision)) /* 1st half */
    {
      *ret_coords = g_array_append_vals (*ret_coords, &(subdivided[0]), 3);
    }
  else
    {
      gimp_bezier_coords_subdivide2 (&(subdivided[0]), precision,
                                     ret_coords, depth-1);
    }

  if (!depth || gimp_bezier_coords_is_straight (&(subdivided[3]),
                                                precision)) /* 2nd half */
    {
      *ret_coords = g_array_append_vals (*ret_coords, &(subdivided[3]), 3);
    }
  else
    {
      gimp_bezier_coords_subdivide2 (&(subdivided[3]), precision,
                                     ret_coords, depth-1);
    }
}
