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

#ifndef __APPENV_H__
#define __APPENV_H__


typedef enum
{
  MESSAGE_BOX,
  CONSOLE,
  ERROR_CONSOLE
} MessageHandlerType;


extern gboolean no_interface;
extern gboolean no_splash;
extern gboolean no_splash_image;
extern gboolean no_data;
extern gboolean be_verbose;
extern gboolean use_debug_handler;
extern gboolean console_messages;
extern gboolean restore_session;
extern gboolean double_speed;

extern MessageHandlerType message_handler;

/* FIXME: gimp_busy HACK */
extern gboolean gimp_busy;


#endif /*  __APPENV_H__  */
