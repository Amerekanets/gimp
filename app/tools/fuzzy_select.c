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

#include <stdlib.h>

#include <gtk/gtk.h>

#include "libgimpmath/gimpmath.h"

#include "apptypes.h"

#include "boundary.h"
#include "channel.h"
#include "cursorutil.h"
#include "draw_core.h"
#include "drawable.h"
#include "edit_selection.h"
#include "gimage_mask.h"
#include "gimpimage.h"
#include "gimprc.h"
#include "gdisplay.h"
#include "pixel_region.h"
#include "tile_manager.h"
#include "tile.h"

#include "fuzzy_select.h"
#include "rect_select.h"
#include "selection_options.h"
#include "tool_options.h"
#include "tools.h"

#include "libgimp/gimpintl.h"


/*  the fuzzy selection structures  */

typedef struct _FuzzySelect FuzzySelect;

struct _FuzzySelect
{
  DrawCore *core;       /*  Core select object                      */

  gint      op;         /*  selection operation (ADD, SUB, etc)     */

  gint      current_x;  /*  these values are updated on every motion event  */
  gint      current_y;  /*  (enables immediate cursor updating on modifier
			 *   key events).  */

  gint      x, y;             /*  Point from which to execute seed fill  */
  gint      first_x;          /*                                         */
  gint      first_y;          /*  variables to keep track of sensitivity */
  gdouble   first_threshold;  /* initial value of threshold slider   */
};


/*  fuzzy select action functions  */
static void         fuzzy_select_button_press    (Tool           *tool,
						  GdkEventButton *bevent,
						  GDisplay       *gdisp);
static void         fuzzy_select_button_release  (Tool           *tool,
						  GdkEventButton *bevent,
						  GDisplay       *gdisp);
static void         fuzzy_select_motion          (Tool           *tool,
						  GdkEventMotion *mevent,
						  GDisplay       *gdisp);
static void         fuzzy_select_control         (Tool           *tool,
						  ToolAction      tool_action,
						  GDisplay       *gdisp);

static void         fuzzy_select_draw            (Tool           *tool);

static GdkSegment * fuzzy_select_calculate       (Tool           *tool,
						  GDisplay       *gdisp,
						  gint           *nsegs);


/*  the fuzzy selection tool options  */
static SelectionOptions  *fuzzy_options = NULL;

/*  XSegments which make up the fuzzy selection boundary  */
static GdkSegment *segs     = NULL;
static gint        num_segs = 0;

Channel * fuzzy_mask = NULL;


/*************************************/
/*  Fuzzy selection apparatus  */

static gint
is_pixel_sufficiently_different (guchar   *col1, 
				 guchar   *col2,
				 gboolean  antialias, 
				 gint      threshold, 
				 gint      bytes,
				 gboolean  has_alpha)
{
  gint diff;
  gint max;
  gint b;
  gint alpha;

  max = 0;
  alpha = (has_alpha) ? bytes - 1 : bytes;

  /*  if there is an alpha channel, never select transparent regions  */
  if (has_alpha && col2[alpha] == 0)
    return 0;

  for (b = 0; b < bytes; b++)
    {
      diff = col1[b] - col2[b];
      diff = abs (diff);
      if (diff > max)
	max = diff;
    }

  if (antialias)
    {
      float aa;

      aa = 1.5 - ((float) max / threshold);
      if (aa <= 0)
	return 0;
      else if (aa < 0.5)
	return (unsigned char) (aa * 512);
      else
	return 255;
    }
  else
    {
      if (max > threshold)
	return 0;
      else
	return 255;
    }
}

static void
ref_tiles (TileManager  *src, 
	   TileManager  *mask, 
	   Tile        **s_tile, 
	   Tile        **m_tile,
	   gint          x, 
	   gint          y, 
	   guchar      **s, 
	   guchar      **m)
{
  if (*s_tile != NULL)
    tile_release (*s_tile, FALSE);
  if (*m_tile != NULL)
    tile_release (*m_tile, TRUE);

  *s_tile = tile_manager_get_tile (src, x, y, TRUE, FALSE);
  *m_tile = tile_manager_get_tile (mask, x, y, TRUE, TRUE);

  *s = tile_data_pointer (*s_tile, x % TILE_WIDTH, y % TILE_HEIGHT);
  *m = tile_data_pointer (*m_tile, x % TILE_WIDTH, y % TILE_HEIGHT);
}

