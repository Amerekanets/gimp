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

#ifndef  __GIMP_BUCKET_FILL_TOOL_H__
#define  __GIMP_BUCKET_FILL_TOOL_H__


#include "gimptool.h"


typedef enum
{
  FG_BUCKET_FILL,
  BG_BUCKET_FILL,
  PATTERN_BUCKET_FILL
} BucketFillMode;


#define GIMP_TYPE_BUCKET_FILL_TOOL            (gimp_bucket_fill_tool_get_type ())
#define GIMP_BUCKET_FILL_TOOL(obj)            (GTK_CHECK_CAST ((obj), GIMP_TYPE_BUCKET_FILL_TOOL, GimpBucketFillTool))
#define GIMP_IS_BUCKET_FILL_TOOL(obj)         (GTK_CHECK_TYPE ((obj), GIMP_TYPE_BUCKET_FILL_TOOL))
#define GIMP_BUCKET_FILL_TOOL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GIMP_TYPE_BUCKET_FILL_TOOL, GimpBucketFillToolClass))
#define GIMP_IS_BUCKET_FILL_TOOL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_BUCKET_FILL_TOOL))


typedef struct _GimpBucketFillTool      GimpBucketFillTool;
typedef struct _GimpBucketFillToolClass GimpBucketFillToolClass;

struct _GimpBucketFillTool
{
  GimpTool  parent_instance;

  gint      target_x;  /*  starting x coord  */
  gint      target_y;  /*  starting y coord  */
};

struct _GimpBucketFillToolClass
{
  GimpToolClass  parent_class;
};


void       gimp_bucket_fill_tool_register (void);

GtkType    gimp_bucket_fill_tool_get_type (void);


void       bucket_fill                    (GimpImage      *gimage,
                                           GimpDrawable   *drawable,
                                           BucketFillMode  fill_mode,
                                           gint            paint_mode,
                                           gdouble         opacity,
                                           gdouble         threshold,
                                           gboolean        sample_merged,
                                           gdouble         x,
                                           gdouble         y);

void       bucket_fill_region             (BucketFillMode  fill_mode,
                                           PixelRegion    *bufPR,
                                           PixelRegion    *maskPR,
                                           guchar         *col,
                                           TempBuf        *pattern,
                                           gint            off_x,
                                           gint            off_y,
                                           gboolean        has_alpha);


#endif  /*  __GIMP_BUCKET_FILL_TOOL_H__  */
