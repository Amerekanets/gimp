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

#ifndef __GDISPLAY_H__
#define __GDISPLAY_H__

typedef enum
{
  SelectionOff,
  SelectionLayerOff,
  SelectionOn,
  SelectionPause,
  SelectionResume
} SelectionControl;

/*
 *  Global variables
 *
 */

/*  some useful macros  */

/* unpacking the user scale level (char) */
#define  SCALESRC(g)    (g->scale & 0x00ff)
#define  SCALEDEST(g)   (g->scale >> 8)

/* finding the effective screen resolution (double) */
#define  SCREEN_XRES(g) (g->dot_for_dot? g->gimage->xresolution : monitor_xres)
#define  SCREEN_YRES(g) (g->dot_for_dot? g->gimage->yresolution : monitor_yres)

/* calculate scale factors (double) */
#define  SCALEFACTOR_X(g)  ((SCALEDEST(g) * SCREEN_XRES(g)) /          \
			    (SCALESRC(g) * g->gimage->xresolution))
#define  SCALEFACTOR_Y(g)  ((SCALEDEST(g) * SCREEN_YRES(g)) /          \
			    (SCALESRC(g) * g->gimage->yresolution))

/* scale values */
#define  SCALEX(g,x)    ((int)(x * SCALEFACTOR_X(g)))
#define  SCALEY(g,y)    ((int)(y * SCALEFACTOR_Y(g)))

/* unscale values */
#define  UNSCALEX(g,x)  ((int)(x / SCALEFACTOR_X(g)))
#define  UNSCALEY(g,y)  ((int)(y / SCALEFACTOR_Y(g)))
/* (and float-returning versions) */
#define  FUNSCALEX(g,x)  ((x / SCALEFACTOR_X(g)))
#define  FUNSCALEY(g,y)  ((y / SCALEFACTOR_Y(g)))




#define LOWPASS(x) ((x>0) ? x : 0)
/* #define HIGHPASS(x,y) ((x>y) ? y : x) */ /* unused - == MIN */


/* maximal width of the string holding the cursor-coordinates for
   the status line */
#define CURSOR_STR_LENGTH 256

/* maximal length of the format string for the cursor-coordinates */
#define CURSOR_FORMAT_LENGTH 32

typedef struct _IdleRenderStruct
{
  gint width;
  gint height;
  gint x;
  gint y;
  gint basex;
  gint basey;
  guint idleid;
  /*guint handlerid;*/
  gboolean active;
  GSList *update_areas;           /*  flushed update areas */

} IdleRenderStruct;


struct _GDisplay
{
  gint ID;                        /*  unique identifier for this gdisplay     */

  GtkWidget *shell;               /*  shell widget for this gdisplay          */
  GtkWidget *canvas;              /*  canvas widget for this gdisplay         */
  GtkWidget *hsb, *vsb;           /*  widgets for scroll bars                 */
  GtkWidget *qmaskoff, *qmaskon;  /*  widgets for qmask buttons               */
  GtkWidget *hrule, *vrule;       /*  widgets for rulers                      */
  GtkWidget *origin;              /*  widgets for rulers                      */
  GtkWidget *popup;               /*  widget for popup menu                   */
  GtkWidget *statusarea;          /*  hbox holding the statusbar and stuff    */
  GtkWidget *statusbar;           /*  widget for statusbar                    */
  GtkWidget *progressbar;         /*  widget for progressbar                  */
  GtkWidget *cursor_label;        /*  widget for cursor position              */
  gchar cursor_format_str [CURSOR_FORMAT_LENGTH]; /* we need a variable format
						   * string because different
						   * units have different number
						   * of decimals              */
  GtkWidget *cancelbutton;        /*  widget for cancel button                */
  guint progressid;               /*  id of statusbar message for progress    */

  InfoDialog *window_info_dialog; /*  dialog box for image information        */
  InfoDialog *window_nav_dialog;  /*  dialog box for image navigation         */
  GtkWidget  *nav_popup;          /*  widget for the popup navigation window  */

  gint color_type;                /*  is this an RGB or GRAY colormap         */

  GtkAdjustment *hsbdata;         /*  horizontal data information             */
  GtkAdjustment *vsbdata;         /*  vertical data information               */

  GdkPixmap *icon;		  /*  Pixmap for the icon                     */
  GdkBitmap *iconmask;		  /*  Bitmap for the icon mask                */
  guint      iconsize;            /*  The size of the icon pixmap             */
  guint      icon_needs_update;   /*  Do we need to render a new icon?        */
  gint       icon_idle_id;        /*  The ID of the idle-function             */

  GimpImage *gimage;	          /*  pointer to the associated gimage struct */
  gint instance;                  /*  the instance # of this gdisplay as      */
                                  /*  taken from the gimage at creation       */

  gint depth;                     /*  depth of our drawables                  */
  gint disp_width;                /*  width of drawing area in the window     */
  gint disp_height;               /*  height of drawing area in the window    */
  gint disp_xoffset;
  gint disp_yoffset;

  gint offset_x;                  /*  offset of display image into raw image  */
  gint offset_y;        
  gint scale;        	          /*  scale factor from original raw image    */
  gboolean dot_for_dot;		  /*  is monitor resolution being ignored?    */
  gboolean draw_guides;           /*  should the guides be drawn?             */
  gboolean snap_to_guides;        /*  should the guides be snapped to?        */

  Selection *select;              /*  Selection object                        */

  GdkGC *scroll_gc;               /*  GC for scrolling                        */

  GSList *update_areas;           /*  Update areas list                       */
  GSList *display_areas;          /*  Display areas list                      */

