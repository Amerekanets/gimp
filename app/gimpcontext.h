/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpcontext.h: Copyright (C) 1999 Michael Natterer <mitch@gimp.org>
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

#ifndef __GIMP_CONTEXT_H__
#define __GIMP_CONTEXT_H__


#include "gimpobject.h"


#define GIMP_TYPE_CONTEXT            (gimp_context_get_type ())
#define GIMP_CONTEXT(obj)            (GTK_CHECK_CAST ((obj), GIMP_TYPE_CONTEXT, GimpContext))
#define GIMP_CONTEXT_CLASS(klass)    (GTK_CHECK_CLASS_CAST (klass, GIMP_TYPE_CONTEXT, GimpContextClass))
#define GIMP_IS_CONTEXT(obj)         (GTK_CHECK_TYPE ((obj), GIMP_TYPE_CONTEXT))
#define GIMP_IS_CONTEXT_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_CONTEXT))

typedef enum
{
  GIMP_CONTEXT_ARG_IMAGE,
  GIMP_CONTEXT_ARG_DISPLAY,
  GIMP_CONTEXT_ARG_TOOL,
  GIMP_CONTEXT_ARG_FOREGROUND,
  GIMP_CONTEXT_ARG_BACKGROUND,
  GIMP_CONTEXT_ARG_OPACITY,
  GIMP_CONTEXT_ARG_PAINT_MODE,
  GIMP_CONTEXT_ARG_BRUSH,
  GIMP_CONTEXT_ARG_PATTERN,
  GIMP_CONTEXT_ARG_GRADIENT,
  GIMP_CONTEXT_ARG_PALETTE,
  GIMP_CONTEXT_NUM_ARGS
} GimpContextArgType;

typedef enum
{
  GIMP_CONTEXT_IMAGE_MASK      = 1 <<  0,
  GIMP_CONTEXT_DISPLAY_MASK    = 1 <<  1,
  GIMP_CONTEXT_TOOL_MASK       = 1 <<  2,
  GIMP_CONTEXT_FOREGROUND_MASK = 1 <<  3,
  GIMP_CONTEXT_BACKGROUND_MASK = 1 <<  4,
  GIMP_CONTEXT_OPACITY_MASK    = 1 <<  5,
  GIMP_CONTEXT_PAINT_MODE_MASK = 1 <<  6,
  GIMP_CONTEXT_BRUSH_MASK      = 1 <<  7,
  GIMP_CONTEXT_PATTERN_MASK    = 1 <<  8,
  GIMP_CONTEXT_GRADIENT_MASK   = 1 <<  9,
  GIMP_CONTEXT_PALETTE_MASK    = 1 << 10,

  /*  aliases  */
  GIMP_CONTEXT_PAINT_ARGS_MASK = (GIMP_CONTEXT_FOREGROUND_MASK |
				  GIMP_CONTEXT_BACKGROUND_MASK |
				  GIMP_CONTEXT_OPACITY_MASK    |
				  GIMP_CONTEXT_PAINT_MODE_MASK |
				  GIMP_CONTEXT_BRUSH_MASK      |
				  GIMP_CONTEXT_PATTERN_MASK    |
				  GIMP_CONTEXT_GRADIENT_MASK),
  GIMP_CONTEXT_ALL_ARGS_MASK   = (GIMP_CONTEXT_IMAGE_MASK      |
				  GIMP_CONTEXT_DISPLAY_MASK    |
				  GIMP_CONTEXT_TOOL_MASK       |
				  GIMP_CONTEXT_PALETTE_MASK    |
				  GIMP_CONTEXT_PAINT_ARGS_MASK)
} GimpContextArgMask;

typedef struct _GimpContextClass GimpContextClass;

struct _GimpContext
{
  GimpObject	    parent_instance;

  GimpContext	   *parent;

  guint32           defined_args;

  GimpImage	   *image;
  GDisplay	   *display;

  ToolType          tool;

  GimpRGB           foreground;
  GimpRGB           background;

  gdouble	    opacity;
  LayerModeEffects  paint_mode;

  GimpBrush        *brush;
  gchar            *brush_name;

  GimpPattern      *pattern;
  gchar            *pattern_name;

  GimpGradient     *gradient;
  gchar            *gradient_name;

  GimpPalette      *palette;
  gchar            *palette_name;
};

struct _GimpContextClass
{
  GimpObjectClass  parent_class;

  void (* image_changed)      (GimpContext      *context,
			       GimpImage        *image);
  void (* display_changed)    (GimpContext      *context,
			       GDisplay         *display);

  void (* tool_changed)       (GimpContext      *context,
			       ToolType          tool);

  void (* foreground_changed) (GimpContext      *context,
			       GimpRGB          *color);
  void (* background_changed) (GimpContext      *context,
			       GimpRGB          *color);
  void (* opacity_changed)    (GimpContext      *context,
			       gdouble           opacity);
  void (* paint_mode_changed) (GimpContext      *context,
			       LayerModeEffects  paint_mode);
  void (* brush_changed)      (GimpContext      *context,
			       GimpBrush        *brush);
  void (* pattern_changed)    (GimpContext      *context,
			       GimpPattern      *pattern);
  void (* gradient_changed)   (GimpContext      *context,
			       GimpGradient     *gradient);
  void (* palette_changed)    (GimpContext      *context,
			       GimpPalette      *palette);
};

GtkType       gimp_context_get_type          (void);
GimpContext * gimp_context_new               (const gchar *name,
					      GimpContext *template);

