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

#ifndef __GIMP_DISPLAY_SHELL_APPEARANCE_H__
#define __GIMP_DISPLAY_SHELL_APPEARANCE_H__


void       gimp_display_shell_set_padding         (GimpDisplayShell *shell,
                                                   GimpDisplayPaddingMode  mode,
                                                   GimpRGB          *color);

void       gimp_display_shell_set_fullscreen      (GimpDisplayShell *shell,
                                                   gboolean          fullscreen);
gboolean   gimp_display_shell_get_fullscreen      (GimpDisplayShell *shell);

void       gimp_display_shell_set_show_menubar    (GimpDisplayShell *shell,
                                                   gboolean          show);
gboolean   gimp_display_shell_get_show_menubar    (GimpDisplayShell *shell);

void       gimp_display_shell_set_show_rulers     (GimpDisplayShell *shell,
                                                   gboolean          show);
gboolean   gimp_display_shell_get_show_rulers     (GimpDisplayShell *shell);

void       gimp_display_shell_set_show_scrollbars (GimpDisplayShell *shell,
                                                   gboolean          show);
gboolean   gimp_display_shell_get_show_scrollbars (GimpDisplayShell *shell);

void       gimp_display_shell_set_show_statusbar  (GimpDisplayShell *shell,
                                                   gboolean          show);
gboolean   gimp_display_shell_get_show_statusbar  (GimpDisplayShell *shell);


#endif /* __GIMP_DISPLAY_SHELL_APPEARANCE_H__ */
