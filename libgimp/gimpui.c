/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
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

#include "gimp.h"
#include "gimpui.h"

#include "libgimpwidgets/gimpwidgets.h"

/**
 * gimp_ui_init:
 * @prog_name: The name of the plug-in which will be passed as argv[0] to
 *             gtk_init(). It's a convention to use the name of the
 *             executable and _not_ the PDB procedure name or something.
 * @preview: #TRUE if the plug-in has some kind of preview in it's UI.
 *           Note that passing #TRUE is recommended also if one of the
 *           used GIMP Library widgets contains a preview (like the image
 *           menu returned by gimp_image_menu_new()).
 *
 * This function initializes GTK+ with gtk_init() and initializes GDK's 
 * image rendering subsystem (GdkRGB) to follow the GIMP main program's 
 * colormap allocation/installation policy.
 *
 * The GIMP's colormap policy can be determinded by the user with the
 * gimprc variables @min_colors and @install_cmap.
 **/
void
gimp_ui_init (const gchar *prog_name,
	      gboolean     preview)
{
  gint    argc;
  gchar **argv;
  gchar  *user_gtkrc;

  static gboolean initialized = FALSE;

  g_return_if_fail (prog_name != NULL);

  if (initialized)
    return;

  argc    = 1;
  argv    = g_new (gchar *, 1);
  argv[0] = g_strdup (prog_name);

  gtk_init (&argc, &argv);
  gtk_rc_parse (gimp_gtkrc ());

  user_gtkrc = gimp_personal_rc_file ("gtkrc");
  gtk_rc_parse (user_gtkrc);
  g_free (user_gtkrc);

  gdk_rgb_set_min_colors (gimp_min_colors ());
  gdk_rgb_set_install (gimp_install_cmap ());

  gtk_widget_set_default_colormap (gdk_rgb_get_colormap ());

  /*  Set the gamma after installing the colormap because
   *  gtk_preview_set_gamma() initializes GdkRGB if not already done
   */
  if (preview)
    gtk_preview_set_gamma (gimp_gamma ());

  gimp_stock_init ();

  initialized = TRUE;
}
