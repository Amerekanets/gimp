/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * gimpintl.h
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

#ifndef __GIMPINTL_H__
#define __GIMPINTL_H__

#include <glib.h>
#include <locale.h>

/* Copied from gnome-i18n.h by Tom Tromey <tromey@creche.cygnus.com>
 * Heavily modified by Daniel Egger <Daniel.Egger@t-online.de>
 * So be sure to hit me instead of him if something is wrong here
 */ 

#ifndef LOCALEDIR
#define LOCALEDIR g_strconcat (gimp_data_directory (), \
			       G_DIR_SEPARATOR_S, \
			       "locale", \
			       NULL)
#endif

#ifdef ENABLE_NLS
#    include <libintl.h>
#    define _(String) gettext (String)
#    ifdef gettext_noop
#        define N_(String) gettext_noop (String)
#    else
#        define N_(String) (String)
#    endif
#else
/* Stubs that do something close enough.  */
#    define textdomain(String) (String)
#    define gettext(String) (String)
#    define dgettext(Domain,Message) (Message)
#    define dcgettext(Domain,Message,Type) (Message)
#    define bindtextdomain(Domain,Directory) (Domain)
#    define _(String) (String)
#    define N_(String) (String)
#endif

#ifndef HAVE_BIND_TEXTDOMAIN_CODESET
#    define bind_textdomain_codeset(Domain, Codeset) (Domain)
#endif

#define INIT_LOCALE( domain )	G_STMT_START{	\
     setlocale (LC_ALL, "");                    \
     bindtextdomain (domain, LOCALEDIR);	\
     bind_textdomain_codeset (domain, "UTF-8"); \
     textdomain (domain);			\
}G_STMT_END

#endif /* __GIMPINTL_H__ */
