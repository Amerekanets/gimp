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

#ifndef __GIMP_DISPLAY_SHELL_H__
#define __GIMP_DISPLAY_SHELL_H__


#include <gtk/gtkwindow.h>


/*  FIXME: remove all gui/ stuff  */
#include "gui/gui-types.h"


/* finding the effective screen resolution (double) */
#define  SCREEN_XRES(s)   (s->dot_for_dot ? \
                           s->gdisp->gimage->xresolution : s->monitor_xres)
#define  SCREEN_YRES(s)   (s->dot_for_dot ? \
                           s->gdisp->gimage->yresolution : s->monitor_yres)

/* unpacking the user scale level (char) */
#define  SCALESRC(s)      (s->scale & 0x00ff)
#define  SCALEDEST(s)     (s->scale >> 8)

/* calculate scale factors (double) */
#define  SCALEFACTOR_X(s) ((SCALEDEST(s) * SCREEN_XRES(s)) / \
			   (SCALESRC(s) * s->gdisp->gimage->xresolution))
#define  SCALEFACTOR_Y(s) ((SCALEDEST(s) * SCREEN_YRES(s)) / \
			   (SCALESRC(s) * s->gdisp->gimage->yresolution))

/* scale values */
#define  SCALEX(s,x)      ((gint) (x * SCALEFACTOR_X(s)))
#define  SCALEY(s,y)      ((gint) (y * SCALEFACTOR_Y(s)))

/* unscale values */
#define  UNSCALEX(s,x)    ((gint) (x / SCALEFACTOR_X(s)))
#define  UNSCALEY(s,y)    ((gint) (y / SCALEFACTOR_Y(s)))
/* (and float-returning versions) */
#define  FUNSCALEX(s,x)   (x / SCALEFACTOR_X(s))
#define  FUNSCALEY(s,y)   (y / SCALEFACTOR_Y(s))


#define GIMP_TYPE_DISPLAY_SHELL            (gimp_display_shell_get_type ())
#define GIMP_DISPLAY_SHELL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_DISPLAY_SHELL, GimpDisplayShell))
#define GIMP_DISPLAY_SHELL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_DISPLAY_SHELL, GimpDisplayShellClass))
#define GIMP_IS_DISPLAY_SHELL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_DISPLAY_SHELL))
#define GIMP_IS_DISPLAY_SHELL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_DISPLAY_SHELL))
#define GIMP_DISPLAY_SHELL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_DISPLAY_SHELL, GimpDisplayShellClass))


typedef struct _GimpDisplayShellClass  GimpDisplayShellClass;

struct _GimpDisplayShell
{
  GtkWindow         parent_instance;

  GimpDisplay      *gdisp;

  GimpItemFactory  *menubar_factory;
  GimpItemFactory  *popup_factory;
  GimpItemFactory  *qmask_factory;

  gdouble           monitor_xres;
  gdouble           monitor_yres;

  gint              scale;             /*  scale factor from original raw image    */
  gint              other_scale;       /*  scale factor entered in Zoom->Other     */
  gboolean          dot_for_dot;       /*  is monitor resolution being ignored?    */

  gint              offset_x;          /*  offset of display image into raw image  */
  gint              offset_y;

  gint              disp_width;        /*  width of drawing area              */
  gint              disp_height;       /*  height of drawing area             */
  gint              disp_xoffset;
  gint              disp_yoffset;

  gboolean          proximity;         /* is a device in proximity            */
  gboolean          snap_to_guides;    /*  should the guides be snapped to?   */
  gboolean          snap_to_grid;      /*  should the grid be snapped to?     */

  Selection        *select;            /*  Selection object                   */

  GtkAdjustment    *hsbdata;           /*  adjustments                        */
  GtkAdjustment    *vsbdata;

  GtkWidget        *canvas;            /*  canvas widget                      */

  GtkWidget        *hsb;               /*  scroll bars                        */
  GtkWidget        *vsb;
  GtkWidget        *qmask;             /*  qmask button                       */
  GtkWidget        *hrule;             /*  rulers                             */
  GtkWidget        *vrule;
  GtkWidget        *origin;            /*  origin button                      */

  GtkWidget        *statusbar;         /*  statusbar                          */

  guchar           *render_buf;        /*  buffer for rendering the image     */
  GdkGC            *render_gc;         /*  GC for rendering the image         */

  guint             title_idle_id;     /*  title update idle ID               */

  gint              icon_size;         /*  size of the icon pixmap            */
  guint             icon_idle_id;      /*  ID of the idle-function            */

  GdkCursorType       current_cursor;  /*  Currently installed main cursor    */
  GimpToolCursorType  tool_cursor;     /*  Current Tool cursor                */
  GimpCursorModifier  cursor_modifier; /*  Cursor modifier (plus, minus, ...) */

