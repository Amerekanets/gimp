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

#include "libgimpwidgets/gimpwidgets.h"

#include "tools-types.h"

#include "base/tile-manager.h"

#include "core/gimpdrawable.h"
#include "core/gimpdrawable-transform.h"
#include "core/gimpimage.h"
#include "core/gimpimage-mask.h"
#include "core/gimpitem-linked.h"
#include "core/gimptoolinfo.h"

#include "widgets/gimpwidgets-utils.h"

#include "display/gimpdisplay.h"

#include "gimpflipoptions.h"
#include "gimpfliptool.h"
#include "gimptoolcontrol.h"

#include "gimp-intl.h"


/*  local function prototypes  */

static void          gimp_flip_tool_class_init    (GimpFlipToolClass *klass);
static void          gimp_flip_tool_init          (GimpFlipTool      *flip_tool);

static void          gimp_flip_tool_modifier_key  (GimpTool          *tool,
                                                   GdkModifierType    key,
                                                   gboolean           press,
						   GdkModifierType    state,
						   GimpDisplay       *gdisp);
static void          gimp_flip_tool_cursor_update (GimpTool          *tool,
                                                   GimpCoords        *coords,
						   GdkModifierType    state,
						   GimpDisplay       *gdisp);

static TileManager * gimp_flip_tool_transform     (GimpTransformTool *tool,
                                                   GimpItem          *item,
						   GimpDisplay       *gdisp);


static GimpTransformToolClass *parent_class = NULL;


/*  public functions  */

void 
gimp_flip_tool_register (GimpToolRegisterCallback  callback,
                         gpointer                  data)
{
  (* callback) (GIMP_TYPE_FLIP_TOOL,
                GIMP_TYPE_FLIP_OPTIONS,
                gimp_flip_options_gui,
                FALSE,
                "gimp-flip-tool",
                _("Flip"),
                _("Flip the layer or selection"),
                N_("/Tools/Transform Tools/Flip"), "<shift>F",
                NULL, "tools/flip.html",
                GIMP_STOCK_TOOL_FLIP,
                data);
}