static int
find_contiguous_segment (guchar      *col, 
			 PixelRegion *src,
			 PixelRegion *mask, 
			 gint         width, 
			 gint         bytes,
			 gboolean     has_alpha, 
			 gboolean     antialias, 
			 gint         threshold,
			 gint         initial, 
			 gint        *start, 
			 gint        *end)
{
  guchar *s;
  guchar *m;
  guchar  diff;
  Tile   *s_tile = NULL;
  Tile   *m_tile = NULL;

  ref_tiles (src->tiles, mask->tiles, &s_tile, &m_tile, src->x, src->y, &s, &m);

  /* check the starting pixel */
  if (! (diff = is_pixel_sufficiently_different (col, s, antialias,
						 threshold, bytes, has_alpha)))
    {
      tile_release (s_tile, FALSE);
      tile_release (m_tile, TRUE);
      return FALSE;
    }

  *m-- = diff;
  s -= bytes;
  *start = initial - 1;

  while (*start >= 0 && diff)
    {
      if (! ((*start + 1) % TILE_WIDTH))
	ref_tiles (src->tiles, mask->tiles, &s_tile, &m_tile, *start, src->y, &s, &m);

      diff = is_pixel_sufficiently_different (col, s, antialias,
					      threshold, bytes, has_alpha);
      if ((*m-- = diff))
	{
	  s -= bytes;
	  (*start)--;
	}
    }

  diff = 1;
  *end = initial + 1;
  if (*end % TILE_WIDTH && *end < width)
    ref_tiles (src->tiles, mask->tiles, &s_tile, &m_tile, *end, src->y, &s, &m);

  while (*end < width && diff)
    {
      if (! (*end % TILE_WIDTH))
	ref_tiles (src->tiles, mask->tiles, &s_tile, &m_tile, *end, src->y, &s, &m);

      diff = is_pixel_sufficiently_different (col, s, antialias,
					      threshold, bytes, has_alpha);
      if ((*m++ = diff))
	{
	  s += bytes;
	  (*end)++;
	}
    }

  tile_release (s_tile, FALSE);
  tile_release (m_tile, TRUE);
  return TRUE;
}

static void
find_contiguous_region_helper (PixelRegion *mask, 
			       PixelRegion *src,
			       gboolean     has_alpha, 
			       gboolean     antialias, 
			       gint         threshold, 
			       gboolean     indexed,
			       gint         x, 
			       gint         y, 
			       guchar      *col)
{
  gint start, end, i;
  gint val;
  gint bytes;

  Tile *tile;

  if (threshold == 0) threshold = 1;
  if (x < 0 || x >= src->w) return;
  if (y < 0 || y >= src->h) return;

  tile = tile_manager_get_tile (mask->tiles, x, y, TRUE, FALSE);
  val = *(guchar *)(tile_data_pointer (tile, 
				       x % TILE_WIDTH, y % TILE_HEIGHT));
  tile_release (tile, FALSE);
  if (val != 0)
    return;

  src->x = x;
  src->y = y;

  bytes = src->bytes;
  if(indexed)
    {
      bytes = has_alpha ? 4 : 3;
    }

  if (! find_contiguous_segment (col, src, mask, src->w,
				 src->bytes, has_alpha,
				 antialias, threshold, x, &start, &end))
    return;

  for (i = start + 1; i < end; i++)
    {
      find_contiguous_region_helper (mask, src, has_alpha, antialias, 
				     threshold, indexed, i, y - 1, col);
      find_contiguous_region_helper (mask, src, has_alpha, antialias, 
				     threshold, indexed, i, y + 1, col);
    }
}

