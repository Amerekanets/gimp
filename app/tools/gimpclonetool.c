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

#include "libgimpwidgets/gimpwidgets.h"

#include "tools-types.h"

#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimpimage-mask.h"
#include "core/gimptoolinfo.h"

#include "paint/gimpclone.h"
#include "paint/gimpcloneoptions.h"

#include "widgets/gimphelp-ids.h"
#include "widgets/gimppropwidgets.h"

#include "display/gimpdisplay.h"

#include "gimpclonetool.h"
#include "gimppaintoptions-gui.h"
#include "gimptoolcontrol.h"

#include "gimp-intl.h"


#define TARGET_WIDTH  15
#define TARGET_HEIGHT 15


static void   gimp_clone_tool_class_init       (GimpCloneToolClass *klass);
static void   gimp_clone_tool_init             (GimpCloneTool      *tool);

static GObject * gimp_clone_tool_constructor   (GType            type,
                                                guint            n_params,
                                                GObjectConstructParam *params);
static void   gimp_clone_tool_button_press     (GimpTool        *tool,
                                                GimpCoords      *coords,
                                                guint32          time,
                                                GdkModifierType  state,
                                                GimpDisplay     *gdisp);
static void   gimp_clone_tool_motion           (GimpTool        *tool,
                                                GimpCoords      *coords,
                                                guint32          time,
                                                GdkModifierType  state,
                                                GimpDisplay     *gdisp);

static void   gimp_clone_tool_cursor_update    (GimpTool        *tool,
                                                GimpCoords      *coords,
                                                GdkModifierType  state,
                                                GimpDisplay     *gdisp);

static void   gimp_clone_tool_draw             (GimpDrawTool    *draw_tool);

static void   gimp_clone_init_callback         (GimpClone       *clone,
                                                gpointer         data);
static void   gimp_clone_finish_callback       (GimpClone       *clone,
                                                gpointer         data);
static void   gimp_clone_pretrace_callback     (GimpClone       *clone,
                                                gpointer         data);
static void   gimp_clone_posttrace_callback    (GimpClone       *clone,
                                                gpointer         data);

static GtkWidget * gimp_clone_options_gui      (GimpToolOptions *tool_options);


static GimpPaintToolClass *parent_class;


/* public functions  */

void
gimp_clone_tool_register (GimpToolRegisterCallback  callback,
                          gpointer                  data)
{
  (* callback) (GIMP_TYPE_CLONE_TOOL,
                GIMP_TYPE_CLONE_OPTIONS,
                gimp_clone_options_gui,
                GIMP_PAINT_OPTIONS_CONTEXT_MASK |
                GIMP_CONTEXT_PATTERN_MASK,
                "gimp-clone-tool",
                _("Clone"),
                _("Paint using Patterns or Image Regions"),
                N_("/Tools/Paint Tools/_Clone"), "C",
                NULL, GIMP_HELP_TOOL_CLONE,
                GIMP_STOCK_TOOL_CLONE,
                data);
}

