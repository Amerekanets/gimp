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

#include <gtk/gtk.h>

#include "display-types.h"

#include "base/boundary.h"

#include "core/gimpimage.h"
#include "core/gimpimage-mask.h"

#include "gimpdisplay.h"
#include "gimpdisplay-marching-ants.h"
#include "gimpdisplay-ops.h"
#include "gimpdisplay-selection.h"

#include "colormaps.h"
#include "gimprc.h"


#define USE_XDRAWPOINTS
#undef VERBOSE

/*  The possible internal drawing states...  */
#define INVISIBLE         0
#define INTRO             1
#define MARCHING          2

#define INITIAL_DELAY     15  /* in milleseconds */


/*  local function prototypes  */

static GdkPixmap * create_cycled_ants_pixmap (GdkWindow  *window,
					      gint        depth);
static void        cycle_ant_colors          (Selection  *select);
static void        selection_add_point       (GdkPoint   *points[8],
					      gint        max_npoints[8],
					      gint        npoints[8],
					      gint        x,
					      gint        y);
static void        selection_render_points   (Selection  *select);
static void        selection_draw            (Selection  *select);
static void        selection_transform_segs  (Selection  *select,
					      BoundSeg   *src_segs,
					      GdkSegment *dest_segs,
					      gint        num_segs);
static void        selection_generate_segs   (Selection  *select);
static void        selection_free_segs       (Selection  *select);
static gboolean    selection_start_marching  (gpointer    data);
static gboolean    selection_march_ants      (gpointer    data);


GdkPixmap * marching_ants[9]  = { NULL };
GdkPixmap * cycled_ants_pixmap = NULL;


/*  public functions  */

Selection *
selection_create (GdkWindow *win,
		  GDisplay  *gdisp,
		  gint       size,
		  gint       width,
		  gint       speed)
{
  GdkColor   fg, bg;
  Selection *new;
  gint       base_type;
  gint       i;

  new = g_new (Selection, 1);
  base_type = gimp_image_base_type (gdisp->gimage);

  if (gimprc.cycled_marching_ants)
    {
      new->cycle = TRUE;
      if (!cycled_ants_pixmap)
	cycled_ants_pixmap = create_cycled_ants_pixmap (win, g_visual->depth);

      new->cycle_pix = cycled_ants_pixmap;
    }
  else
    {
      new->cycle = FALSE;
      if (!marching_ants[0])
	for (i = 0; i < 8; i++)
	  marching_ants[i] = gdk_bitmap_create_from_data (win, (char*) ant_data[i], 8, 8);
    }

  new->win            = win;
  new->gdisp          = gdisp;
  new->segs_in        = NULL;
  new->segs_out       = NULL;
  new->segs_layer     = NULL;
  new->num_segs_in    = 0;
  new->num_segs_out   = 0;
  new->num_segs_layer = 0;
  new->index_in       = 0;
  new->index_out      = 0;
  new->index_layer    = 0;
  new->state          = INVISIBLE;
  new->paused         = 0;
  new->recalc         = TRUE;
  new->speed          = speed;
  new->hidden         = FALSE;

  for (i = 0; i < 8; i++)
    new->points_in[i] = NULL;

  /*  create a new graphics context  */
  new->gc_in = gdk_gc_new (new->win);

  if (new->cycle)
    {
      gdk_gc_set_fill (new->gc_in, GDK_TILED);
      gdk_gc_set_tile (new->gc_in, new->cycle_pix);
      gdk_gc_set_line_attributes (new->gc_in, 1, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER);
    }
  else
    {
      /*  get black and white pixels for this gdisplay  */
      fg.pixel = gdisplay_black_pixel (gdisp);
      bg.pixel = gdisplay_white_pixel (gdisp);

      gdk_gc_set_foreground (new->gc_in, &fg);
      gdk_gc_set_background (new->gc_in, &bg);
      gdk_gc_set_fill (new->gc_in, GDK_OPAQUE_STIPPLED);
      gdk_gc_set_line_attributes (new->gc_in, 1, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER);
    }

#ifdef USE_XDRAWPOINTS
  new->gc_white = gdk_gc_new (new->win);
  gdk_gc_set_foreground (new->gc_white, &bg);

  new->gc_black = gdk_gc_new (new->win);
  gdk_gc_set_foreground (new->gc_black, &fg);
#endif

  /*  Setup 2nd & 3rd GCs  */
  fg.pixel = gdisplay_white_pixel (gdisp);
  bg.pixel = gdisplay_gray_pixel (gdisp);

  /*  create a new graphics context  */
  new->gc_out = gdk_gc_new (new->win);
  gdk_gc_set_foreground (new->gc_out, &fg);
  gdk_gc_set_background (new->gc_out, &bg);
  gdk_gc_set_fill (new->gc_out, GDK_OPAQUE_STIPPLED);
  gdk_gc_set_line_attributes (new->gc_out, 1, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER);

  /*  get black and color pixels for this gdisplay  */
  fg.pixel = gdisplay_black_pixel (gdisp);
  bg.pixel = gdisplay_color_pixel (gdisp);

  /*  create a new graphics context  */
  new->gc_layer = gdk_gc_new (new->win);
  gdk_gc_set_foreground (new->gc_layer, &fg);
  gdk_gc_set_background (new->gc_layer, &bg);
  gdk_gc_set_fill (new->gc_layer, GDK_OPAQUE_STIPPLED);
  gdk_gc_set_line_attributes (new->gc_layer, 1, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER);

  return new;
}


