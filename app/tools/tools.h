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

#ifndef __TOOLS_H__
#define __TOOLS_H__


#include "cursorutil.h"


/*  The possibilities for where the cursor lies  */
#define  ACTIVE_LAYER      (1 << 0)
#define  SELECTION         (1 << 1)
#define  NON_ACTIVE_LAYER  (1 << 2)

/*  Tool action function declarations  */
typedef void   (* ButtonPressFunc)    (Tool           *tool,
				       GdkEventButton *bevent,
				       GDisplay       *gdisp);
typedef void   (* ButtonReleaseFunc)  (Tool           *tool,
				       GdkEventButton *bevent,
				       GDisplay       *gdisp);
typedef void   (* MotionFunc)         (Tool           *tool,
				       GdkEventMotion *mevent,
				       GDisplay       *gdisp);
typedef void   (* ArrowKeysFunc)      (Tool           *tool,
				       GdkEventKey    *kevent,
				       GDisplay       *gdisp);
typedef void   (* ModifierKeyFunc)    (Tool           *tool,
				       GdkEventKey    *kevent,
				       GDisplay       *gdisp);
typedef void   (* CursorUpdateFunc)   (Tool           *tool,
				       GdkEventMotion *mevent,
				       GDisplay       *gdisp);
typedef void   (* OperUpdateFunc)     (Tool           *tool,
				       GdkEventMotion *mevent,
				       GDisplay       *gdisp);
typedef void   (* ToolCtlFunc)        (Tool           *tool,
				       ToolAction      action,
				       GDisplay       *gdisp);

/*  ToolInfo function declarations  */
typedef Tool * (* ToolInfoNewFunc)    (void);
typedef void   (* ToolInfoFreeFunc)   (Tool           *tool);
typedef void   (* ToolInfoInitFunc)   (GDisplay       *gdisp);

/*  The types of tools...  */
struct _Tool
{
  /*  Data  */
  ToolType      type;         /*  Tool type                                   */
  gint          ID;           /*  unique tool ID                              */

  ToolState     state;        /*  state of tool activity                      */
  gint          paused_count; /*  paused control count                        */
  gboolean      scroll_lock;  /*  allow scrolling or not                      */
  gboolean      auto_snap_to; /*  snap to guides automatically                */

  gboolean      preserve;     /*  Preserve this tool across drawable changes  */
  GDisplay     *gdisp;        /*  pointer to currently active gdisp           */
  GimpDrawable *drawable;     /*  pointer to the tool's current drawable      */

  gboolean      toggled;      /*  Bad hack to let the paint_core show the
			       *  right toggle cursors
			       */


  gpointer      private;      /*  Tool-specific information                 */

  /*  Action functions  */
  ButtonPressFunc    button_press_func;
  ButtonReleaseFunc  button_release_func;
  MotionFunc         motion_func;
  ArrowKeysFunc      arrow_keys_func;
  ModifierKeyFunc    modifier_key_func;
  CursorUpdateFunc   cursor_update_func;
  OperUpdateFunc     oper_update_func;
  ToolCtlFunc        control_func;
};

struct _ToolInfo
{
  ToolOptions *tool_options;

  gchar       *tool_name;

  gchar       *menu_path;  
  gchar       *menu_accel; 

  gchar      **icon_data;
  GdkPixmap   *icon_pixmap;
  GdkBitmap   *icon_mask;

  gchar       *tool_desc;
  const gchar *private_tip;

  ToolType     tool_id;

  ToolInfoNewFunc  new_func;
  ToolInfoFreeFunc free_func;
  ToolInfoInitFunc init_func;

  GtkWidget   *tool_widget;

  GimpContext *tool_context;

  BitmapCursor tool_cursor;
  BitmapCursor toggle_cursor;
};


/*  Function declarations  */
Tool      * tools_new_tool             (ToolType     tool_type);

void        tools_select               (ToolType     tool_type);
void        tools_initialize           (ToolType     tool_type,
					GDisplay    *gdisplay);

void        active_tool_control        (ToolAction   action,
					GDisplay    *gdisp);

void        tools_help_func            (const gchar *help_data);

void        tools_register             (ToolType     tool_type,
					ToolOptions *tool_options);

gchar     * tool_active_PDB_string    (void);

/*  don't unref these pixmaps, they are static!  */
GdkPixmap * tool_get_pixmap           (ToolType      tool_type);
GdkBitmap * tool_get_mask             (ToolType      tool_type);


/*  Global Data Structures  */
extern Tool     *active_tool;
extern ToolInfo  tool_info[];
extern gint      num_tools;


#endif  /*  __TOOLS_H__  */