/*  TODO: - gimp_context_find ()
 *
 *        probably interacting with the context manager:
 *        - gimp_context_push () which will call gimp_context_set_parent()
 *        - gimp_context_push_new () to do a GL-style push
 *        - gimp_context_pop ()
 *
 *        a proper mechanism to prevent silly operations like pushing
 *        the user context to some client stack etc.
 */


/*  to be used by the context management system only
 */
void          gimp_context_set_current       (GimpContext *context);
void          gimp_context_set_user          (GimpContext *context);
void          gimp_context_set_default       (GimpContext *context);

/*  these are always available
 */
GimpContext * gimp_context_get_current       (void);
GimpContext * gimp_context_get_user          (void);
GimpContext * gimp_context_get_default       (void);
GimpContext * gimp_context_get_standard      (void);

/*  functions for manipulating a single context
 */
const gchar * gimp_context_get_name          (const GimpContext *context);
void          gimp_context_set_name          (GimpContext       *context,
					      const gchar       *name);

GimpContext * gimp_context_get_parent        (const GimpContext *context);
void          gimp_context_set_parent        (GimpContext       *context,
					      GimpContext       *parent);
void          gimp_context_unset_parent      (GimpContext       *context);

/*  define / undefinine context arguments
 *
 *  the value of an undefined argument will be taken from the parent context.
 */
void          gimp_context_define_arg        (GimpContext        *context,
					      GimpContextArgType  arg,
					      gboolean            defined);

gboolean      gimp_context_arg_defined       (GimpContext        *context,
					      GimpContextArgType  arg);

void          gimp_context_define_args       (GimpContext        *context,
					      GimpContextArgMask  args_mask,
					      gboolean            defined);

/*  copying context arguments
 */
void          gimp_context_copy_arg          (GimpContext        *src,
					      GimpContext        *dest,
					      GimpContextArgType  arg);

void          gimp_context_copy_args         (GimpContext        *src,
					      GimpContext        *dest,
					      GimpContextArgMask  args_mask);


/*  manipulate by GtkType  */
GimpContextArgType   gimp_context_type_to_arg         (GtkType type);
const gchar        * gimp_context_type_to_signal_name (GtkType type);

GimpObject       * gimp_context_get_by_type        (GimpContext     *context,
						    GtkType          type);
void               gimp_context_set_by_type        (GimpContext     *context,
						    GtkType          type,
						    GimpObject      *object);
void               gimp_context_changed_by_type    (GimpContext     *context,
						    GtkType          type);


/*  image  */
GimpImage        * gimp_context_get_image          (GimpContext     *context);
void               gimp_context_set_image          (GimpContext     *context,
						    GimpImage       *image);
void               gimp_context_image_changed      (GimpContext     *context);


/*  display  */
GDisplay         * gimp_context_get_display        (GimpContext     *context);
void               gimp_context_set_display        (GimpContext     *context,
						    GDisplay        *display);
void               gimp_context_display_changed    (GimpContext     *context);


/*  tool  */
ToolType           gimp_context_get_tool           (GimpContext     *context);
void               gimp_context_set_tool           (GimpContext     *context,
						    ToolType         tool_type);
void               gimp_context_tool_changed       (GimpContext     *context);


/*  foreground color  */
void               gimp_context_get_foreground     (GimpContext     *context,
						    GimpRGB         *color);
void               gimp_context_set_foreground     (GimpContext     *context,
						    const GimpRGB   *color);
void               gimp_context_foreground_changed (GimpContext     *context);


/*  background color  */
void               gimp_context_get_background     (GimpContext     *context,
						    GimpRGB         *color);
void               gimp_context_set_background     (GimpContext     *context,
						    const GimpRGB   *color);
void               gimp_context_background_changed (GimpContext     *context);


/*  color utility functions  */
void               gimp_context_set_default_colors (GimpContext     *context);
void               gimp_context_swap_colors        (GimpContext     *context);


/*  opacity  */
gdouble            gimp_context_get_opacity        (GimpContext     *context);
void               gimp_context_set_opacity        (GimpContext     *context,
						    gdouble          opacity);
void               gimp_context_opacity_changed    (GimpContext     *context);


/*  paint mode  */
LayerModeEffects   gimp_context_get_paint_mode     (GimpContext     *context);
void               gimp_context_set_paint_mode     (GimpContext     *context,
						    LayerModeEffects paint_mode);
void               gimp_context_paint_mode_changed (GimpContext     *context);


/*  brush  */
GimpBrush        * gimp_context_get_brush          (GimpContext     *context);
void               gimp_context_set_brush          (GimpContext     *context,
						    GimpBrush       *brush);
void               gimp_context_brush_changed      (GimpContext     *context);


/*  pattern  */
GimpPattern      * gimp_context_get_pattern        (GimpContext     *context);
void               gimp_context_set_pattern        (GimpContext     *context,
						    GimpPattern     *pattern);
void               gimp_context_pattern_changed    (GimpContext     *context);


/*  gradient  */
GimpGradient     * gimp_context_get_gradient       (GimpContext     *context);
void               gimp_context_set_gradient       (GimpContext     *context,
						    GimpGradient    *gradient);
void               gimp_context_gradient_changed   (GimpContext     *context);


/*  palette  */
GimpPalette      * gimp_context_get_palette        (GimpContext     *context);
void               gimp_context_set_palette        (GimpContext     *context,
						    GimpPalette     *palette);
void               gimp_context_palette_changed    (GimpContext     *context);


#endif /* __GIMP_CONTEXT_H__ */