void
selection_free (Selection *select)
{
  if (select->state != INVISIBLE)
    g_source_remove (select->timeout_id);

  if (select->gc_in)
    gdk_gc_unref (select->gc_in);
  if (select->gc_out)
    gdk_gc_unref (select->gc_out);
  if (select->gc_layer)
    gdk_gc_unref (select->gc_layer);
#ifdef USE_XDRAWPOINTS
  if (select->gc_white)
    gdk_gc_unref (select->gc_white);
  if (select->gc_black)
    gdk_gc_unref (select->gc_black);
#endif
  selection_free_segs (select);

  g_free (select);
}


void
selection_pause (Selection *select)
{
  if (select->state != INVISIBLE)
    {
      g_source_remove (select->timeout_id);
      select->timeout_id = 0;

      select->state = INVISIBLE;
    }

  select->paused ++;
}


void
selection_resume (Selection *select)
{
  if (select->paused == 1)
    {
      select->state      = INTRO;
      select->timeout_id = g_timeout_add (INITIAL_DELAY,
					  selection_start_marching,
					  select);
    }

  select->paused--;
}


void
selection_start (Selection *select,
		 gboolean   recalc)
{
  /*  A call to selection_start with recalc == TRUE means that
   *  we want to recalculate the selection boundary--usually
   *  after scaling or panning the display, or modifying the
   *  selection in some way.  If recalc == FALSE, the already
   *  calculated boundary is simply redrawn.
   */
  if (recalc)
    select->recalc = TRUE;

  /*  If this selection is paused, do not start it  */
  if (select->paused > 0)
    return;

  if (select->state != INVISIBLE)
    g_source_remove (select->timeout_id);

  select->state      = INTRO;  /*  The state before the first draw  */
  select->timeout_id = g_timeout_add (INITIAL_DELAY,
				      selection_start_marching,
				      select);
}


void
selection_invis (Selection *select)
{
  GDisplay * gdisp;
  int x1, y1, x2, y2;

  if (select->state != INVISIBLE)
    {
      g_source_remove (select->timeout_id);
      select->timeout_id = 0;

      select->state = INVISIBLE;
    }

  gdisp = (GDisplay *) select->gdisp;

  /*  Find the bounds of the selection  */
  if (gdisplay_mask_bounds (gdisp, &x1, &y1, &x2, &y2))
    {
      gdisplay_expose_area (gdisp, x1, y1, (x2 - x1), (y2 - y1));
    }
  else
    {
      selection_start (select, TRUE);
    }
}


