/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 * Copyright (C) 1998 Andy Thomas (alt@picnic.demon.co.uk)
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

#include <string.h>

#include <gtk/gtk.h>

#include "libgimpwidgets/gimpwidgets.h"

#include "apptypes.h"

#include "gimppalette.h"
#include "palette_select.h"
#include "palette.h"
#include "paletteP.h"

#include "libgimp/gimpintl.h"


/*  local function prototypes  */

static gint   palette_select_button_press   (GtkWidget      *widget,
					     GdkEventButton *bevent,
					     gpointer        data);
static void   palette_select_close_callback (GtkWidget      *widget,
					     gpointer        data);
static void   palette_select_edit_callback  (GtkWidget      *widget,
					     gpointer        data);


/*  list of active dialogs  */

static GSList *active_dialogs = NULL;


/*  public functions  */

PaletteSelect *
palette_select_new (const gchar *title,
		    const gchar *initial_palette)
{
  GimpPalette   *p_entries = NULL;
  PaletteSelect *psp;
  GSList        *list;
  GtkWidget     *vbox;
  GtkWidget     *hbox;
  GtkWidget     *scrolled_win;
  gchar         *titles[3];
  gint           select_pos;

  palette_select_palette_init ();

  psp = g_new (PaletteSelect, 1);
  psp->callback_name = NULL;
  
  /*  The shell and main vbox  */
  psp->shell = gimp_dialog_new (title ? title : _("Palette Selection"),
				"palette_selection",
				gimp_standard_help_func,
				"dialogs/palette_selection.html",
				GTK_WIN_POS_MOUSE,
				FALSE, TRUE, FALSE,

				_("Edit"), palette_select_edit_callback,
				psp, NULL, NULL, TRUE, FALSE,
				_("Close"), palette_select_close_callback,
				psp, NULL, NULL, FALSE, TRUE,

				NULL);

  vbox = gtk_vbox_new (FALSE, 1);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 1);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (psp->shell)->vbox), vbox);

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  /* clist preview of gradients */
  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_ALWAYS);
  gtk_box_pack_start (GTK_BOX (hbox), scrolled_win, TRUE, TRUE, 0); 
  gtk_widget_show (scrolled_win);

  titles[0] = _("Palette");
  titles[1] = _("Ncols");
  titles[2] = _("Name");
  psp->clist = gtk_clist_new_with_titles (3, titles);
  gtk_clist_set_shadow_type (GTK_CLIST (psp->clist), GTK_SHADOW_IN);
  gtk_clist_set_row_height (GTK_CLIST (psp->clist), SM_PREVIEW_HEIGHT + 2);
  gtk_clist_set_use_drag_icons (GTK_CLIST (psp->clist), FALSE);
  gtk_clist_column_titles_passive (GTK_CLIST (psp->clist));
  gtk_widget_set_usize (psp->clist, 203, 200);
  gtk_clist_set_column_width (GTK_CLIST (psp->clist), 0, SM_PREVIEW_WIDTH + 2);
  gtk_container_add (GTK_CONTAINER (scrolled_win), psp->clist);
  gtk_widget_show (psp->clist);

  select_pos = -1;
  if (initial_palette && strlen (initial_palette))
    {
      for (list = palettes_list; list; list = g_slist_next (list))
	{
	  p_entries = (GimpPalette *) list->data;
	  
	  if (strcmp (GIMP_OBJECT (p_entries)->name, initial_palette) > 0)
	    break;

	  select_pos++;
	}
    }

  gtk_widget_realize (psp->shell);
  psp->gc = gdk_gc_new (psp->shell->window);  
  
  palette_clist_init (psp->clist, psp->shell, psp->gc);
  gtk_signal_connect (GTK_OBJECT (psp->clist), "button_press_event",
		      GTK_SIGNAL_FUNC (palette_select_button_press),
		      (gpointer) psp);

  /* Now show the dialog */
  gtk_widget_show (vbox);
  gtk_widget_show (psp->shell);

  if (select_pos != -1) 
    {
      gtk_clist_select_row (GTK_CLIST (psp->clist), select_pos, -1);
      gtk_clist_moveto (GTK_CLIST (psp->clist), select_pos, 0, 0.0, 0.0); 
    }
  else
    gtk_clist_select_row (GTK_CLIST (psp->clist), 0, -1);

  active_dialogs = g_slist_append (active_dialogs, psp);

  return psp;
}

