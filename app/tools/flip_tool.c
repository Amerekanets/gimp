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
#include <gdk/gdkkeysyms.h>

#include "apptypes.h"

#include "appenv.h"
#include "cursorutil.h"
#include "drawable.h"
#include "flip_tool.h"
#include "gdisplay.h"
#include "gimage_mask.h"
#include "gimpimage.h"
#include "gimpui.h"
#include "paint_funcs.h"
#include "path_transform.h"
#include "pixel_region.h"
#include "temp_buf.h"
#include "tile_manager.h"
#include "tile_manager_pvt.h"
#include "tools.h"
#include "tool_options.h"
#include "transform_core.h"

#include "libgimp/gimpintl.h"


#define FLIP_INFO 0

/*  the flip structures  */

typedef struct _FlipOptions FlipOptions;

struct _FlipOptions
{
  ToolOptions              tool_options;

  InternalOrientationType  type;
  InternalOrientationType  type_d;
  GtkWidget               *type_w[2];
};


/*  local function prototypes  */

static TileManager * flip_tool_transform  (Tool           *tool,
					   GDisplay       *gdisp,
					   TransformState  state);


/*  private variables  */

static FlipOptions *flip_options = NULL;

/*  functions  */

/*  FIXME: Lame - 1 hacks abound since the code assumes certain values for
 *  the ORIENTATION_FOO constants.
 */

static void
flip_options_reset (void)
{
  FlipOptions *options = flip_options;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (options->type_w[options->type_d - 1]), TRUE); 
}

static FlipOptions *
flip_options_new (void)
{
  FlipOptions *options;

  GtkWidget *vbox;
  GtkWidget *frame;
 
  /*  the new flip tool options structure  */
  options = g_new (FlipOptions, 1);
  tool_options_init ((ToolOptions *) options,
		     _("Flip Tool"),
		     flip_options_reset);
  options->type = options->type_d = ORIENTATION_HORIZONTAL;

  /*  the main vbox  */
  vbox = options->tool_options.main_vbox;

  /*  tool toggle  */
  frame =
    gimp_radio_group_new2 (TRUE, _("Tool Toggle"),
			   gimp_radio_button_update,
			   &options->type, (gpointer) options->type,

			   _("Horizontal"), (gpointer) ORIENTATION_HORIZONTAL,
			   &options->type_w[0],
			   _("Vertical"), (gpointer) ORIENTATION_VERTICAL,
			   &options->type_w[1],

			   NULL);

  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  return options;
}

static void
flip_modifier_key_func (Tool        *tool,
			GdkEventKey *kevent,
			GDisplay    *gdisp)
{
  switch (kevent->keyval)
    {
    case GDK_Alt_L: case GDK_Alt_R:
      break;
    case GDK_Shift_L: case GDK_Shift_R:
      break;
    case GDK_Control_L: case GDK_Control_R:
      if (flip_options->type == ORIENTATION_HORIZONTAL)
	{
	  gtk_toggle_button_set_active
	    (GTK_TOGGLE_BUTTON (flip_options->type_w[ORIENTATION_VERTICAL - 1]), TRUE);
	}
      else
	{
	  gtk_toggle_button_set_active
	    (GTK_TOGGLE_BUTTON (flip_options->type_w[ORIENTATION_HORIZONTAL - 1]), TRUE);
	}
      break;
    }
}

static TileManager *
flip_tool_transform (Tool           *tool,
                     GDisplay       *gdisp,
                     TransformState  state)
{
  TransformCore *transform_core;

  transform_core = (TransformCore *) tool->private;

  switch (state)
    {
    case TRANSFORM_INIT:
      transform_info = NULL;
      break;

    case TRANSFORM_MOTION:
      break;

    case TRANSFORM_RECALC:
      break;

    case TRANSFORM_FINISH:
      /*      transform_core->trans_info[FLIP] *= -1.0;*/
      return flip_tool_flip (gdisp->gimage,
	  		     gimp_image_active_drawable (gdisp->gimage),
			     transform_core->original, 
			     (int) transform_core->trans_info[FLIP_INFO],
			     flip_options->type);
      break;
    }

  return NULL;
}

