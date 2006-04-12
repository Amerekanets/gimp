/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-2003 Peter Mattis and Spencer Kimball
 *
 * gimpbrushselect_pdb.c
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* NOTE: This file is autogenerated by pdbgen.pl */

#include "config.h"

#include "gimp.h"

/**
 * gimp_brushes_popup:
 * @brush_callback: The callback PDB proc to call when brush selection is made.
 * @popup_title: Title to give the brush popup window.
 * @initial_brush: The name of the brush to set as the first selected.
 * @opacity: The initial opacity of the brush.
 * @spacing: The initial spacing of the brush (if < 0 then use brush default spacing).
 * @paint_mode: The initial paint mode.
 *
 * Invokes the Gimp brush selection.
 *
 * This procedure popups the brush selection dialog.
 *
 * Returns: TRUE on success.
 */
gboolean
gimp_brushes_popup (const gchar          *brush_callback,
                    const gchar          *popup_title,
                    const gchar          *initial_brush,
                    gdouble               opacity,
                    gint                  spacing,
                    GimpLayerModeEffects  paint_mode)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gboolean success = TRUE;

  return_vals = gimp_run_procedure ("gimp-brushes-popup",
                                    &nreturn_vals,
                                    GIMP_PDB_STRING, brush_callback,
                                    GIMP_PDB_STRING, popup_title,
                                    GIMP_PDB_STRING, initial_brush,
                                    GIMP_PDB_FLOAT, opacity,
                                    GIMP_PDB_INT32, spacing,
                                    GIMP_PDB_INT32, paint_mode,
                                    GIMP_PDB_END);

  success = return_vals[0].data.d_status == GIMP_PDB_SUCCESS;

  gimp_destroy_params (return_vals, nreturn_vals);

  return success;
}

/**
 * gimp_brushes_close_popup:
 * @brush_callback: The name of the callback registered for this popup.
 *
 * Popdown the Gimp brush selection.
 *
 * This procedure closes an opened brush selection dialog.
 *
 * Returns: TRUE on success.
 */
gboolean
gimp_brushes_close_popup (const gchar *brush_callback)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gboolean success = TRUE;

  return_vals = gimp_run_procedure ("gimp-brushes-close-popup",
                                    &nreturn_vals,
                                    GIMP_PDB_STRING, brush_callback,
                                    GIMP_PDB_END);

  success = return_vals[0].data.d_status == GIMP_PDB_SUCCESS;

  gimp_destroy_params (return_vals, nreturn_vals);

  return success;
}

/**
 * gimp_brushes_set_popup:
 * @brush_callback: The name of the callback registered for this popup.
 * @brush_name: The name of the brush to set as selected.
 * @opacity: The initial opacity of the brush.
 * @spacing: The initial spacing of the brush (if < 0 then use brush default spacing).
 * @paint_mode: The initial paint mode.
 *
 * Sets the current brush selection in a popup.
 *
 * Sets the current brush selection in a popup.
 *
 * Returns: TRUE on success.
 */
gboolean
gimp_brushes_set_popup (const gchar          *brush_callback,
                        const gchar          *brush_name,
                        gdouble               opacity,
                        gint                  spacing,
                        GimpLayerModeEffects  paint_mode)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gboolean success = TRUE;

  return_vals = gimp_run_procedure ("gimp-brushes-set-popup",
                                    &nreturn_vals,
                                    GIMP_PDB_STRING, brush_callback,
                                    GIMP_PDB_STRING, brush_name,
                                    GIMP_PDB_FLOAT, opacity,
                                    GIMP_PDB_INT32, spacing,
                                    GIMP_PDB_INT32, paint_mode,
                                    GIMP_PDB_END);

  success = return_vals[0].data.d_status == GIMP_PDB_SUCCESS;

  gimp_destroy_params (return_vals, nreturn_vals);

  return success;
}
