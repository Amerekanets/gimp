/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimp_brush_generated module Copyright 1998 Jay Cox <jaycox@earthlink.net>
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

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gtk/gtk.h>

#include "libgimpmath/gimpmath.h"

#include "apptypes.h"

#include "appenv.h"
#include "gimpbrushgenerated.h"
#include "gimprc.h"
#include "gimpbrush.h"
#include "temp_buf.h"

/*  this needs to go away  */
#include "tools/paint_core.h"


#define OVERSAMPLING 5


/*  local function prototypes  */
static void     gimp_brush_generated_class_init  (GimpBrushGeneratedClass *klass);
static void       gimp_brush_generated_init      (GimpBrushGenerated *brush);
static void       gimp_brush_generated_destroy   (GtkObject          *object);
static gboolean   gimp_brush_generated_save      (GimpData           *data);
static void       gimp_brush_generated_dirty     (GimpData           *data);
static gchar    * gimp_brush_generated_get_extension (GimpData       *data);
static GimpData * gimp_brush_generated_duplicate (GimpData           *data);


static GimpBrushClass *parent_class = NULL;


GtkType
gimp_brush_generated_get_type (void)
{
  static GtkType type = 0;

  if (!type)
    {
      GtkTypeInfo info =
      {
	"GimpBrushGenerated",
	sizeof (GimpBrushGenerated),
	sizeof (GimpBrushGeneratedClass),
	(GtkClassInitFunc) gimp_brush_generated_class_init,
	(GtkObjectInitFunc) gimp_brush_generated_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL
      };

      type = gtk_type_unique (GIMP_TYPE_BRUSH, &info);
    }

  return type;
}

static void
gimp_brush_generated_class_init (GimpBrushGeneratedClass *klass)
{
  GtkObjectClass *object_class;
  GimpDataClass  *data_class;

  object_class = (GtkObjectClass *) klass;
  data_class   = (GimpDataClass *) klass;

  parent_class = gtk_type_class (GIMP_TYPE_BRUSH);

  object_class->destroy = gimp_brush_generated_destroy;

  data_class->save          = gimp_brush_generated_save;
  data_class->dirty         = gimp_brush_generated_dirty;
  data_class->get_extension = gimp_brush_generated_get_extension;
  data_class->duplicate     = gimp_brush_generated_duplicate;
}

static void
gimp_brush_generated_init (GimpBrushGenerated *brush)
{
  brush->radius       = 5.0;
  brush->hardness     = 0.0;
  brush->angle        = 0.0;
  brush->aspect_ratio = 1.0;
  brush->freeze       = 0;
}