void
palette_select_clist_insert_all (GimpPalette *p_entries)
{
  GimpPalette   *chk_entries;
  PaletteSelect *psp; 
  GSList        *list;
  gint           pos = 0;

  for (list = palettes_list; list; list = g_slist_next (list))
    {
      chk_entries = (GimpPalette *) list->data;
      
      /*  to make sure we get something!  */
      if (chk_entries == NULL)
	return;

      if (strcmp (GIMP_OBJECT (p_entries)->name,
		  GIMP_OBJECT (chk_entries)->name) == 0)
	break;

      pos++;
    }

  for (list = active_dialogs; list; list = g_slist_next (list))
    {
      psp = (PaletteSelect *) list->data;

      gtk_clist_freeze (GTK_CLIST (psp->clist));
      palette_clist_insert (psp->clist, psp->shell, psp->gc, p_entries, pos);
      gtk_clist_thaw (GTK_CLIST (psp->clist));
    }
}

void
palette_select_set_text_all (GimpPalette *entries)
{
  GimpPalette   *p_entries = NULL;
  PaletteSelect *psp; 
  GSList        *list;
  gchar         *num_buf;
  gint           pos = 0;

  for (list = palettes_list; list;  list = g_slist_next (list))
    {
      p_entries = (GimpPalette *) list->data;
      
      if (p_entries == entries)
	break;

      pos++;
    }

  if (p_entries == NULL)
    return; /* This is actually an error */

  num_buf = g_strdup_printf ("%d",p_entries->n_colors);;

  for (list = active_dialogs; list; list = g_slist_next (list))
    {
      psp = (PaletteSelect *) list->data;

      gtk_clist_set_text (GTK_CLIST (psp->clist), pos, 1, num_buf);
    }

  g_free (num_buf);
}

void
palette_select_refresh_all (void)
{
  PaletteSelect *psp; 
  GSList        *list;

  for (list = active_dialogs; list; list = g_slist_next (list))
    {
      psp = (PaletteSelect *) list->data;

      gtk_clist_freeze (GTK_CLIST (psp->clist));
      gtk_clist_clear (GTK_CLIST (psp->clist));
      palette_clist_init (psp->clist, psp->shell, psp->gc);
      gtk_clist_thaw (GTK_CLIST (psp->clist));
    }
}

/*  local functions  */

static gint
palette_select_button_press (GtkWidget      *widget,
			     GdkEventButton *bevent,
			     gpointer        data)
{
  PaletteSelect *psp;

  psp = (PaletteSelect *) data;

  if (bevent->button == 1 && bevent->type == GDK_2BUTTON_PRESS)
    {
      palette_select_edit_callback (widget, data);

      return TRUE;
    }

  return FALSE;
}

static void
palette_select_edit_callback (GtkWidget *widget,
			      gpointer   data)
{
  GimpPalette   *p_entries = NULL;
  PaletteSelect *psp;
  GList         *sel_list;

  psp = (PaletteSelect *) data;

  sel_list = GTK_CLIST (psp->clist)->selection;

  while (sel_list)
    {
      gint row;

      row = GPOINTER_TO_INT (sel_list->data);

      p_entries = 
	(GimpPalette *) gtk_clist_get_row_data (GTK_CLIST (psp->clist), row);

      palette_create_edit (p_entries);

      /* One only */
      return;
    }
}

static void
palette_select_free (PaletteSelect *psp)
{
  if (psp)
    {
      /*
      if(psp->callback_name)
	g_free (gsp->callback_name);
      */

      /* remove from active list */
      active_dialogs = g_slist_remove (active_dialogs, psp); 

      g_free (psp);
    }
}

static void
palette_select_close_callback (GtkWidget *widget,
			       gpointer   data)
{
  PaletteSelect *psp;

  psp = (PaletteSelect *) data;

  if (GTK_WIDGET_VISIBLE (psp->shell))
    gtk_widget_hide (psp->shell);

  gtk_widget_destroy (psp->shell); 
  palette_select_free (psp); 
}
