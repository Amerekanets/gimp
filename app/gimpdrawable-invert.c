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

#include "drawable.h"
#include "gimpimage.h"
#include "gimplut.h"
#include "invert.h"
#include "lut_funcs.h"
#include "pixel_processor.h"
#include "pixel_region.h"

#include "pdb/procedural_db.h"

#include "libgimp/gimpintl.h"


void
image_invert (GimpImage *gimage)
{
  GimpDrawable *drawable;
  Argument     *return_vals;
  gint          nreturn_vals;

  drawable = gimp_image_active_drawable (gimage);

  if (gimp_drawable_is_indexed (drawable))
    {
      g_message (_("Invert does not operate on indexed drawables."));
      return;
    }

  return_vals =
    procedural_db_run_proc ("gimp_invert",
			    &nreturn_vals,
			    PDB_DRAWABLE, gimp_drawable_get_ID (drawable),
			    PDB_END);

  if (!return_vals || return_vals[0].value.pdb_int != PDB_SUCCESS)
    g_message (_("Invert operation failed."));

  procedural_db_destroy_args (return_vals, nreturn_vals);
}


/*  Inverter  */

void
invert (GimpDrawable *drawable)
{
  PixelRegion  srcPR, destPR;
  gint         x1, y1, x2, y2;
  GimpLut     *lut;

  lut = invert_lut_new (gimp_drawable_bytes (drawable));

  gimp_drawable_mask_bounds (drawable, &x1, &y1, &x2, &y2);
  pixel_region_init (&srcPR, gimp_drawable_data (drawable),
		     x1, y1, (x2 - x1), (y2 - y1), FALSE);
  pixel_region_init (&destPR, gimp_drawable_shadow (drawable),
		     x1, y1, (x2 - x1), (y2 - y1), TRUE);

  pixel_regions_process_parallel ((p_func)gimp_lut_process, lut, 
				  2, &srcPR, &destPR);

  gimp_lut_free (lut);

  gimp_drawable_merge_shadow (drawable, TRUE);

  drawable_update (drawable,
		   x1, y1,
		   (x2 - x1), (y2 - y1));
}
