/* The GIMP -- an image manipulation program
 * Copyright (C) 1995-2000 Spencer Kimball and Peter Mattis
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

/* NOTE: This file is autogenerated by pdbgen.pl. */

#include "config.h"

#include <glib.h>
#include "app_procs.h"

#include "libgimp/gimpintl.h"

/* Forward declarations for registering PDB procs */

void register_brush_select_procs    (void);
void register_brushes_procs         (void);
void register_channel_procs         (void);
void register_channel_ops_procs     (void);
void register_color_procs           (void);
void register_convert_procs         (void);
void register_drawable_procs        (void);
void register_edit_procs            (void);
void register_fileops_procs         (void);
void register_floating_sel_procs    (void);
void register_gdisplay_procs        (void);
void register_gimage_procs          (void);
void register_gimage_mask_procs     (void);
void register_gimprc_procs          (void);
void register_gimphelp_procs        (void);
void register_gradient_procs        (void);
void register_gradient_select_procs (void);
void register_guides_procs          (void);
void register_interface_procs       (void);
void register_layer_procs           (void);
void register_misc_procs            (void);
void register_palette_procs         (void);
void register_parasite_procs        (void);
void register_paths_procs           (void);
void register_pattern_select_procs  (void);
void register_patterns_procs        (void);
void register_plug_in_procs         (void);
void register_procedural_db_procs   (void);
void register_text_tool_procs       (void);
void register_tools_procs           (void);
void register_undo_procs            (void);
void register_unit_procs            (void);

/* 322 procedures registered total */

void
internal_procs_init (void)
{
  app_init_update_status (_("Internal Procedures"), _("Brush UI"), 0.0);
  register_brush_select_procs ();

  app_init_update_status (NULL, _("Brushes"), 0.009);
  register_brushes_procs ();

  app_init_update_status (NULL, _("Channel"), 0.043);
  register_channel_procs ();

  app_init_update_status (NULL, _("Channel Ops"), 0.09);
  register_channel_ops_procs ();

  app_init_update_status (NULL, _("Color"), 0.096);
  register_color_procs ();

  app_init_update_status (NULL, _("Convert"), 0.134);
  register_convert_procs ();

  app_init_update_status (NULL, _("Drawable procedures"), 0.143);
  register_drawable_procs ();

  app_init_update_status (NULL, _("Edit procedures"), 0.211);
  register_edit_procs ();

  app_init_update_status (NULL, _("File Operations"), 0.23);
  register_fileops_procs ();

  app_init_update_status (NULL, _("Floating selections"), 0.255);
  register_floating_sel_procs ();

  app_init_update_status (NULL, _("GDisplay procedures"), 0.273);
  register_gdisplay_procs ();

  app_init_update_status (NULL, _("Image"), 0.283);
  register_gimage_procs ();

  app_init_update_status (NULL, _("Image mask"), 0.466);
  register_gimage_mask_procs ();

  app_init_update_status (NULL, _("Gimprc procedures"), 0.519);
  register_gimprc_procs ();

  app_init_update_status (NULL, _("Help procedures"), 0.528);
  register_gimphelp_procs ();

  app_init_update_status (NULL, _("Gradients"), 0.531);
  register_gradient_procs ();

  app_init_update_status (NULL, _("Gradient UI"), 0.547);
  register_gradient_select_procs ();

  app_init_update_status (NULL, _("Guide procedures"), 0.559);
  register_guides_procs ();

  app_init_update_status (NULL, _("Interface"), 0.578);
  register_interface_procs ();

  app_init_update_status (NULL, _("Layer"), 0.587);
  register_layer_procs ();

  app_init_update_status (NULL, _("Miscellaneous"), 0.683);
  register_misc_procs ();

  app_init_update_status (NULL, _("Palette"), 0.689);
  register_palette_procs ();

  app_init_update_status (NULL, _("Parasite procedures"), 0.711);
  register_parasite_procs ();

  app_init_update_status (NULL, _("Paths"), 0.752);
  register_paths_procs ();

  app_init_update_status (NULL, _("Pattern UI"), 0.792);
  register_pattern_select_procs ();

  app_init_update_status (NULL, _("Patterns"), 0.801);
  register_patterns_procs ();

  app_init_update_status (NULL, _("Plug-in"), 0.814);
  register_plug_in_procs ();

  app_init_update_status (NULL, _("Procedural database"), 0.829);
  register_procedural_db_procs ();

  app_init_update_status (NULL, _("Text procedures"), 0.854);
  register_text_tool_procs ();

  app_init_update_status (NULL, _("Tool procedures"), 0.866);
  register_tools_procs ();

  app_init_update_status (NULL, _("Undo"), 0.96);
  register_undo_procs ();

  app_init_update_status (NULL, _("Units"), 0.966);
  register_unit_procs ();
}