GType
gimp_clone_tool_get_type (void)
{
  static GType tool_type = 0;

  if (! tool_type)
    {
      static const GTypeInfo tool_info =
      {
        sizeof (GimpCloneToolClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gimp_clone_tool_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data     */
        sizeof (GimpCloneTool),
        0,              /* n_preallocs    */
        (GInstanceInitFunc) gimp_clone_tool_init,
      };

      tool_type = g_type_register_static (GIMP_TYPE_PAINT_TOOL,
                                          "GimpCloneTool",
                                          &tool_info, 0);
    }

  return tool_type;
}

/* static functions  */

static void
gimp_clone_tool_class_init (GimpCloneToolClass *klass)
{
  GObjectClass      *object_class;
  GimpToolClass     *tool_class;
  GimpDrawToolClass *draw_tool_class;

  object_class    = G_OBJECT_CLASS (klass);
  tool_class      = GIMP_TOOL_CLASS (klass);
  draw_tool_class = GIMP_DRAW_TOOL_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->constructor = gimp_clone_tool_constructor;

  tool_class->button_press  = gimp_clone_tool_button_press;
  tool_class->motion        = gimp_clone_tool_motion;
  tool_class->cursor_update = gimp_clone_tool_cursor_update;

  draw_tool_class->draw     = gimp_clone_tool_draw;
}

static void
gimp_clone_tool_init (GimpCloneTool *clone)
{
  GimpTool      *tool;
  GimpPaintTool *paint_tool;

  tool       = GIMP_TOOL (clone);
  paint_tool = GIMP_PAINT_TOOL (clone);

  gimp_tool_control_set_tool_cursor (tool->control, GIMP_CLONE_TOOL_CURSOR);
}

static GObject *
gimp_clone_tool_constructor (GType                  type,
                             guint                  n_params,
                             GObjectConstructParam *params)
{
  GObject       *object;
  GimpPaintTool *paint_tool;
  GimpClone     *clone_core;

  object = G_OBJECT_CLASS (parent_class)->constructor (type, n_params, params);

  paint_tool = GIMP_PAINT_TOOL (object);

  clone_core = GIMP_CLONE (paint_tool->core);

  clone_core->init_callback      = gimp_clone_init_callback;
  clone_core->finish_callback    = gimp_clone_finish_callback;
  clone_core->pretrace_callback  = gimp_clone_pretrace_callback;
  clone_core->posttrace_callback = gimp_clone_posttrace_callback;
  clone_core->callback_data      = object;

  return object;
}

static void
gimp_clone_tool_button_press (GimpTool        *tool,
                              GimpCoords      *coords,
                              guint32          time,
                              GdkModifierType  state,
                              GimpDisplay     *gdisp)
{
  GimpPaintTool *paint_tool;

  paint_tool = GIMP_PAINT_TOOL (tool);

  if (state & GDK_CONTROL_MASK)
    GIMP_CLONE (paint_tool->core)->set_source = TRUE;
  else
    GIMP_CLONE (paint_tool->core)->set_source = FALSE;

  GIMP_TOOL_CLASS (parent_class)->button_press (tool, coords, time, state,
                                                gdisp);
}

static void
gimp_clone_tool_motion (GimpTool        *tool,
                        GimpCoords      *coords,
                        guint32          time,
                        GdkModifierType  state,
                        GimpDisplay     *gdisp)
{
  GimpPaintTool *paint_tool;

  paint_tool = GIMP_PAINT_TOOL (tool);

  if (state & GDK_CONTROL_MASK)
    GIMP_CLONE (paint_tool->core)->set_source = TRUE;
  else
    GIMP_CLONE (paint_tool->core)->set_source = FALSE;

  GIMP_TOOL_CLASS (parent_class)->motion (tool, coords, time, state, gdisp);
}

void
gimp_clone_tool_cursor_update (GimpTool        *tool,
                               GimpCoords      *coords,
			       GdkModifierType  state,
			       GimpDisplay     *gdisp)
{
  GimpCloneOptions *options;
  GdkCursorType     ctype = GIMP_MOUSE_CURSOR;

  options = (GimpCloneOptions *) tool->tool_info->tool_options;

  if (gimp_display_coords_in_active_drawable (gdisp, coords))
    {
      /*  One more test--is there a selected region?
       *  if so, is cursor inside?
       */
      if (gimp_image_mask_is_empty (gdisp->gimage))
        ctype = GIMP_MOUSE_CURSOR;
      else if (gimp_image_mask_value (gdisp->gimage, coords->x, coords->y))
        ctype = GIMP_MOUSE_CURSOR;
    }

  if (options->clone_type == GIMP_IMAGE_CLONE)
    {
      if (state & GDK_CONTROL_MASK)
	ctype = GIMP_CROSSHAIR_SMALL_CURSOR;
      else if (! GIMP_CLONE (GIMP_PAINT_TOOL (tool)->core)->src_drawable)
	ctype = GIMP_BAD_CURSOR;
    }

  gimp_tool_control_set_cursor (tool->control, ctype);

  GIMP_TOOL_CLASS (parent_class)->cursor_update (tool, coords, state, gdisp);
}

static void
gimp_clone_tool_draw (GimpDrawTool *draw_tool)
{
  GimpTool *tool;

  tool = GIMP_TOOL (draw_tool);

  if (gimp_tool_control_is_active (tool->control))
    {
      GimpCloneOptions *options;

      options = (GimpCloneOptions *) tool->tool_info->tool_options;

      if (draw_tool->gdisp && options->clone_type == GIMP_IMAGE_CLONE)
        {
          GimpClone *clone;

          clone = GIMP_CLONE (GIMP_PAINT_TOOL (draw_tool)->core);

          if (clone->src_drawable)
            {
              gint off_x;
              gint off_y;

              gimp_item_offsets (GIMP_ITEM (clone->src_drawable),
                                 &off_x, &off_y);

              gimp_draw_tool_draw_handle (draw_tool,
                                          GIMP_HANDLE_CROSS,
                                          clone->src_x + off_x,
                                          clone->src_y + off_y,
                                          TARGET_WIDTH, TARGET_WIDTH,
                                          GTK_ANCHOR_CENTER,
                                          FALSE);
            }
        }
    }
  else
    {
      GIMP_DRAW_TOOL_CLASS (parent_class)->draw (draw_tool);
    }
}

static void
gimp_clone_init_callback (GimpClone *clone,
                          gpointer   data)
{
  gimp_draw_tool_start (GIMP_DRAW_TOOL (data),
                        GIMP_TOOL (data)->gdisp);
}

static void
gimp_clone_finish_callback (GimpClone *clone,
                            gpointer   data)
{
  gimp_draw_tool_stop (GIMP_DRAW_TOOL (data));
}

static void
gimp_clone_pretrace_callback (GimpClone *clone,
                              gpointer   data)
{
  gimp_draw_tool_pause (GIMP_DRAW_TOOL (data));
}

static void
gimp_clone_posttrace_callback (GimpClone *clone,
                               gpointer   data)
{
  gimp_draw_tool_resume (GIMP_DRAW_TOOL (data));
}


/*  tool options stuff  */

static GtkWidget *
gimp_clone_options_gui (GimpToolOptions *tool_options)
{
  GObject   *config;
  GtkWidget *vbox;
  GtkWidget *frame;

  config = G_OBJECT (tool_options);

  vbox = gimp_paint_options_gui (tool_options);

  frame = gimp_prop_enum_radio_frame_new (config, "clone-type",
                                          _("Source"),
                                          0, 0);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  frame = gimp_prop_enum_radio_frame_new (config, "align-mode",
                                          _("Alignment"),
                                          0, 0);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  return vbox;
}