  GdkCursorType  current_cursor;  /*  Currently installed cursor              */
  ToolType       cursor_tool;     /*  Cursor for which tool?                  */
  CursorModifier cursor_modifier; /*  Cursor modifier (plus, minus, ...)      */
  gboolean       toggle_cursor;   /*  Cursor toggled?                         */

  GdkCursorType override_cursor;  /*  Overriding cursor (ie. hourglass)       */
  gboolean using_override_cursor; /* is the cursor overridden? (ie. hourglass)*/

  gboolean draw_cursor;	          /* should we draw software cursor ?         */
  gint cursor_x;                  /* software cursor X value                  */
  gint cursor_y;                  /* software cursor Y value                  */
  gboolean proximity;             /* is a device in proximity of gdisplay ?   */
  gboolean have_cursor;		  /* is cursor currently drawn ?              */
  
  IdleRenderStruct idle_render;   /* state of this gdisplay's render thread   */
  
#ifdef DISPLAY_FILTERS
  GList     *cd_list;             /* color display conversion stuff           */
  GtkWidget *cd_ui;		  /* color display filter dialog              */
#endif /* DISPLAY_FILTERS */

  GtkWidget *warning_dialog;      /* "Changes were made to %s. Close anyway?" */
};



/* member function declarations */

GDisplay * gdisplay_new                    (GimpImage *, guint);
void       gdisplay_reconnect              (GDisplay *, GimpImage *);
void       gdisplay_remove_and_delete      (GDisplay *);
gint       gdisplay_mask_value             (GDisplay *, gint, gint);
gint       gdisplay_mask_bounds            (GDisplay *, gint *, gint *, gint *, gint *);
void       gdisplay_transform_coords       (GDisplay *, gint, gint, gint *, gint *, gint);
void       gdisplay_untransform_coords     (GDisplay *, gint, gint, gint *, gint *, 
					                gboolean, gboolean);
void       gdisplay_transform_coords_f     (GDisplay *, gdouble, gdouble, 
					                gdouble *, gdouble *, gboolean);
void       gdisplay_untransform_coords_f   (GDisplay *, gdouble, gdouble, 
					                gdouble *, gdouble *, gboolean);

void       gdisplay_real_install_tool_cursor (GDisplay       *gdisp,
					      GdkCursorType   cursor_type,
					      ToolType        tool_type,
					      CursorModifier  modifier,
					      gboolean        toggle_cursor,
					      gboolean        always_install);
void       gdisplay_install_tool_cursor      (GDisplay       *gdisp,
					      GdkCursorType   cursor_type,
					      ToolType        tool_type,
					      CursorModifier  modifier,
					      gboolean        toggle_cursor);
void       gdisplay_remove_tool_cursor       (GDisplay       *gdisp);
void       gdisplay_install_override_cursor  (GDisplay       *gdisp,
					      GdkCursorType   cursor_type);
void       gdisplay_remove_override_cursor   (GDisplay       *gdisp);

void       gdisplay_set_menu_sensitivity   (GDisplay *);
void       gdisplay_expose_area            (GDisplay *, gint, gint, gint, gint);
void       gdisplay_expose_guide           (GDisplay *, Guide *);
void       gdisplay_expose_full            (GDisplay *);
void       gdisplay_flush                  (GDisplay *);
void       gdisplay_flush_now              (GDisplay *);
void       gdisplay_update_icon            (GDisplay *);
gint       gdisplay_update_icon_invoker    (gpointer);
void       gdisplay_draw_guides            (GDisplay *);
void       gdisplay_draw_guide             (GDisplay *, Guide *, gboolean);
Guide*     gdisplay_find_guide             (GDisplay *, gdouble, double);
gboolean   gdisplay_snap_point             (GDisplay *, gdouble, gdouble, 
					                gdouble *, gdouble *);
void       gdisplay_snap_rectangle         (GDisplay *, gdouble, gdouble, gdouble, gdouble,
					                gdouble *, gdouble *);
void	   gdisplay_update_cursor	   (GDisplay *, gint, gint);
void	   gdisplay_set_dot_for_dot	   (GDisplay *, gboolean);
void       gdisplay_resize_cursor_label    (GDisplay *);
void       gdisplay_update_title           (GDisplay *);
void       gdisplay_flush_displays_only    (GDisplay *gdisp); /* no rerender! */


GDisplay * gdisplay_active                 (void);
GDisplay * gdisplay_get_by_ID              (gint  ID);


/*  function declarations  */

GDisplay * gdisplays_check_valid           (GDisplay *, GimpImage *);
void       gdisplays_reconnect             (GimpImage *old, GimpImage *new);
void       gdisplays_update_title          (GimpImage *);
void       gdisplays_resize_cursor_label   (GimpImage *);
void       gdisplays_setup_scale           (GimpImage *);
void       gdisplays_update_area           (GimpImage *, gint, gint, gint, gint);
void       gdisplays_expose_guides         (GimpImage *);
void       gdisplays_expose_guide          (GimpImage *, Guide *);
void       gdisplays_update_full           (GimpImage *);
void       gdisplays_shrink_wrap           (GimpImage *);
void       gdisplays_expose_full           (void);
void       gdisplays_selection_visibility  (GimpImage *, SelectionControl);
gboolean   gdisplays_dirty                 (void);
void       gdisplays_delete                (void);
void       gdisplays_flush                 (void);
void       gdisplays_flush_now             (void);
void       gdisplays_finish_draw           (void);
void       gdisplays_nav_preview_resized   (void);
void       gdisplays_foreach               (GFunc func, gpointer user_data);

#endif /*  __GDISPLAY_H__  */
