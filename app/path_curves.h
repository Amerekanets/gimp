/* The GIMP -- an image manipulation program
 *
 * This file Copyright (C) 1999 Simon Budig
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

#ifndef __PATH_CURVES_H__
#define __PATH_CURVES_H__

#undef PATH_TOOL_DEBUG
#ifdef PATH_TOOL_DEBUG
#include <stdio.h>
#endif

#define IMAGE_COORDS    1
#define AA_IMAGE_COORDS 2
#define SCREEN_COORDS   3

#define SEGMENT_ACTIVE  1

#define PATH_TOOL_DRAG  1

#define PATH_TOOL_REDRAW_ALL      1
#define PATH_TOOL_REDRAW_ACTIVE   2
#define PATH_TOOL_REDRAW_HANDLES  4

#define SUBDIVIDE  1000

typedef enum { SEGMENT_LINE=0, SEGMENT_BEZIER} SegmentType;

enum { ON_ANCHOR, ON_HANDLE, ON_CURVE, ON_CANVAS };

typedef struct _path_segment PathSegment;
typedef struct _path_curve   PathCurve;
typedef struct _npath         NPath;


struct _path_segment
{
   SegmentType  type;         /* What type of segment */
   gdouble      x, y;         /* location of starting-point in image space  */
   gpointer     data;         /* Additional data, dependant of segment-type */
   
   guint32      flags;        /* Various Flags: Is the Segment active? */

   PathCurve   *parent;       /* the parent Curve */
   PathSegment *next;         /* Next Segment or NULL */
   PathSegment *prev;         /* Previous Segment or NULL */

};


struct _path_curve
{
   PathSegment *segments;    /* The segments of the curve */
   PathSegment *cur_segment; /* the current segment */
   NPath        *parent;      /* the parent Path */
   PathCurve   *next;        /* Next Curve or NULL */
   PathCurve   *prev;        /* Previous Curve or NULL */
};


struct _npath
{
   PathCurve *curves;        /* the curves */
   PathCurve *cur_curve;     /* the current curve */
   GString   *name;          /* the name of the path */
   guint32    state;         /* is the path locked? */
   /* GimpPathTool  *path_tool; */     /* The parent Path Tool */
};


typedef void
(*PathTraverseFunc)    (NPath *,
                        PathCurve *,
                        gpointer);
typedef void
(*CurveTraverseFunc)   (NPath *,
                        PathCurve *,
                        PathSegment *,
                        gpointer);
typedef void
(*SegmentTraverseFunc) (NPath *,
                        PathCurve *,
                        PathSegment *,
                        gint,
                        gint,
                        gpointer);


/*
 * This function is to get a set of npoints different coordinates for
 * the range from start to end (each in the range from 0 to 1 and
 * start < end).
 * returns the number of created coords. Make sure that the points-
 * Array is allocated.
 */

typedef guint (*PathGetPointsFunc) (PathSegment *segment,
				    gdouble *points,
				    guint npoints,
				    gdouble start,
				    gdouble end);

typedef void (*PathGetPointFunc) (PathSegment *segment,
		       		  gdouble position,
		       		  gdouble *x,
		       		  gdouble *y);

typedef void (*PathDrawHandlesFunc) (GimpDrawTool *tool,
			  	     PathSegment *segment);
			  
typedef void (*PathDrawSegmentFunc) (GimpDrawTool *tool,
			  	     PathSegment *segment);
			  

typedef gdouble (*PathOnSegmentFunc) (PathSegment *segment,
				      gint x,
				      gint y,
				      gint halfwidth,
				      gint *distance);

typedef void (*PathDragSegmentFunc) (PathSegment *segment,
				     gdouble position,
				     gdouble dx,
				     gdouble dy);

typedef gint (*PathOnHandlesFunc) (PathSegment *segment,
				   gdouble x,
				   gdouble y,
				   gdouble halfwidth);

typedef void (*PathDragHandleFunc) (PathSegment *segment,
				    gdouble dx,
				    gdouble dy,
				    gint handle_id);

typedef PathSegment * (*PathInsertAnchorFunc) (PathSegment *segment,
					       gdouble position);