void
selection_layer_invis (Selection *select)
{
  gint x1, y1;
  gint x2, y2;
  gint x3, y3;
  gint x4, y4;

  if (select->state != INVISIBLE)
    {
      g_source_remove (select->timeout_id);
      select->timeout_id = 0;

      select->state = INVISIBLE;
    }

  if (select->segs_layer != NULL && select->num_segs_layer == 4)
    {
      GDisplay *gdisp;

      gdisp = select->gdisp;

      x1 = select->segs_layer[0].x1 - 1;
      y1 = select->segs_layer[0].y1 - 1;
      x2 = select->segs_layer[3].x2 + 1;
      y2 = select->segs_layer[3].y2 + 1;

      x3 = select->segs_layer[0].x1 + 1;
      y3 = select->segs_layer[0].y1 + 1;
      x4 = select->segs_layer[3].x2 - 1;
      y4 = select->segs_layer[3].y2 - 1;

      /*  expose the region  */
      gdisplay_expose_area (gdisp, x1, y1, (x2 - x1) + 1, (y3 - y1) + 1);
      gdisplay_expose_area (gdisp, x1, y3, (x3 - x1) + 1, (y4 - y3) + 1);
      gdisplay_expose_area (gdisp, x1, y4, (x2 - x1) + 1, (y2 - y4) + 1);
      gdisplay_expose_area (gdisp, x4, y3, (x2 - x4) + 1, (y4 - y3) + 1);
    }
}


void
selection_toggle (Selection *select)
{
  selection_invis (select);
  selection_layer_invis (select);

  /*  toggle the visibility  */
  select->hidden = select->hidden ? FALSE : TRUE;

  selection_start (select, TRUE);
}


/*  private functions  */

static GdkPixmap *
create_cycled_ants_pixmap (GdkWindow *window,
			   gint       depth)
{
  GdkPixmap *pixmap;
  GdkGC     *gc;
  GdkColor   col;
  gint       i, j;

  pixmap = gdk_pixmap_new (window, 8, 8, depth);
  gc = gdk_gc_new (window);

  for (i = 0; i < 8; i++)
    for (j = 0; j < 8; j++)
      {
	col.pixel = marching_ants_pixels[((i + j) % 8)];
	gdk_gc_set_foreground (gc, &col);
	gdk_draw_line (pixmap, gc, i, j, i, j);
      }

  gdk_gc_unref (gc);

  return pixmap;
}


static void
cycle_ant_colors (Selection *select)
{
  gint i;
  gint index;

  for (i = 0; i < 8; i++)
    {
      index = (i + (8 - select->index_in)) % 8;
      if (index < 4)
	marching_ants_pixels[i] = get_color (0, 0, 0);
      else
	marching_ants_pixels[i] = get_color (255, 255, 255);
    }
}


#define MAX_POINTS_INC 2048

static void
selection_add_point (GdkPoint *points[8],
		     gint      max_npoints[8],
		     gint      npoints[8],
		     gint      x,
		     gint      y)
{
  gint i, j;

  j = (x - y) & 7;

  i = npoints[j]++;
  if (i == max_npoints[j])
    {
      max_npoints[j] += 2048;
      points[j] = g_realloc (points[j], sizeof (GdkPoint) * max_npoints[j]);
    }
  points[j][i].x = x;
  points[j][i].y = y;
}


/* Render the segs_in array into points_in */
static void
selection_render_points (Selection *select)
{
  gint i, j;
  gint max_npoints[8];
  gint x, y;
  gint dx, dy;
  gint dxa, dya;
  gint r;

  if (select->segs_in == NULL)
    return;

  for (j = 0; j < 8; j++)
    {
      max_npoints[j] = MAX_POINTS_INC;
      select->points_in[j] = g_new (GdkPoint, max_npoints[j]);
      select->num_points_in[j] = 0;
    }

  for (i = 0; i < select->num_segs_in; i++)
    {
#ifdef VERBOSE
      g_print ("%2d: (%d, %d) - (%d, %d)\n", i,
	       select->segs_in[i].x1,
	       select->segs_in[i].y1,
	       select->segs_in[i].x2,
	       select->segs_in[i].y2);
#endif
      x = select->segs_in[i].x1;
      dxa = select->segs_in[i].x2 - x;
      if (dxa > 0)
	{
	  dx = 1;
	}
      else
	{
	  dxa = -dxa;
	  dx = -1;
	}
      y = select->segs_in[i].y1;
      dya = select->segs_in[i].y2 - y;
      if (dya > 0)
	{
	  dy = 1;
	}
      else
	{
	  dya = -dya;
	  dy = -1;
	}
      if (dxa > dya)
	{
	  r = dya;
	  do {
	    selection_add_point (select->points_in,
				 max_npoints,
				 select->num_points_in,
				 x, y);
	    x += dx;
	    r += dya;
	    if (r >= (dxa << 1)) {
	      y += dy;
	      r -= (dxa << 1);
	    }
	  } while (x != select->segs_in[i].x2);
	}
      else if (dxa < dya)
	{
	  r = dxa;
	  do {
	    selection_add_point (select->points_in,
				 max_npoints,
				 select->num_points_in,
				 x, y);
	    y += dy;
	    r += dxa;
	    if (r >= (dya << 1)) {
	      x += dx;
	      r -= (dya << 1);
	    }
	  } while (y != select->segs_in[i].y2);
	}
      else
	selection_add_point (select->points_in,
			     max_npoints,
			     select->num_points_in,
			     x, y);
    }
}