GType
gimp_flip_tool_get_type (void)
{
  static GType tool_type = 0;

  if (! tool_type)
    {
      static const GTypeInfo tool_info =
      {
        sizeof (GimpFlipToolClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_flip_tool_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data     */
	sizeof (GimpFlipTool),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_flip_tool_init,
      };

      tool_type = g_type_register_static (GIMP_TYPE_TRANSFORM_TOOL,
					  "GimpFlipTool", 
                                          &tool_info, 0);
    }

  return tool_type;
}


/*  private functions  */

static void
gimp_flip_tool_class_init (GimpFlipToolClass *klass)
{
  GimpToolClass          *tool_class;
  GimpTransformToolClass *trans_class;

  tool_class   = GIMP_TOOL_CLASS (klass);
  trans_class  = GIMP_TRANSFORM_TOOL_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  tool_class->modifier_key  = gimp_flip_tool_modifier_key;
  tool_class->cursor_update = gimp_flip_tool_cursor_update;

  trans_class->transform    = gimp_flip_tool_transform;
}

static void
gimp_flip_tool_init (GimpFlipTool *flip_tool)
{
  GimpTool          *tool;
  GimpTransformTool *transform_tool;

  tool           = GIMP_TOOL (flip_tool);
  transform_tool = GIMP_TRANSFORM_TOOL (flip_tool);

  gimp_tool_control_set_snap_to            (tool->control, FALSE);
  gimp_tool_control_set_cursor             (tool->control,
                                            GDK_SB_H_DOUBLE_ARROW);
  gimp_tool_control_set_tool_cursor        (tool->control,
                                            GIMP_FLIP_HORIZONTAL_TOOL_CURSOR);
  gimp_tool_control_set_toggle_cursor      (tool->control,
                                            GDK_SB_V_DOUBLE_ARROW);
  gimp_tool_control_set_toggle_tool_cursor (tool->control,
                                            GIMP_FLIP_VERTICAL_TOOL_CURSOR);

  transform_tool->use_grid = FALSE;
}

static void
gimp_flip_tool_modifier_key (GimpTool        *tool,
                             GdkModifierType  key,
                             gboolean         press,
			     GdkModifierType  state,
			     GimpDisplay     *gdisp)
{
  GimpFlipOptions *options;

  options = GIMP_FLIP_OPTIONS (tool->tool_info->tool_options);

  if (key == GDK_CONTROL_MASK)
    {
      switch (options->flip_type)
        {
        case GIMP_ORIENTATION_HORIZONTAL:
          g_object_set (options,
                        "flip-type", GIMP_ORIENTATION_VERTICAL,
                        NULL);
          break;

        case GIMP_ORIENTATION_VERTICAL:
          g_object_set (options,
                        "flip-type", GIMP_ORIENTATION_HORIZONTAL,
                        NULL);
          break;

        default:
          break;
	}
    }
}

static void
gimp_flip_tool_cursor_update (GimpTool        *tool,
                              GimpCoords      *coords,
			      GdkModifierType  state,
			      GimpDisplay     *gdisp)
{
  GimpFlipOptions *options;
  gboolean         bad_cursor = TRUE;

  options = GIMP_FLIP_OPTIONS (tool->tool_info->tool_options);

  if (gimp_display_coords_in_active_drawable (gdisp, coords))
    {
      /*  Is there a selected region? If so, is cursor inside? */
      if (gimp_image_mask_is_empty (gdisp->gimage) ||
          gimp_image_mask_value (gdisp->gimage, coords->x, coords->y))
        {
          bad_cursor = FALSE;
        }
    }

  if (bad_cursor)
    {
      gimp_tool_control_set_cursor (tool->control, GIMP_BAD_CURSOR);
      gimp_tool_control_set_toggle_cursor (tool->control, GIMP_BAD_CURSOR);
    }
  else
    {
      gimp_tool_control_set_cursor (tool->control, GDK_SB_H_DOUBLE_ARROW);
      gimp_tool_control_set_toggle_cursor (tool->control, GDK_SB_V_DOUBLE_ARROW);
    }

  gimp_tool_control_set_toggle (tool->control,
                                options->flip_type == GIMP_ORIENTATION_VERTICAL);

  GIMP_TOOL_CLASS (parent_class)->cursor_update (tool, coords, state, gdisp);
}

static TileManager *
gimp_flip_tool_transform (GimpTransformTool *trans_tool,
                          GimpItem          *active_item,
			  GimpDisplay       *gdisp)
{
  GimpTransformOptions *tr_options;
  GimpFlipOptions      *options;
  gdouble               axis = 0.0;
  TileManager          *ret  = NULL;

  options = GIMP_FLIP_OPTIONS (GIMP_TOOL (trans_tool)->tool_info->tool_options);
  tr_options = GIMP_TRANSFORM_OPTIONS (options);

  switch (options->flip_type)
    {
    case GIMP_ORIENTATION_HORIZONTAL:
      axis = ((gdouble) trans_tool->x1 +
              (gdouble) (trans_tool->x2 - trans_tool->x1) / 2.0);
      break;

    case GIMP_ORIENTATION_VERTICAL:
      axis = ((gdouble) trans_tool->y1 +
              (gdouble) (trans_tool->y2 - trans_tool->y1) / 2.0);
      break;

    default:
      break;
    }

  if (gimp_item_get_linked (active_item))
    gimp_item_linked_flip (active_item, options->flip_type, axis, FALSE);

  switch (tr_options->type)
    {
    case GIMP_TRANSFORM_TYPE_LAYER:
    case GIMP_TRANSFORM_TYPE_SELECTION:
      ret = gimp_drawable_transform_tiles_flip (GIMP_DRAWABLE (active_item),
                                                trans_tool->original,
                                                options->flip_type, axis,
                                                FALSE);
      break;

    case GIMP_TRANSFORM_TYPE_PATH:
      /* TODO */
      break;
    }

  return ret;
}