typedef void (*PathUpdateSegmentFunc) (PathSegment *segment);

typedef void (*PathFlipSegmentFunc) (PathSegment *segment);

typedef void (*PathInitSegmentFunc) (PathSegment *segment);

typedef void (*PathCleanupSegmentFunc) (PathSegment *segment);

typedef struct {
   PathGetPointsFunc      get_points;
   PathGetPointFunc       get_point;
   PathDrawHandlesFunc    draw_handles;
   PathDrawSegmentFunc    draw_segment;
   PathOnSegmentFunc      on_segment;
   PathDragSegmentFunc    drag_segment;
   PathOnHandlesFunc      on_handles;
   PathDragHandleFunc     drag_handle;
   PathInsertAnchorFunc   insert_anchor;
   PathUpdateSegmentFunc  update_segment;
   PathFlipSegmentFunc    flip_segment;
   PathInitSegmentFunc    init_segment;
   PathCleanupSegmentFunc cleanup_segment;
} CurveDescription;


guint
path_curve_get_points (PathSegment *segment,
		       gdouble *points,
		       guint npoints,
		       gdouble start,
		       gdouble end);

void
path_curve_get_point (PathSegment *segment,
		      gdouble position,
		      gdouble *x,
		      gdouble *y);

void
path_curve_draw_handles (GimpDrawTool *tool,
			 PathSegment *segment);
			  
void
path_curve_draw_segment (GimpDrawTool *tool,
			 PathSegment *segment);
			  

gdouble
path_curve_on_segment (PathSegment *segment,
		       gint x,
		       gint y,
		       gint halfwidth,
		       gint *distance);

void
path_curve_drag_segment (PathSegment *segment,
			 gdouble position,
			 gdouble dx,
			 gdouble dy);

gint
path_curve_on_handle (PathSegment *segment,
		      gdouble x,
		      gdouble y,
		      gdouble halfwidth);

void
path_curve_drag_handle (PathSegment *segment,
			gdouble dx,
			gdouble dy,
			gint handle_id);

PathSegment *
path_curve_insert_anchor (PathSegment *segment,
		          gdouble position);

void
path_curve_update_segment (PathSegment *segment);

void
path_curve_flip_segment (PathSegment *segment);

void
path_curve_init_segment (PathSegment *segment);

void
path_curve_cleanup_segment (PathSegment *segment);










/* This is, what Soleil (Olofs little daughter) has to say to this:

fc fc g hgvfvv  drrrrrrtcc jctfcz w   sdzs d bx   cv^[ ^[c^[f c
vffvcccccccccccccggfc fvx^[c^[x^[x^[x^[x^[x^[x^[ v       xbvcbvcxv cxxc xxxx^[x
xz^[c^[x^[x^[x^[x^[x^[x^[xxxxxxcccccccxxxxxxxxxxxxxxxv�"�p'
hj^[[24~^[[4~^[[1~^[[4~^[[1~^[[4~ ^[[D^[[Bk^[[B,,,,,
,^[[2~^[[4~^[[6~^[[4~l^[[6~,l' .holg^[[B^[[B n,,klmj ^[[B^[[1~j ^[[P^[[B
^[[D^[[4~^[[6~nb ^[[A^[[C ^[[Akj^[[B            ^[[A^[[C^[[A


...^[[1~^[[D^[[4~^[[2~^[[C^[[B,^[[A^[[2~^[[C^[[2~^[[A^[[3~^[[A^[[4~ ^[[2~
^[[2~p�-  ., �^[[A�pl.,  k,km ,
m,^[[5~^[[6~^[[2~^[[C^[[3~p^[[A^[[B�^[[2~^[[B^[[6~^[[1~, .^[[D^[[4~^[[2~^[[Db
.l, .,.,m ^[[2~p�l. ik
^[[20~kl9i^[[20~^[[20~^[[20~^[[21~^[[21~^[[21~^[[21~^[[21~^[[21~^[[20~m +
^[[A^[[5~^[[G^[[D ^[[5~^[[1+^[[C

*/

#endif /* __PATH_CURVES_H__ */

