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

#ifndef __GIMP_DISPLAY_SHELL_SELECTION_H__
#define __GIMP_DISPLAY_SHELL_SELECTION_H__


struct _Selection
{
  /*  This information is for maintaining the selection's appearance  */
  GdkWindow        *win;              /*  Window to draw to                    */
  GimpDisplayShell *shell;            /*  GimpDisplay that owns the selection  */
  GdkGC            *gc_in;            /*  GC for drawing selection outline     */
  GdkGC            *gc_out;           /*  GC for selected regions outside
                                       *  current layer */
  GdkGC            *gc_layer;         /*  GC for current layer outline         */

  /*  This information is for drawing the marching ants around the border  */
  GdkSegment       *segs_in;          /*  gdk segments of area boundary     */
  GdkSegment       *segs_out;         /*  gdk segments of area boundary     */
  GdkSegment       *segs_layer;       /*  gdk segments of area boundary     */
  gint              num_segs_in;      /*  number of segments in segs1       */
  gint              num_segs_out;     /*  number of segments in segs2       */
  gint              num_segs_layer;   /*  number of segments in segs3       */
  gint              index_in;         /*  index of current stipple pattern  */
  gint              index_out;        /*  index of current stipple pattern  */
  gint              index_layer;      /*  index of current stipple pattern  */
  gint              state;            /*  internal drawing state            */
  gint              paused;           /*  count of pause requests           */
  gboolean          recalc;           /*  flag to recalculate the selection */
  gint              speed;            /*  speed of marching ants            */
  gboolean          hidden;           /*  is the selection hidden?          */
  gboolean          layer_hidden;     /*  is the layer boundary hidden?     */
  guint             timeout_id;       /*  timer for successive draws        */
  gint              cycle;            /*  color cycling turned on           */
  GdkPixmap        *cycle_pix;        /*  cycling pixmap                    */

  /* These are used only if USE_XDRAWPOINTS is defined. */
  GdkPoint         *points_in[8];     /*  points of segs_in for fast ants   */
  gint              num_points_in[8]; /*  number of points in points_in     */
  GdkGC            *gc_white;         /*  gc for drawing white points       */
  GdkGC            *gc_black;         /*  gc for drawing black points       */
};


Selection * gimp_display_shell_selection_create       (GdkWindow    *window,
                                                       GimpDisplayShell *gdisp,
                                                       gint          size,
                                                       gint          width,
                                                       gint          speed);
void        gimp_display_shell_selection_free         (Selection    *select);

void        gimp_display_shell_selection_pause        (Selection    *select);
void        gimp_display_shell_selection_resume       (Selection    *select);

void        gimp_display_shell_selection_start        (Selection    *select,
                                                       gboolean      recalc);
void        gimp_display_shell_selection_invis        (Selection    *select);
void        gimp_display_shell_selection_layer_invis  (Selection    *select);

void        gimp_display_shell_selection_toggle       (Selection     *select);
void        gimp_display_shell_selection_toggle_layer (Selection     *select);


#endif  /*  __GIMP_DISPLAY_SHELL_SELECTION_H__  */
