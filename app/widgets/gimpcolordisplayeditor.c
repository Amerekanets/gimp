/* The GIMP -- an image manipulation program
 * Copyright (C) 1999 Manish Singh
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

#ifdef __GNUC__
#warning GTK_DISABLE_DEPRECATED
#endif
#undef GTK_DISABLE_DEPRECATED
#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "display-types.h"

#include "core/gimpimage.h"

#include "gimpdisplay.h"
#include "gimpdisplayshell.h"
#include "gimpdisplayshell-filter.h"
#include "gimpdisplayshell-filter-dialog.h"

#include "libgimp/gimpintl.h"


typedef struct _ColorDisplayDialog ColorDisplayDialog;

struct _ColorDisplayDialog
{
  GimpDisplayShell *shell;

  GtkWidget *dialog;

  GtkWidget *src;
  GtkWidget *dest;

  gint src_row;
  gint dest_row;

  gboolean modified;

  GList *old_nodes;
  GList *conf_nodes;

  GtkWidget *buttons[5];
};

enum
{
  BUTTON_ADD,
  BUTTON_REMOVE,
  BUTTON_UP,
  BUTTON_DOWN,
  BUTTON_CONFIGURE
};

typedef void (*ButtonCallback) (GtkWidget *, gpointer);

typedef struct _ButtonInfo ButtonInfo;

struct _ButtonInfo
{
  const gchar    *label;
  ButtonCallback  callback;
};

static void make_dialog (ColorDisplayDialog *cdd);

static void color_display_ok_callback        (GtkWidget *widget,
					      gpointer   data);
static void color_display_cancel_callback    (GtkWidget *widget,
					      gpointer   data);
static void color_display_add_callback       (GtkWidget *widget,
					      gpointer   data);
static void color_display_remove_callback    (GtkWidget *widget,
					      gpointer   data);
static void color_display_up_callback        (GtkWidget *widget,
					      gpointer   data);
static void color_display_down_callback      (GtkWidget *widget,
					      gpointer   data);
static void color_display_configure_callback (GtkWidget *widget,
					      gpointer   data);

static void src_list_populate  (const char *name,
				gpointer    data);
static void dest_list_populate (GList      *node_list,
    				GtkWidget  *dest);

static void select_src    (GtkWidget       *widget,
                           gint             row,
                           gint             column,
                           GdkEventButton  *event,
                           gpointer         data);
static void unselect_src  (GtkWidget       *widget,
                           gint             row,
                           gint             column,
                           GdkEventButton  *event,
                           gpointer         data);
static void select_dest   (GtkWidget       *widget,
			   gint             row,
			   gint             column,
			   GdkEventButton  *event,
			   gpointer         data);
static void unselect_dest (GtkWidget       *widget,
                           gint             row,
                           gint             column,
                           GdkEventButton  *event,
                           gpointer         data);

static void color_display_update_up_and_down(ColorDisplayDialog *cdd);

#define LIST_WIDTH  180
#define LIST_HEIGHT 180

#define UPDATE_DISPLAY(shell) G_STMT_START      \
{	                                        \
  gimp_display_shell_expose_full (shell);       \
  gimp_display_shell_flush (shell);             \
} G_STMT_END  

static void
make_dialog (ColorDisplayDialog *cdd)
{
  GtkWidget *hbox;
  GtkWidget *scrolled_win;
  GtkWidget *vbbox;
  gchar     *titles[2];
  gint       i;

  static ButtonInfo buttons[] = 
  {
    { N_("Add"),       color_display_add_callback       },
    { N_("Remove"),    color_display_remove_callback    },
    { N_("Up"),        color_display_up_callback        },
    { N_("Down"),      color_display_down_callback      },
    { N_("Configure"), color_display_configure_callback }
  };

  cdd->dialog = gimp_dialog_new (_("Color Display Filters"), "display_color",
				gimp_standard_help_func,
				"dialogs/display_filters/display_filters.html",
				GTK_WIN_POS_NONE,
				FALSE, TRUE, FALSE,

				GTK_STOCK_CANCEL, color_display_cancel_callback,
				cdd, NULL, NULL, FALSE, TRUE,

				GTK_STOCK_OK, color_display_ok_callback,
				cdd, NULL, NULL, TRUE, FALSE,

				NULL);

  hbox = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (cdd->dialog)->vbox), hbox,
		      TRUE, TRUE, 4);

  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_set_border_width (GTK_CONTAINER (scrolled_win), 5);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				  GTK_POLICY_AUTOMATIC, 
				  GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (hbox), scrolled_win, TRUE, TRUE, 0);

  titles[0] = _("Available Filters");
  titles[1] = NULL;
  cdd->src = gtk_clist_new_with_titles (1, titles);
  gtk_widget_set_usize (cdd->src, LIST_WIDTH, LIST_HEIGHT);
  gtk_clist_column_titles_passive (GTK_CLIST (cdd->src));
  gtk_clist_set_auto_sort (GTK_CLIST (cdd->src), TRUE);
  gtk_container_add (GTK_CONTAINER (scrolled_win), cdd->src);

  g_signal_connect (G_OBJECT (cdd->src), "select_row",
                    G_CALLBACK (select_src),
                    cdd);
  g_signal_connect (G_OBJECT (cdd->src), "unselect_row",
                    G_CALLBACK (unselect_src),
                    cdd);

  vbbox = gtk_vbutton_box_new ();
  gtk_vbutton_box_set_layout_default (GTK_BUTTONBOX_START);
  gtk_box_pack_start (GTK_BOX (hbox), vbbox, FALSE, FALSE, 2);

  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_set_border_width (GTK_CONTAINER (scrolled_win), 5);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				  GTK_POLICY_AUTOMATIC, 
				  GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (hbox), scrolled_win, TRUE, TRUE, 0);

  titles[0] = _("Active Filters");
  titles[1] = NULL;
  cdd->dest = gtk_clist_new_with_titles (1, titles);
  gtk_widget_set_usize (cdd->dest, LIST_WIDTH, LIST_HEIGHT);
  gtk_clist_column_titles_passive (GTK_CLIST (cdd->dest));
  gtk_container_add (GTK_CONTAINER (scrolled_win), cdd->dest);

  g_signal_connect (G_OBJECT (cdd->dest), "select_row",
                    G_CALLBACK (select_dest),
                    cdd);
  g_signal_connect (G_OBJECT (cdd->dest), "unselect_row",
                    G_CALLBACK (unselect_dest),
                    cdd);

  for (i = 0; i < 5; i++)
    {
       cdd->buttons[i] = 
	 gtk_button_new_with_label (gettext (buttons[i].label));
       gtk_box_pack_start (GTK_BOX (vbbox), cdd->buttons[i], FALSE, FALSE, 0);

       g_signal_connect (G_OBJECT (cdd->buttons[i]), "clicked",
                         G_CALLBACK (buttons[i].callback),
                         cdd);
       gtk_widget_set_sensitive (cdd->buttons[i], FALSE);
    }

  gtk_widget_show_all (hbox);
}

static void
color_display_ok_callback (GtkWidget *widget,
			   gpointer   data)
{
  ColorDisplayDialog *cdd   = data;
  GimpDisplayShell   *shell = cdd->shell;
  GList              *list;

  gtk_widget_destroy (GTK_WIDGET (cdd->dialog));
  shell->cd_ui = NULL;

  if (cdd->modified)
    {
      for (list = cdd->old_nodes; list; list = g_list_next (list))
	{
	  if (! g_list_find (shell->cd_list, list->data))
	    gimp_display_shell_filter_detach_destroy (shell, list->data);
	}

      g_list_free (cdd->old_nodes);

      UPDATE_DISPLAY (shell);
    }
}

static void
color_display_cancel_callback (GtkWidget *widget,
			       gpointer   data)
{
  ColorDisplayDialog *cdd   = data;
  GimpDisplayShell   *shell = cdd->shell;
  GList *list;
  GList *next;

  gtk_widget_destroy (GTK_WIDGET (cdd->dialog));
  shell->cd_ui = NULL;
  
  if (cdd->modified)
    {
      list = shell->cd_list;
      shell->cd_list = cdd->old_nodes;

      while (list)
	{
	  next = list->next;

	  if (! g_list_find (cdd->old_nodes, list->data))
	    gimp_display_shell_filter_detach_destroy (shell, list->data);

	  list = next;
	}

      UPDATE_DISPLAY (shell);
    }
}

static void 
color_display_update_up_and_down (ColorDisplayDialog *cdd)
{
  gtk_widget_set_sensitive (cdd->buttons[BUTTON_UP],   cdd->dest_row > 0);
  gtk_widget_set_sensitive (cdd->buttons[BUTTON_DOWN], cdd->dest_row >= 0 &&
			    cdd->dest_row < GTK_CLIST (cdd->dest)->rows - 1);
}

static void
color_display_add_callback (GtkWidget *widget,
			    gpointer   data)
{
  ColorDisplayDialog *cdd   = data;
  GimpDisplayShell   *shell = cdd->shell;
  gchar              *name  = NULL;
  ColorDisplayNode   *node;
  gint                row;

  if (cdd->src_row < 0)
    return;

  gtk_clist_get_text (GTK_CLIST (cdd->src), cdd->src_row, 0, &name);

  if (!name)
    return;

  cdd->modified = TRUE;

  node = gimp_display_shell_filter_attach (shell, name);

  row = gtk_clist_append (GTK_CLIST (cdd->dest), &name);
  gtk_clist_set_row_data (GTK_CLIST (cdd->dest), row, node);

  color_display_update_up_and_down (cdd);

  UPDATE_DISPLAY (shell);
}

static void
color_display_remove_callback (GtkWidget *widget,
			       gpointer   data)
{
  ColorDisplayDialog *cdd   = data;
  GimpDisplayShell   *shell = cdd->shell;
  ColorDisplayNode   *node;

  if (cdd->dest_row < 0)
    return;
  
  node = gtk_clist_get_row_data (GTK_CLIST (cdd->dest), cdd->dest_row);
  gtk_clist_remove (GTK_CLIST (cdd->dest), cdd->dest_row);

  cdd->modified = TRUE;

  if (g_list_find (cdd->old_nodes, node))
    gimp_display_shell_filter_detach (shell, node);
  else
    gimp_display_shell_filter_detach_destroy (shell, node);

  cdd->dest_row = -1;

  color_display_update_up_and_down (cdd);
  
  UPDATE_DISPLAY (shell);
}

static void
color_display_up_callback (GtkWidget *widget,
			   gpointer   data)
{
  ColorDisplayDialog *cdd   = data;
  GimpDisplayShell   *shell = cdd->shell;
  ColorDisplayNode   *node;

  if (cdd->dest_row < 1)
    return;

  node = gtk_clist_get_row_data (GTK_CLIST (cdd->dest), cdd->dest_row);
  gtk_clist_row_move (GTK_CLIST (cdd->dest), cdd->dest_row, cdd->dest_row - 1);

  gimp_display_shell_filter_reorder_up (shell, node);
  cdd->modified = TRUE;

  cdd->dest_row--;

  color_display_update_up_and_down (cdd);

  UPDATE_DISPLAY (shell);
}

static void
color_display_down_callback (GtkWidget *widget,
			     gpointer   data)
{
  ColorDisplayDialog *cdd   = data;
  GimpDisplayShell   *shell = cdd->shell;
  ColorDisplayNode   *node;

  if (cdd->dest_row < 0)
    return;

  if (cdd->dest_row >= GTK_CLIST (cdd->dest)->rows -1)
    return;

  node = gtk_clist_get_row_data (GTK_CLIST (cdd->dest), cdd->dest_row);
  gtk_clist_row_move (GTK_CLIST (cdd->dest), cdd->dest_row, cdd->dest_row + 1);

  gimp_display_shell_filter_reorder_down (shell, node);
  cdd->modified = TRUE;

  cdd->dest_row++;

  color_display_update_up_and_down (cdd);

  UPDATE_DISPLAY (shell);
}

static void
color_display_configure_callback (GtkWidget *widget,
				  gpointer   data)
{
  ColorDisplayDialog *cdd   = data;
  GimpDisplayShell   *shell = cdd->shell;
  ColorDisplayNode   *node;

  if (cdd->dest_row < 0)
    return;

  node = gtk_clist_get_row_data (GTK_CLIST (cdd->dest), cdd->dest_row);

  if (! g_list_find (cdd->conf_nodes, node))
    cdd->conf_nodes = g_list_append (cdd->conf_nodes, node);

  gimp_display_shell_filter_configure (node, NULL, NULL, NULL, NULL);

  cdd->modified = TRUE;

  UPDATE_DISPLAY (shell);
}

void
gimp_display_shell_filter_dialog_new (GimpDisplayShell *shell)
{
  ColorDisplayDialog *cdd;

  cdd = g_new0 (ColorDisplayDialog, 1);

  make_dialog (cdd);

  gtk_clist_clear (GTK_CLIST (cdd->src));
  gtk_clist_clear (GTK_CLIST (cdd->dest));

  color_display_foreach (src_list_populate, cdd->src);

  cdd->old_nodes = shell->cd_list;
  dest_list_populate (shell->cd_list, cdd->dest);
  shell->cd_list = g_list_copy (cdd->old_nodes);

  cdd->shell = shell;

  cdd->src_row = -1;
  cdd->dest_row = -1;

  shell->cd_ui = cdd->dialog;
}

static void
src_list_populate (const char *name,
		   gpointer    data)
{
  gtk_clist_append (GTK_CLIST (data), (gchar **) &name);
}

static void
dest_list_populate (GList     *node_list,
    		    GtkWidget *dest)
{
  ColorDisplayNode *node;
  int               row;

  while (node_list)
    {
      node = node_list->data;

      row = gtk_clist_append (GTK_CLIST (dest), &node->cd_name);
      gtk_clist_set_row_data (GTK_CLIST (dest), row, node);

      node_list = node_list->next;
    }
}

static void
select_src (GtkWidget       *widget,
	    gint             row,
	    gint             column,
	    GdkEventButton  *event,
	    gpointer         data)
{
  ColorDisplayDialog *cdd = data;

  cdd->src_row = row;

  gtk_widget_set_sensitive (cdd->buttons[BUTTON_ADD], TRUE);
}

static void
unselect_src (GtkWidget      *widget,
              gint            row,
              gint            column,
              GdkEventButton *event,
              gpointer        data)
{
  ColorDisplayDialog *cdd = data;

  cdd->src_row = -1;

  gtk_widget_set_sensitive (cdd->buttons[BUTTON_ADD], FALSE);
}

static void
select_dest (GtkWidget       *widget,
	     gint             row,
	     gint             column,
	     GdkEventButton  *event,
	     gpointer         data)
{
  ColorDisplayDialog *cdd = data;

  cdd->dest_row = row;

  gtk_widget_set_sensitive (cdd->buttons[BUTTON_REMOVE],    TRUE);
  gtk_widget_set_sensitive (cdd->buttons[BUTTON_CONFIGURE], TRUE);

  color_display_update_up_and_down (cdd);
}

static void
unselect_dest (GtkWidget      *widget,
               gint            row,
               gint            column,
               GdkEventButton *event,
               gpointer        data)
{
  ColorDisplayDialog *cdd = data;

  cdd->dest_row = -1;

  gtk_widget_set_sensitive (cdd->buttons[BUTTON_REMOVE],    FALSE);
  gtk_widget_set_sensitive (cdd->buttons[BUTTON_CONFIGURE], FALSE);

  color_display_update_up_and_down (cdd);
}