  GdkCursorType     override_cursor;   /*  Overriding cursor                  */
  gboolean          using_override_cursor; /*  is the cursor overridden?      */

  gboolean          draw_cursor;       /* should we draw software cursor ?    */
  gboolean          have_cursor;       /* is cursor currently drawn ?         */
  gint              cursor_x;          /* software cursor X value             */
  gint              cursor_y;          /* software cursor Y value             */

  GtkWidget        *padding_button;    /* GimpColorPanel in the NE corner     */
  GtkWidget        *nav_ebox;          /* GtkEventBox on the SE corner        */

  GtkWidget        *warning_dialog;    /*  close warning dialog               */
  InfoDialog       *info_dialog;       /*  image information dialog           */
  GtkWidget        *scale_dialog;      /*  scale (zoom) dialog                */
  GtkWidget        *nav_popup;         /*  navigation popup                   */
  GtkWidget        *grid_dialog;       /*  grid configuration dialog          */

  GList            *filters;           /* color display conversion stuff      */
  GtkWidget        *filters_dialog;    /* color display filter dialog         */

  GdkWindowState    window_state;      /* for fullscreen display              */

  gint              paused_count;

  GQuark            vectors_freeze_handler;
  GQuark            vectors_thaw_handler;
  GQuark            vectors_visible_handler;

  GimpDisplayOptions *options;
  GimpDisplayOptions *fullscreen_options;

  /*  the state of gimp_display_shell_tool_events()  */
  gboolean          space_pressed;
  gboolean          space_release_pending;
  gboolean          scrolling;
  gint              scroll_start_x;
  gint              scroll_start_y;
  gboolean          button_press_before_focus;
};

struct _GimpDisplayShellClass
{
  GtkWindowClass  parent_class;

  void (* scaled)    (GimpDisplayShell *shell);
  void (* scrolled)  (GimpDisplayShell *shell);
  void (* reconnect) (GimpDisplayShell *shell);
};


GType       gimp_display_shell_get_type              (void) G_GNUC_CONST;

GtkWidget * gimp_display_shell_new                   (GimpDisplay      *gdisp,
                                                      guint             scale,
                                                      GimpMenuFactory  *menu_factory,
                                                      GimpItemFactory  *popup_factory);

void        gimp_display_shell_close                 (GimpDisplayShell *shell,
                                                      gboolean          kill_it);

void        gimp_display_shell_reconnect             (GimpDisplayShell *shell);

void        gimp_display_shell_scaled                (GimpDisplayShell *shell);
void        gimp_display_shell_scrolled              (GimpDisplayShell *shell);

void        gimp_display_shell_snap_coords           (GimpDisplayShell *shell,
                                                      GimpCoords       *coords,
                                                      GimpCoords       *snapped_coords,
                                                      gint              snap_offset_x,
                                                      gint              snap_offset_y,
                                                      gint              snap_width,
                                                      gint              snap_height);

gboolean    gimp_display_shell_mask_bounds           (GimpDisplayShell *shell,
                                                      gint             *x1,
                                                      gint             *y1,
                                                      gint             *x2,
                                                      gint             *y2);

void        gimp_display_shell_expose_area           (GimpDisplayShell *shell,
                                                      gint              x,
                                                      gint              y,
                                                      gint              w,
                                                      gint              h);
void        gimp_display_shell_expose_guide          (GimpDisplayShell *shell,
                                                      GimpGuide        *guide);
void        gimp_display_shell_expose_full           (GimpDisplayShell *shell);

void        gimp_display_shell_flush                 (GimpDisplayShell *shell,
                                                      gboolean          now);

void        gimp_display_shell_pause                 (GimpDisplayShell *shell);
void        gimp_display_shell_resume                (GimpDisplayShell *shell);

void        gimp_display_shell_draw_area             (GimpDisplayShell *shell,
                                                      gint              x,
                                                      gint              y,
                                                      gint              w,
                                                      gint              h);
void        gimp_display_shell_draw_cursor           (GimpDisplayShell *shell);
void        gimp_display_shell_draw_guide            (GimpDisplayShell *shell,
                                                      GimpGuide        *guide,
                                                      gboolean          active);
void        gimp_display_shell_draw_guides           (GimpDisplayShell *shell);

void        gimp_display_shell_draw_grid             (GimpDisplayShell *shell);

void        gimp_display_shell_draw_vector           (GimpDisplayShell *shell,
                                                      GimpVectors      *vectors);
void        gimp_display_shell_draw_vectors          (GimpDisplayShell *shell);

void        gimp_display_shell_update_icon           (GimpDisplayShell *shell);

void        gimp_display_shell_shrink_wrap           (GimpDisplayShell *shell);

void        gimp_display_shell_selection_visibility  (GimpDisplayShell *shell,
                                                      GimpSelectionControl  control);


#endif /* __GIMP_DISPLAY_SHELL_H__ */