static void
selection_draw (Selection *select)
{
  if (select->hidden)
    return;

  if (select->segs_layer && select->index_layer == 0)
    gdk_draw_segments (select->win, select->gc_layer,
		       select->segs_layer, select->num_segs_layer);
#ifdef USE_XDRAWPOINTS
#ifdef VERBOSE
  {
    gint j, sum;
    sum = 0;
    for (j = 0; j < 8; j++)
      sum += select->num_points_in[j];

    g_print ("%d segments, %d points\n", select->num_segs_in, sum);
  }
#endif
  if (select->segs_in)
    {
      gint i;

      if (select->index_in == 0)
	{
	  for (i = 0; i < 4; i++)
	    if (select->num_points_in[i])
	      gdk_draw_points (select->win, select->gc_white,
			       select->points_in[i], select->num_points_in[i]);
	  for (i = 4; i < 8; i++)
	    if (select->num_points_in[i])
	      gdk_draw_points (select->win, select->gc_black,
			       select->points_in[i], select->num_points_in[i]);
	}
      else
	{
	  i = ((select->index_in + 3) & 7);
	  if (select->num_points_in[i])
	    gdk_draw_points (select->win, select->gc_white,
			     select->points_in[i], select->num_points_in[i]);
	  i = ((select->index_in + 7) & 7);
	  if (select->num_points_in[i])
	    gdk_draw_points (select->win, select->gc_black,
			     select->points_in[i], select->num_points_in[i]);
	}
    }
#else
  if (select->segs_in)
    gdk_draw_segments (select->win, select->gc_in,
		       select->segs_in, select->num_segs_in);
#endif
  if (select->segs_out && select->index_out == 0)
    gdk_draw_segments (select->win, select->gc_out,
		       select->segs_out, select->num_segs_out);
}


static void
selection_transform_segs (Selection  *select,
			  BoundSeg   *src_segs,
			  GdkSegment *dest_segs,
			  gint        num_segs)
{
  GDisplay *gdisp;
  gint      x, y;
  gint      i;

  gdisp = (GDisplay *) select->gdisp;

  for (i = 0; i < num_segs; i++)
    {
      gdisplay_transform_coords (gdisp, src_segs[i].x1, src_segs[i].y1,
				 &x, &y, 0);

      dest_segs[i].x1 = x;
      dest_segs[i].y1 = y;

      gdisplay_transform_coords (gdisp, src_segs[i].x2, src_segs[i].y2,
				 &x, &y, 0);

      dest_segs[i].x2 = x;
      dest_segs[i].y2 = y;

      /*  If this segment is a closing segment && the segments lie inside
       *  the region, OR if this is an opening segment and the segments
       *  lie outside the region...
       *  we need to transform it by one display pixel
       */
      if (!src_segs[i].open)
	{
	  /*  If it is vertical  */
	  if (dest_segs[i].x1 == dest_segs[i].x2)
	    {
	      dest_segs[i].x1 -= 1;
	      dest_segs[i].x2 -= 1;
	    }
	  else
	    {
	      dest_segs[i].y1 -= 1;
	      dest_segs[i].y2 -= 1;
	    }
	}
    }
}