Channel *
find_contiguous_region (GImage       *gimage, 
			GimpDrawable *drawable, 
			gboolean      antialias,
			gint          threshold, 
			gint          x, 
			gint          y, 
			gboolean      sample_merged)
{
  PixelRegion srcPR, maskPR;
  Channel  *mask;
  guchar   *start;
  gboolean  has_alpha;
  gboolean  indexed;
  gint      type;
  gint      bytes;
  Tile     *tile;

  if (sample_merged)
    {
      pixel_region_init (&srcPR, gimp_image_composite (gimage), 0, 0,
			 gimage->width, gimage->height, FALSE);
      type = gimp_image_composite_type (gimage);
      has_alpha = (type == RGBA_GIMAGE ||
		   type == GRAYA_GIMAGE ||
		   type == INDEXEDA_GIMAGE);
    }
  else
    {
      pixel_region_init (&srcPR, gimp_drawable_data (drawable),
			 0, 0,
			 gimp_drawable_width (drawable),
			 gimp_drawable_height (drawable),
			 FALSE);
      has_alpha = gimp_drawable_has_alpha (drawable);
    }
  indexed = gimp_drawable_is_indexed (drawable);
  bytes   = gimp_drawable_bytes (drawable);
  
  if (indexed)
    {
      bytes = has_alpha ? 4 : 3;
    }
  mask = channel_new_mask (gimage, srcPR.w, srcPR.h);
  pixel_region_init (&maskPR, gimp_drawable_data (GIMP_DRAWABLE(mask)),
		     0, 0, 
		     gimp_drawable_width (GIMP_DRAWABLE(mask)), 
		     gimp_drawable_height (GIMP_DRAWABLE(mask)), 
		     TRUE);

  tile = tile_manager_get_tile (srcPR.tiles, x, y, TRUE, FALSE);
  if (tile)
    {
      start = tile_data_pointer (tile, x%TILE_WIDTH, y%TILE_HEIGHT);

      find_contiguous_region_helper (&maskPR, &srcPR, has_alpha, antialias, 
				     threshold, bytes, x, y, start);

      tile_release (tile, FALSE);
    }

  return mask;
}

void
fuzzy_select (GImage       *gimage, 
	      GimpDrawable *drawable, 
	      gint          op, 
	      gboolean      feather,
	      gdouble       feather_radius)
{
  gint off_x, off_y;

  /*  if applicable, replace the current selection  */
  if (op == CHANNEL_OP_REPLACE)
    gimage_mask_clear (gimage);
  else
    gimage_mask_undo (gimage);

  if (drawable)     /* NULL if sample_merged is active */
    gimp_drawable_offsets (drawable, &off_x, &off_y);
  else
    off_x = off_y = 0;
  
  if (feather)
    channel_feather (fuzzy_mask, gimp_image_get_mask (gimage),
		     feather_radius,
		     feather_radius,
		     op, off_x, off_y);
  else
    channel_combine_mask (gimp_image_get_mask (gimage),
			  fuzzy_mask, op, off_x, off_y);

  /*  free the fuzzy region struct  */
  channel_delete (fuzzy_mask);
  fuzzy_mask = NULL;
}

/*  fuzzy select action functions  */

static void
fuzzy_select_button_press (Tool           *tool, 
			   GdkEventButton *bevent,
			   GDisplay       *gdisp)
{
  FuzzySelect *fuzzy_sel;

  fuzzy_sel = (FuzzySelect *) tool->private;

  fuzzy_sel->x = bevent->x;
  fuzzy_sel->y = bevent->y;
  fuzzy_sel->first_x = fuzzy_sel->x;
  fuzzy_sel->first_y = fuzzy_sel->y;
  fuzzy_sel->first_threshold = fuzzy_options->threshold;

  gdk_pointer_grab (gdisp->canvas->window, FALSE,
		    GDK_POINTER_MOTION_HINT_MASK |
		    GDK_BUTTON1_MOTION_MASK |
		    GDK_BUTTON_RELEASE_MASK,
		    NULL, NULL, bevent->time);

  tool->state = ACTIVE;
  tool->gdisp = gdisp;

  if (fuzzy_sel->op == SELECTION_MOVE_MASK)
    {
      init_edit_selection (tool, gdisp, bevent, EDIT_MASK_TRANSLATE);
      return;
    }
  else if (fuzzy_sel->op == SELECTION_MOVE)
    {
      init_edit_selection (tool, gdisp, bevent, EDIT_MASK_TO_LAYER_TRANSLATE);
      return;
    }

  /*  calculate the region boundary  */
  segs = fuzzy_select_calculate (tool, gdisp, &num_segs);

  draw_core_start (fuzzy_sel->core,
		   gdisp->canvas->window,
		   tool);
}