static void
gimp_brush_generated_destroy (GtkObject *object)
{
  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static gboolean
gimp_brush_generated_save (GimpData *data)
{
  GimpBrushGenerated *brush;
  FILE               *fp;

  g_return_val_if_fail (data != NULL, FALSE);
  g_return_val_if_fail (GIMP_IS_BRUSH_GENERATED (data), FALSE);

  brush = GIMP_BRUSH_GENERATED (data);

  /*  we are (finaly) ready to try to save the generated brush file  */
  if ((fp = fopen (data->filename, "wb")) == NULL)
    {
      g_warning ("Unable to save file %s", data->filename);
      return FALSE;
    }

  /* write magic header */
  fprintf (fp, "GIMP-VBR\n");

  /* write version */
  fprintf (fp, "1.0\n");

  /* write name */
  fprintf (fp, "%.255s\n", GIMP_OBJECT (brush)->name);

  /* write brush spacing */
  fprintf (fp, "%f\n", (gfloat) GIMP_BRUSH (brush)->spacing);

  /* write brush radius */
  fprintf (fp, "%f\n", brush->radius);

  /* write brush hardness */
  fprintf (fp, "%f\n", brush->hardness);

  /* write brush aspect_ratio */
  fprintf (fp, "%f\n", brush->aspect_ratio);

  /* write brush angle */
  fprintf (fp, "%f\n", brush->angle);

  fclose (fp);

  return TRUE;
}

static gchar *
gimp_brush_generated_get_extension (GimpData *data)
{
  return GIMP_BRUSH_GENERATED_FILE_EXTENSION;
}

static GimpData *
gimp_brush_generated_duplicate (GimpData *data)
{
  GimpBrushGenerated *brush;

  brush = GIMP_BRUSH_GENERATED (data);

  return gimp_brush_generated_new (brush->radius,
				   brush->hardness,
				   brush->angle,
				   brush->aspect_ratio);
}

static double
gauss (gdouble f)
{ 
  /* this aint' a real gauss function */
  if (f < -.5)
    {
      f = -1.0 - f;
      return (2.0 * f*f);
    }

  if (f < .5)
    return (1.0 - 2.0 * f*f);

  f = 1.0 -f;
  return (2.0 * f*f);
}

static void
gimp_brush_generated_dirty (GimpData *data)
{
  GimpBrushGenerated *brush;
  register GimpBrush *gbrush = NULL;
  register gint       x, y;
  register guchar    *centerp;
  register gdouble    d;
  register gdouble    exponent;
  register guchar     a;
  register gint       length;
  register guchar    *lookup;
  register gdouble    sum, c, s, tx, ty;
  gdouble buffer[OVERSAMPLING];
  gint    width, height;

  brush = GIMP_BRUSH_GENERATED (data);

  if (brush->freeze) /* if we are frozen defer rerendering till later */
    return;

  gbrush = GIMP_BRUSH (brush);

  if (stingy_memory_use && gbrush->mask)
    temp_buf_unswap (gbrush->mask);

  if (gbrush->mask)
    {
      temp_buf_free (gbrush->mask);
    }

  /* compute the range of the brush. should do a better job than this? */
  s = sin (gimp_deg_to_rad (brush->angle));
  c = cos (gimp_deg_to_rad (brush->angle));

  tx = MAX (fabs (c*ceil (brush->radius) - s*ceil (brush->radius)
		  / brush->aspect_ratio), 
	    fabs (c*ceil (brush->radius) + s*ceil (brush->radius)
		  / brush->aspect_ratio));
  ty = MAX (fabs (s*ceil (brush->radius) + c*ceil (brush->radius)
		  / brush->aspect_ratio),
	    fabs (s*ceil (brush->radius) - c*ceil (brush->radius)
		  / brush->aspect_ratio));

  if (brush->radius > tx)
    width = ceil (tx);
  else
    width = ceil (brush->radius);

  if (brush->radius > ty)
    height = ceil (ty);
  else
    height = ceil (brush->radius);

  /* compute the axis for spacing */
  GIMP_BRUSH (brush)->x_axis.x =        c * brush->radius;
  GIMP_BRUSH (brush)->x_axis.y = -1.0 * s * brush->radius;

  GIMP_BRUSH (brush)->y_axis.x = (s * brush->radius / brush->aspect_ratio);
  GIMP_BRUSH (brush)->y_axis.y = (c * brush->radius / brush->aspect_ratio);
  
  gbrush->mask = temp_buf_new (width * 2 + 1,
			       height * 2 + 1,
			       1, width, height, 0);
  centerp = &gbrush->mask->data[height * gbrush->mask->width + width];

  if ((1.0 - brush->hardness) < 0.000001)
    exponent = 1000000; 
  else
    exponent = 1/(1.0 - brush->hardness);

  /* set up lookup table */
  length = ceil (sqrt (2 * ceil (brush->radius+1) * ceil (brush->radius+1))+1) * OVERSAMPLING;
  lookup = g_malloc (length);
  sum = 0.0;

  for (x = 0; x < OVERSAMPLING; x++)
    {
      d = fabs ((x + 0.5) / OVERSAMPLING - 0.5);
      if (d > brush->radius)
	buffer[x] = 0.0;
      else
	/* buffer[x] =  (1.0 - pow (d/brush->radius, exponent)); */
	buffer[x] = gauss (pow (d/brush->radius, exponent));
      sum += buffer[x];
    }

  for (x = 0; d < brush->radius || sum > 0.00001; d += 1.0 / OVERSAMPLING)
    {
      sum -= buffer[x % OVERSAMPLING];
      if (d > brush->radius)
	buffer[x % OVERSAMPLING] = 0.0;
      else
	/* buffer[x%OVERSAMPLING] =  (1.0 - pow (d/brush->radius, exponent)); */
	buffer[x % OVERSAMPLING] = gauss (pow (d/brush->radius, exponent));
      sum += buffer[x%OVERSAMPLING];
      lookup[x++] = RINT (sum * (255.0 / OVERSAMPLING));
    }
  while (x < length)
    {
      lookup[x++] = 0;
    }
  /* compute one half and mirror it */
  for (y = 0; y <= height; y++)
    {
      for (x = -width; x <= width; x++)
	{
	  tx = c*x - s*y;
	  ty = c*y + s*x;
	  ty *= brush->aspect_ratio;
	  d = sqrt (tx*tx + ty*ty);
	  if (d < brush->radius+1)
	    a = lookup[(gint) RINT (d * OVERSAMPLING)];
	  else
	    a = 0;
	  centerp[   y*gbrush->mask->width + x] = a;
	  centerp[-1*y*gbrush->mask->width - x] = a;
	}
    }

  g_free (lookup);

  if (GIMP_DATA_CLASS (parent_class)->dirty)
    GIMP_DATA_CLASS (parent_class)->dirty (data);
}

GimpData *
gimp_brush_generated_new (gfloat radius,
			  gfloat hardness,
			  gfloat angle,
			  gfloat aspect_ratio)
{
  GimpBrushGenerated *brush;

  /* set up normal brush data */
  brush = GIMP_BRUSH_GENERATED (gtk_type_new (GIMP_TYPE_BRUSH_GENERATED));

  gimp_object_set_name (GIMP_OBJECT (brush), "Untitled");

  GIMP_BRUSH (brush)->spacing = 20;

  /* set up gimp_brush_generated data */
  brush->radius       = radius;
  brush->hardness     = hardness;
  brush->angle        = angle;
  brush->aspect_ratio = aspect_ratio;

  /* render brush mask */
  gimp_data_dirty (GIMP_DATA (brush));

  return GIMP_DATA (brush);
}

GimpData *
gimp_brush_generated_load (const gchar *filename)
{
  GimpBrushGenerated *brush;
  FILE               *fp;
  gchar               string[256];
  gfloat              fl;
  gfloat              version;

  if ((fp = fopen (filename, "rb")) == NULL)
    return NULL;

  /* make sure the file we are reading is the right type */
  fgets (string, 255, fp);
  
  if (strncmp (string, "GIMP-VBR", 8) != 0)
    return NULL;

  /* make sure we are reading a compatible version */
  fgets (string, 255, fp);
  sscanf (string, "%f", &version);
  g_return_val_if_fail (version < 2.0, NULL);

  /* create new brush */
  brush = GIMP_BRUSH_GENERATED (gtk_type_new (GIMP_TYPE_BRUSH_GENERATED));

  gimp_data_set_filename (GIMP_DATA (brush), filename);

  gimp_brush_generated_freeze (brush);

  /* read name */
  fgets (string, 255, fp);
  if (string[strlen (string) - 1] == '\n')
    string[strlen (string) - 1] = 0;
  gimp_object_set_name (GIMP_OBJECT (brush), string);

  /* read brush spacing */
  fscanf (fp, "%f", &fl);
  GIMP_BRUSH (brush)->spacing = fl;

  /* read brush radius */
  fscanf (fp, "%f", &fl);
  gimp_brush_generated_set_radius (brush, fl);

  /* read brush hardness */
  fscanf (fp, "%f", &fl);
  gimp_brush_generated_set_hardness (brush, fl);

  /* read brush aspect_ratio */
  fscanf (fp, "%f", &fl);
  gimp_brush_generated_set_aspect_ratio (brush, fl);

  /* read brush angle */
  fscanf (fp, "%f", &fl);
  gimp_brush_generated_set_angle (brush, fl);

  fclose (fp);

  gimp_brush_generated_thaw (brush);

  GIMP_DATA (brush)->dirty = FALSE;

  if (stingy_memory_use)
    temp_buf_swap (GIMP_BRUSH (brush)->mask);

  return GIMP_DATA (brush);
}

void
gimp_brush_generated_freeze (GimpBrushGenerated *brush)
{
  g_return_if_fail (brush != NULL);
  g_return_if_fail (GIMP_IS_BRUSH_GENERATED (brush));

  brush->freeze++;
}

void
gimp_brush_generated_thaw (GimpBrushGenerated *brush)
{
  g_return_if_fail (brush != NULL);
  g_return_if_fail (GIMP_IS_BRUSH_GENERATED (brush));
  
  if (brush->freeze > 0)
    brush->freeze--;

  if (brush->freeze == 0)
    gimp_data_dirty (GIMP_DATA (brush));
}

gfloat
gimp_brush_generated_set_radius (GimpBrushGenerated *brush,
				 gfloat              radius)
{
  g_return_val_if_fail (GIMP_IS_BRUSH_GENERATED (brush), -1.0);

  if (radius < 0.0)
    radius = 0.0;
  else if (radius > 32767.0)
    radius = 32767.0;

  if (radius == brush->radius)
    return radius;

  brush->radius = radius;

  if (! brush->freeze)
    gimp_data_dirty (GIMP_DATA (brush));

  return brush->radius;
}

gfloat
gimp_brush_generated_set_hardness (GimpBrushGenerated *brush,
				   gfloat              hardness)
{
  g_return_val_if_fail (GIMP_IS_BRUSH_GENERATED (brush), -1.0);

  if (hardness < 0.0)
    hardness = 0.0;
  else if (hardness > 1.0)
    hardness = 1.0;

  if (brush->hardness == hardness)
    return hardness;

  brush->hardness = hardness;

  if (!brush->freeze)
    gimp_data_dirty (GIMP_DATA (brush));

  return brush->hardness;
}

gfloat
gimp_brush_generated_set_angle (GimpBrushGenerated *brush,
				gfloat              angle)
{
  g_return_val_if_fail (GIMP_IS_BRUSH_GENERATED (brush), -1.0);

  if (angle < 0.0)
    angle = -1.0 * fmod (angle, 180.0);
  else if (angle > 180.0)
    angle = fmod (angle, 180.0);

  if (brush->angle == angle)
    return angle;

  brush->angle = angle;

  if (!brush->freeze)
    gimp_data_dirty (GIMP_DATA (brush));

  return brush->angle;
}

gfloat
gimp_brush_generated_set_aspect_ratio (GimpBrushGenerated *brush,
				       gfloat              ratio)
{
  g_return_val_if_fail (GIMP_IS_BRUSH_GENERATED (brush), -1.0);

  if (ratio < 1.0)
    ratio = 1.0;
  else if (ratio > 1000)
    ratio = 1000;

  if (brush->aspect_ratio == ratio)
    return ratio;

  brush->aspect_ratio = ratio;

  if (!brush->freeze)
    gimp_data_dirty (GIMP_DATA (brush));

  return brush->aspect_ratio;
}

gfloat
gimp_brush_generated_get_radius (const GimpBrushGenerated *brush)
{
  g_return_val_if_fail (GIMP_IS_BRUSH_GENERATED (brush), -1.0);

  return brush->radius;
}

gfloat
gimp_brush_generated_get_hardness (const GimpBrushGenerated *brush)
{
  g_return_val_if_fail (GIMP_IS_BRUSH_GENERATED (brush), -1.0);

  return brush->hardness;
}

gfloat
gimp_brush_generated_get_angle (const GimpBrushGenerated *brush)
{
  g_return_val_if_fail (GIMP_IS_BRUSH_GENERATED (brush), -1.0);

  return brush->angle;
}

gfloat
gimp_brush_generated_get_aspect_ratio (const GimpBrushGenerated *brush)
{
  g_return_val_if_fail (GIMP_IS_BRUSH_GENERATED (brush), -1.0);

  return brush->aspect_ratio;
}
