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

#include "apptypes.h"

#include "appenv.h"
#include "cursorutil.h"
#include "context_manager.h"
#include "gdisplay.h"
#include "gimpcontainer.h"
#include "gimpcontext.h"
#include "gimpimage.h"
#include "gimprc.h"

#include "tools/paint_options.h"
#include "tools/tools.h"


/*
 *  the list of all images
 */
GimpContainer *image_context = NULL;


static GimpContext *global_tool_context = NULL;

#define PAINT_OPTIONS_MASK GIMP_CONTEXT_OPACITY_MASK | \
                           GIMP_CONTEXT_PAINT_MODE_MASK

static void
context_manager_display_changed (GimpContext *context,
				 GDisplay    *display,
				 gpointer     data)
{
  gdisplay_set_menu_sensitivity (display);
}

static void
context_manager_tool_changed (GimpContext *user_context,
			      ToolType     tool_type,
			      gpointer     data)
{
  /* FIXME: gimp_busy HACK */
  if (gimp_busy)
    {
      /*  there may be contexts waiting for the user_context's "tool_changed"
       *  signal, so stop emitting it.
       */
      gtk_signal_emit_stop_by_name (GTK_OBJECT (user_context), "tool_changed");

      if (active_tool->type != tool_type)
	{
	  gtk_signal_handler_block_by_func (GTK_OBJECT (user_context),
					    context_manager_tool_changed,
					    NULL);

	  /*  explicitly set the current tool  */
	  gimp_context_set_tool (user_context, active_tool->type);

	  gtk_signal_handler_unblock_by_func (GTK_OBJECT (user_context),
					      context_manager_tool_changed,
					      NULL);
	}

      /*  take care that the correct toolbox button gets re-activated  */
      tool_type = active_tool->type;
    }
  else
    {
      GimpContext* tool_context;

      if (! global_paint_options)
	{
	  if (active_tool &&
	      (tool_context = tool_info[active_tool->type].tool_context))
	    {
	      gimp_context_unset_parent (tool_context);
	    }

	  if ((tool_context = tool_info[tool_type].tool_context))
	    {
	      gimp_context_copy_args (tool_context, user_context,
				      PAINT_OPTIONS_MASK);
	      gimp_context_set_parent (tool_context, user_context);
	    }
	}

      tools_select (tool_type);
    }

  if (tool_type == SCALE ||
      tool_type == SHEAR ||
      tool_type == PERSPECTIVE)
    tool_type = ROTATE;

  if (! GTK_TOGGLE_BUTTON (tool_info[tool_type].tool_widget)->active)
    {
      gtk_signal_handler_block_by_data
	(GTK_OBJECT (tool_info[tool_type].tool_widget), (gpointer) tool_type);

      gtk_widget_activate (tool_info[tool_type].tool_widget);

      gtk_signal_handler_unblock_by_data
	(GTK_OBJECT (tool_info[tool_type].tool_widget), (gpointer) tool_type);
    }
}

void
context_manager_init (void)
{
  GimpContext *standard_context;
  GimpContext *default_context;
  GimpContext *user_context;
  gint i;

  /* Create the context of all existing images */
  image_context = gimp_container_new (GIMP_TYPE_IMAGE,
				      GIMP_CONTAINER_POLICY_WEAK);

  /*  Implicitly create the standard context  */
  standard_context = gimp_context_get_standard ();

  /*  TODO: load from disk  */
  default_context = gimp_context_new ("Default", NULL);
  gimp_context_set_default (default_context);

  /*  Initialize the user context will with the default context's values  */
  user_context = gimp_context_new ("User", default_context);
  gimp_context_set_user (user_context);

  /*  Update the tear-off menus  */
  gtk_signal_connect (GTK_OBJECT (user_context), "display_changed",
		      GTK_SIGNAL_FUNC (context_manager_display_changed),
		      NULL);

  /*  Update the tool system  */
  gtk_signal_connect (GTK_OBJECT (user_context), "tool_changed",
		      GTK_SIGNAL_FUNC (context_manager_tool_changed),
		      NULL);

  /*  Make the user contect the currently active context  */
  gimp_context_set_current (user_context);

  /*  Create a context to store the paint options of the
   *  global paint options mode
   */
  global_tool_context = gimp_context_new ("Global Tool Context", user_context);

  /*  TODO: add foreground, background, brush, pattern, gradient  */
  gimp_context_define_args (global_tool_context, PAINT_OPTIONS_MASK, FALSE);

  /*  Initialize the paint tools' private contexts  */
  for (i = 0; i < num_tools; i++)
    {
      switch (tool_info[i].tool_id)
	{
	case BUCKET_FILL:
	case BLEND:
	case PENCIL:
	case PAINTBRUSH:
	case ERASER:
	case AIRBRUSH:
	case CLONE:
	case CONVOLVE:
	case INK:
	case DODGEBURN:
	case SMUDGE:
/*  	case XINPUT_AIRBRUSH: */
	  tool_info[i].tool_context =
	    gimp_context_new (tool_info[i].private_tip, global_tool_context);
	  break;

	default:
	  tool_info[i].tool_context = NULL;
	  break;
	}
    }

  if (! global_paint_options &&
      active_tool &&
      tool_info[active_tool->type].tool_context)
    {
      gimp_context_set_parent (tool_info[active_tool->type].tool_context,
			       user_context);
    }
  else if (global_paint_options)
    {
      gimp_context_set_parent (global_tool_context, user_context);
    }
}

void
context_manager_free (void)
{
  gint i;

  gtk_object_unref (GTK_OBJECT (global_tool_context));
  global_tool_context = NULL;

  for (i = 0; i < num_tools; i++)
    {
      if (tool_info[i].tool_context != NULL)
	{
	  gtk_object_unref (GTK_OBJECT (tool_info[i].tool_context));
	  tool_info[i].tool_context = NULL;
	}
    }

  gtk_object_unref (GTK_OBJECT (gimp_context_get_user ()));
  gimp_context_set_user (NULL);
  gimp_context_set_current (NULL);

  /*  TODO: Save to disk before destroying  */
  gtk_object_unref (GTK_OBJECT (gimp_context_get_default ()));
  gimp_context_set_default (NULL);
}

void
context_manager_set_global_paint_options (gboolean global)
{
  GimpContext* context;

  if (global == global_paint_options) return;

  paint_options_set_global (global);

  if (global)
    {
      if (active_tool &&
	  (context = tool_info[active_tool->type].tool_context))
	{
	  gimp_context_unset_parent (context);
	}

      gimp_context_copy_args (global_tool_context, gimp_context_get_user (),
			      PAINT_OPTIONS_MASK);
      gimp_context_set_parent (global_tool_context, gimp_context_get_user ());
    }
  else
    {
      gimp_context_unset_parent (global_tool_context);

      if (active_tool &&
	  (context = tool_info[active_tool->type].tool_context))
	{
	  gimp_context_copy_args (context, gimp_context_get_user (),
				  GIMP_CONTEXT_PAINT_ARGS_MASK);
	  gimp_context_set_parent (context, gimp_context_get_user ());
	}
    }
}