static void
fuzzy_select_button_release (Tool           *tool, 
			     GdkEventButton *bevent,
			     GDisplay       *gdisp)
{
  FuzzySelect  *fuzzy_sel;
  GimpDrawable *drawable;

  fuzzy_sel = (FuzzySelect *) tool->private;

  gdk_pointer_ungrab (bevent->time);
  gdk_flush ();

  draw_core_stop (fuzzy_sel->core, tool);
  tool->state = INACTIVE;

  /*  First take care of the case where the user "cancels" the action  */
  if (! (bevent->state & GDK_BUTTON3_MASK))
    {
      drawable = (fuzzy_options->sample_merged ?
		  NULL : gimp_image_active_drawable (gdisp->gimage));

      fuzzy_select (gdisp->gimage, drawable, fuzzy_sel->op,
		    fuzzy_options->feather, 
		    fuzzy_options->feather_radius);
      gdisplays_flush ();
    }

  /*  If the segment array is allocated, free it  */
  if (segs)
    g_free (segs);
  segs = NULL;
}

static void
fuzzy_select_motion (Tool           *tool, 
		     GdkEventMotion *mevent, 
		     GDisplay       *gdisp)
{
  FuzzySelect *fuzzy_sel;
  GdkSegment  *new_segs;
  gint         num_new_segs;
  gint         diff_x, diff_y;
  gdouble      diff;

  static guint last_time = 0;

  fuzzy_sel = (FuzzySelect *) tool->private;

  /*  needed for immediate cursor update on modifier event  */
  fuzzy_sel->current_x = mevent->x;
  fuzzy_sel->current_y = mevent->y;

  if (tool->state != ACTIVE)
    return;

  /* don't let the events come in too fast, ignore below a delay of 100 ms */
  if (ABS (mevent->time - last_time) < 100)
    return;
  
  last_time = mevent->time;

  diff_x = mevent->x - fuzzy_sel->first_x;
  diff_y = mevent->y - fuzzy_sel->first_y;

  diff = ((ABS (diff_x) > ABS (diff_y)) ? diff_x : diff_y) / 2.0;

  gtk_adjustment_set_value (GTK_ADJUSTMENT (fuzzy_options->threshold_w), 
			    fuzzy_sel->first_threshold + diff);
      
  /*  calculate the new fuzzy boundary  */
  new_segs = fuzzy_select_calculate (tool, gdisp, &num_new_segs);

  /*  stop the current boundary  */
  draw_core_pause (fuzzy_sel->core, tool);

  /*  make sure the XSegment array is freed before we assign the new one  */
  if (segs)
    g_free (segs);
  segs = new_segs;
  num_segs = num_new_segs;

  /*  start the new boundary  */
  draw_core_resume (fuzzy_sel->core, tool);
}