static void
selection_generate_segs (Selection *select)
{
  GDisplay *gdisp;
  BoundSeg *segs_in;
  BoundSeg *segs_out;
  BoundSeg *segs_layer;

  gdisp = (GDisplay *) select->gdisp;

  /*  Ask the gimage for the boundary of its selected region...
   *  Then transform that information into a new buffer of XSegments
   */
  gimage_mask_boundary (gdisp->gimage, &segs_in, &segs_out,
			&select->num_segs_in, &select->num_segs_out);
  if (select->num_segs_in)
    {
      select->segs_in = g_new (GdkSegment, select->num_segs_in);
      selection_transform_segs (select, segs_in, select->segs_in,
				select->num_segs_in);
#ifdef USE_XDRAWPOINTS
      selection_render_points (select);
#endif
    }
  else
    {
      select->segs_in = NULL;
    }

  /*  Possible secondary boundary representation  */
  if (select->num_segs_out)
    {
      select->segs_out = g_new (GdkSegment, select->num_segs_out);
      selection_transform_segs (select, segs_out, select->segs_out,
				select->num_segs_out);
    }
  else
    {
      select->segs_out = NULL;
    }

  /*  The active layer's boundary  */
  gimp_image_layer_boundary (gdisp->gimage, &segs_layer,
			     &select->num_segs_layer);

  if (select->num_segs_layer)
    {
      select->segs_layer = g_new (GdkSegment, select->num_segs_layer);
      selection_transform_segs (select, segs_layer, select->segs_layer,
				select->num_segs_layer);
    }
  else
    {
      select->segs_layer = NULL;
    }

  g_free (segs_layer);
}


static void
selection_free_segs (Selection *select)
{
  gint j;

  if (select->segs_in)
    g_free (select->segs_in);
  if (select->segs_out)
    g_free (select->segs_out);
  if (select->segs_layer)
    g_free (select->segs_layer);

  for (j = 0; j < 8; j++)
    {
      if (select->points_in[j])
	g_free (select->points_in[j]);

      select->points_in[j]     = NULL;
      select->num_points_in[j] = 0;
    }

  select->segs_in        = NULL;
  select->num_segs_in    = 0;
  select->segs_out       = NULL;
  select->num_segs_out   = 0;
  select->segs_layer     = NULL;
  select->num_segs_layer = 0;
}


static gboolean
selection_start_marching (gpointer data)
{
  Selection *select;

  select = (Selection *) data;

  /*  if the RECALC bit is set, reprocess the boundaries  */
  if (select->recalc)
    {
      selection_free_segs (select);
      selection_generate_segs (select);

      /* Toggle the RECALC flag */
      select->recalc = FALSE;
    }

  select->index_in    = 0;
  select->index_out   = 0;
  select->index_layer = 0;

  /*  Make sure the state is set to marching  */
  select->state = MARCHING;

  /*  Draw the ants  */
  if (select->cycle)
    cycle_ant_colors (select);
  else
    {
      gdk_gc_set_stipple (select->gc_in, marching_ants[select->index_in]);
      gdk_gc_set_stipple (select->gc_out, marching_ants[select->index_out]);
      gdk_gc_set_stipple (select->gc_layer, marching_ants[select->index_layer]);
    }

  selection_draw (select);

  /*  Reset the timer  */
  select->timeout_id = g_timeout_add (select->speed,
				      selection_march_ants,
				      select);

  return FALSE;
}


static gboolean
selection_march_ants (gpointer data)
{
  Selection *select;

  select = (Selection *) data;

  /*  increment stipple index  */
  select->index_in++;

#ifndef USE_XDRAWPOINTS
  if (select->index_in > 7)
    select->index_in = 0;
#endif

  /*  outside segments do not march, so index does not cycle  */
  select->index_out++;

  /*  layer doesn't march   */
  select->index_layer++;

  /*  Draw the ants  */
  if (select->cycle)
    cycle_ant_colors (select);
  else
    {
#ifndef USE_XDRAWPOINTS
      gdk_gc_set_stipple (select->gc_in, marching_ants[select->index_in]);
#endif

      selection_draw (select);
    }

  return TRUE;
}