static void
flip_cursor_update (Tool           *tool,
		    GdkEventMotion *mevent,
		    GDisplay       *gdisp)
{
  GimpDrawable  *drawable;
  GdkCursorType  ctype = GIMP_BAD_CURSOR;

  if ((drawable = gimp_image_active_drawable (gdisp->gimage)))
    {
      gint x, y;
      gint off_x, off_y;

      drawable_offsets (drawable, &off_x, &off_y);
      gdisplay_untransform_coords (gdisp, 
				   (double) mevent->x, (double) mevent->y,
				   &x, &y, TRUE, FALSE);

      if (x >= off_x && y >= off_y &&
	  x < (off_x + drawable_width (drawable)) &&
	  y < (off_y + drawable_height (drawable)))
	{
	  /*  Is there a selected region? If so, is cursor inside? */
	  if (gimage_mask_is_empty (gdisp->gimage) ||
	      gimage_mask_value (gdisp->gimage, x, y))
	    {
	      if (flip_options->type == ORIENTATION_HORIZONTAL)
		ctype = GDK_SB_H_DOUBLE_ARROW;
	      else
		ctype = GDK_SB_V_DOUBLE_ARROW;
	    }
	}
    }
  gdisplay_install_tool_cursor (gdisp, ctype,
				FLIP,
				CURSOR_MODIFIER_NONE,
				ctype == GDK_SB_V_DOUBLE_ARROW);
}

Tool *
tools_new_flip (void)
{
  Tool          *tool;
  TransformCore *private;

  /*  The tool options  */
  if (! flip_options)
    {
      flip_options = flip_options_new ();
      tools_register (FLIP, (ToolOptions *) flip_options);
    }

  tool    = transform_core_new (FLIP, FALSE);
  private = tool->private;

  private->trans_func            = flip_tool_transform;
  private->trans_info[FLIP_INFO] = -1.0;

  tool->modifier_key_func  = flip_modifier_key_func;
  tool->cursor_update_func = flip_cursor_update;

  return tool;
}

void
tools_free_flip_tool (Tool *tool)
{
  transform_core_free (tool);
}

TileManager *
flip_tool_flip (GimpImage               *gimage,
		GimpDrawable            *drawable,
		TileManager             *orig,
		gint                     flip,
		InternalOrientationType  type)
{
  TileManager *new;
  PixelRegion  srcPR, destPR;
  gint         orig_width;
  gint         orig_height;
  gint         orig_bpp;
  gint         i;

  if (! orig)
    return NULL;

  orig_width  = tile_manager_level_width (orig);
  orig_height = tile_manager_level_height (orig);
  orig_bpp    = tile_manager_level_bpp (orig);

  if (flip > 0)
    {
      new = tile_manager_new (orig_width, orig_height, orig_bpp);
      pixel_region_init (&srcPR, orig,
			 0, 0, orig_width, orig_height, FALSE);
      pixel_region_init (&destPR, new,
			 0, 0, orig_width, orig_height, TRUE);

      copy_region (&srcPR, &destPR);
      new->x = orig->x;
      new->y = orig->y;
    }
  else
    {
      new = tile_manager_new (orig_width, orig_height, orig_bpp);
      new->x = orig->x;
      new->y = orig->y;

      if (type == ORIENTATION_HORIZONTAL)
	for (i = 0; i < orig->width; i++)
	  {
	    pixel_region_init (&srcPR, orig, i, 0, 1, orig_height, FALSE);
	    pixel_region_init (&destPR, new,
			       (orig_width - i - 1), 0, 1, orig_height, TRUE);
	    copy_region (&srcPR, &destPR); 
	  }
      else
	for (i = 0; i < orig->height; i++)
	  {
	    pixel_region_init (&srcPR, orig, 0, i, orig_width, 1, FALSE);
	    pixel_region_init (&destPR, new,
			       0, (orig_height - i - 1), orig_width, 1, TRUE);
	    copy_region (&srcPR, &destPR);
	  }

      /* flip locked paths */
      /* Note that the undo structures etc are setup before we enter this
       * function.
       */
      if (type == ORIENTATION_HORIZONTAL)
	path_transform_flip_horz (gimage);
      else
	path_transform_flip_vert (gimage);
    }

  return new;
}