static GdkSegment *
fuzzy_select_calculate (Tool     *tool, 
			GDisplay *gdisp,
			gint     *nsegs)
{
  PixelRegion   maskPR;
  FuzzySelect  *fuzzy_sel;
  Channel      *new;
  GdkSegment   *segs;
  BoundSeg     *bsegs;
  GimpDrawable *drawable;
  gint          i;
  gint          x, y;
  gboolean      use_offsets;

  fuzzy_sel = (FuzzySelect *) tool->private;
  drawable  = gimp_image_active_drawable (gdisp->gimage);

  gimp_add_busy_cursors ();

  use_offsets = fuzzy_options->sample_merged ? FALSE : TRUE;

  gdisplay_untransform_coords (gdisp, fuzzy_sel->x,
			       fuzzy_sel->y, &x, &y, FALSE, use_offsets);

  new = find_contiguous_region (gdisp->gimage, drawable, 
				fuzzy_options->antialias,
				fuzzy_options->threshold, x, y, 
				fuzzy_options->sample_merged);

  if (fuzzy_mask)
    gtk_object_unref (GTK_OBJECT (fuzzy_mask));

  fuzzy_mask = new;

  gtk_object_ref (GTK_OBJECT (fuzzy_mask));
  gtk_object_sink (GTK_OBJECT (fuzzy_mask));

  /*  calculate and allocate a new XSegment array which represents the boundary
   *  of the color-contiguous region
   */
  pixel_region_init (&maskPR, gimp_drawable_data (GIMP_DRAWABLE (fuzzy_mask)),
		     0, 0, 
		     gimp_drawable_width (GIMP_DRAWABLE (fuzzy_mask)), 
		     gimp_drawable_height (GIMP_DRAWABLE (fuzzy_mask)), 
		     FALSE);
  bsegs = find_mask_boundary (&maskPR, nsegs, WithinBounds,
			      0, 0,
			      gimp_drawable_width (GIMP_DRAWABLE (fuzzy_mask)),
			      gimp_drawable_height (GIMP_DRAWABLE (fuzzy_mask)));

  segs = g_new (GdkSegment, *nsegs);

  for (i = 0; i < *nsegs; i++)
    {
      gdisplay_transform_coords (gdisp, bsegs[i].x1, bsegs[i].y1, &x, &y, use_offsets);
      segs[i].x1 = x;  segs[i].y1 = y;
      gdisplay_transform_coords (gdisp, bsegs[i].x2, bsegs[i].y2, &x, &y, use_offsets);
      segs[i].x2 = x;  segs[i].y2 = y;
    }

  /*  free boundary segments  */
  g_free (bsegs);

  gimp_remove_busy_cursors (NULL);

  return segs;
}

static void
fuzzy_select_draw (Tool *tool)
{
  FuzzySelect *fuzzy_sel;

  fuzzy_sel = (FuzzySelect *) tool->private;

  if (segs)
    gdk_draw_segments (fuzzy_sel->core->win, fuzzy_sel->core->gc, segs, num_segs);
}

static void
fuzzy_select_control (Tool       *tool,
		      ToolAction  action,
		      GDisplay   *gdisp)
{
  FuzzySelect *fuzzy_sel;

  fuzzy_sel = (FuzzySelect *) tool->private;

  switch (action)
    {
    case PAUSE :
      draw_core_pause (fuzzy_sel->core, tool);
      break;

    case RESUME :
      draw_core_resume (fuzzy_sel->core, tool);
      break;

    case HALT :
      draw_core_stop (fuzzy_sel->core, tool);
      break;

    default:
      break;
    }
}

static void
fuzzy_select_options_reset (void)
{
  selection_options_reset (fuzzy_options);
}

Tool *
tools_new_fuzzy_select (void)
{
  Tool        *tool;
  FuzzySelect *private;

  /*  The tool options  */
  if (! fuzzy_options)
    {
      fuzzy_options = selection_options_new (FUZZY_SELECT,
					     fuzzy_select_options_reset);
      tools_register (FUZZY_SELECT, (ToolOptions *) fuzzy_options);
    }

  tool = tools_new_tool (FUZZY_SELECT);
  private = g_new0 (FuzzySelect, 1);

  private->core = draw_core_new (fuzzy_select_draw);

  tool->scroll_lock = TRUE;  /*  Disallow scrolling  */

  tool->private = (void *) private;

  tool->button_press_func   = fuzzy_select_button_press;
  tool->button_release_func = fuzzy_select_button_release;
  tool->motion_func         = fuzzy_select_motion;
  tool->modifier_key_func   = rect_select_modifier_update;
  tool->cursor_update_func  = rect_select_cursor_update;
  tool->oper_update_func    = rect_select_oper_update;
  tool->control_func        = fuzzy_select_control;

  return tool;
}

void
tools_free_fuzzy_select (Tool *tool)
{
  FuzzySelect *fuzzy_sel;

  fuzzy_sel = (FuzzySelect *) tool->private;
  draw_core_free (fuzzy_sel->core);
  g_free (fuzzy_sel);
}
