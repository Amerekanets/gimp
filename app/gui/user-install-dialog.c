/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 * Copyright (C) 2000 Michael Natterer and Sven Neumann
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gtk/gtk.h>

#include "libgimpmath/gimpmath.h"
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "gui-types.h"

#include "config/gimprc.h"

#include "widgets/gimppropwidgets.h"

#include "gui.h"
#include "resolution-calibrate-dialog.h"
#include "user-install-dialog.h"

#include "libgimp/gimpintl.h"

#ifdef G_OS_WIN32
#  include <io.h>
#  define mkdir(path, mode) _mkdir(path)
#endif


#define PAGE_STYLE(widget)  gtk_widget_modify_style (widget, page_style)
#define TITLE_STYLE(widget) gtk_widget_modify_style (widget, title_style)

enum
{
  GPL_PAGE,
  TREE_PAGE,
  LOG_PAGE,
  TUNING_PAGE,
  RESOLUTION_PAGE,
  EEK_PAGE,
  NUM_PAGES
};

enum
{
  DIRENT_COLUMN,
  PIXBUF_COLUMN,
  DESC_COLUMN,
  NUM_COLUMNS
};


static void     user_install_continue_callback (GtkWidget *widget,
                                                GimpRc    *gimprc);
static void     user_install_cancel_callback   (GtkWidget *widget,
                                                gpointer   data);

static gboolean user_install_run               (void);
static void     user_install_tuning            (GimpRc    *gimprc);
static void     user_install_resolution        (GimpRc    *gimprc);


/*  private stuff  */

static GtkWidget  *user_install_dialog = NULL;

static GtkWidget  *notebook        = NULL;

static GtkWidget  *title_image     = NULL;

static GtkWidget  *title_label     = NULL;
static GtkWidget  *footer_label    = NULL;

static GtkWidget  *log_page        = NULL;
static GtkWidget  *tuning_page     = NULL;
static GtkWidget  *resolution_page = NULL;

static GtkWidget  *continue_button = NULL;
static GtkWidget  *cancel_button   = NULL;

static GtkRcStyle *title_style     = NULL;
static GtkRcStyle *page_style      = NULL;

static GdkColor    black_color;
static GdkColor    white_color;
static GdkColor    title_color;

typedef enum
{
  TREE_ITEM_DONT,              /* Don't pre-create            */
  TREE_ITEM_MKDIR_ONLY,        /* Just mkdir                  */
  TREE_ITEM_FROM_SYSCONF_DIR,  /* Copy from sysconf directory */
  TREE_ITEM_FROM_DATA_DIR      /* ... from data directory     */
} TreeItemType;

static struct
{
  gboolean      directory;
  gchar        *text;
  gchar        *description;
  TreeItemType  type;
  gchar        *source_filename;  /* If NULL, use text */
}
tree_items[] =
{
  {
    FALSE, "gimprc",
    N_("The gimprc is used to store personal preferences\n"
       "that affect GIMP's default behavior.\n"
       "Paths to search for brushes, palettes, gradients,\n"
       "patterns, plug-ins and modules can also configured\n"
       "here."),
    TREE_ITEM_DONT, NULL
  },
  {
    FALSE, "gtkrc",
    N_("GIMP uses an additional gtkrc file so you can\n"
       "configure it to look differently than other GTK apps."),
    TREE_ITEM_FROM_SYSCONF_DIR, "gtkrc_user"
  },
  {
    FALSE, "pluginrc",
    N_("Plug-ins and extensions are external programs run\n"
       "by the GIMP which provide additional functionality.\n"
       "These programs are searched for at run-time and\n"
       "information about their functionality and mod-times\n"
       "is cached in this file.  This file is intended to\n"
       "be GIMP-readable only, and should not be edited."),
    TREE_ITEM_DONT, NULL
  },
  {
    FALSE, "menurc",
    N_("Key shortcuts can be dynamically redefined in The GIMP.\n"
       "The menurc is a dump of your configuration so it can.\n"
       "be remembered for the next session.  You may edit this\n"
       "file if you wish, but it is much easier to define the\n"
       "keys from within The GIMP.  Deleting this file will\n"
       "restore the default shortcuts."),
    TREE_ITEM_DONT, NULL
  },
  {
    FALSE, "sessionrc",
    N_("The sessionrc is used to store what dialog windows were\n"
       "open the last time you quit The GIMP.  You can configure\n"
       "The GIMP to reopen these dialogs at the saved position."),
    TREE_ITEM_DONT, NULL
  },
  {
    FALSE, "unitrc",
    N_("The unitrc is used to store your user units database.\n"
       "You can define additional units and use them just\n"
       "like you use the built-in units inches, millimeters,\n"
       "points and picas.  This file is overwritten each time\n"
       "you quit the GIMP."),
    TREE_ITEM_FROM_SYSCONF_DIR, NULL
  },
  {
    TRUE, "themes",
    N_("This folder is searched for user-installed themes."),
    TREE_ITEM_MKDIR_ONLY, NULL
  },
  {
    TRUE, "brushes",
    N_("This folder is used to store user defined brushes.\n"
       "The GIMP checks this folder in addition to the system-\n"
       "wide GIMP brushes installation when searching for\n"
       "brushes."),
    TREE_ITEM_MKDIR_ONLY, NULL
  },
  {
    TRUE, "gradients",
    N_("This folder is used to store user defined gradients\n"
       "The GIMP checks this folder in addition to the system-\n"
       "wide GIMP gradients installation when searching for\n"
       "gradients."),
    TREE_ITEM_MKDIR_ONLY, NULL
  },
  {
    TRUE, "palettes",
    N_("This folder is used to store user defined palettes.\n"
       "The GIMP checks this folder in addition to the system-\n"
       "wide GIMP palettes installation when searching for\n"
       "palettes."),
    TREE_ITEM_MKDIR_ONLY, NULL
  },
  {
    TRUE, "patterns",
    N_("This folder is used to store user defined patterns.\n"
       "The GIMP checks this folder in addition to the system-\n"
       "wide GIMP patterns installation when searching for\n"
       "patterns."),
    TREE_ITEM_MKDIR_ONLY, NULL
  },
  {
    TRUE, "plug-ins",
    N_("This folder is used to store user created, temporary,\n"
       "or otherwise non-system-supported plug-ins.  The GIMP\n"
       "checks this folder in addition to the system-wide\n"
       "GIMP plug-in folder when searching for plug-ins."),
    TREE_ITEM_MKDIR_ONLY, NULL
  },
  {
    TRUE, "modules",
    N_("This folder is used to store user created, temporary,\n"
       "or otherwise non-system-supported DLL modules.  The\n"
       "GIMP checks this folder in addition to the system-wide\n"
       "GIMP module folder when searching for modules to load\n"
       "during initialization."),
    TREE_ITEM_MKDIR_ONLY, NULL
  },
  {
    TRUE, "environ",
    N_("This folder is used to store user created, temporary,\n"
       "or otherwise non-system-supported additions to the\n"
       "plug-in environment.  The GIMP checks this folder in\n"
       "addition to the system-wide GIMP environment folder\n"
       "when searching for plug-in environment modification\n"
       "files."),
    TREE_ITEM_MKDIR_ONLY, NULL
  },
  {
    TRUE, "scripts",
    N_("This folder is used to store user created and installed\n"
       "scripts.  The GIMP checks this folder in addition to\n"
       "the systemwide GIMP scripts folder when searching for\n"
       "scripts."),
    TREE_ITEM_MKDIR_ONLY, NULL
  },
  {
    TRUE, "tmp",
    N_("This folder is used to temporarily store undo buffers\n"
       "to reduce memory usage.  If The GIMP is unceremoniously\n"
       "killed, files of the form: gimp<#>.<#> may persist in\n"
       "this folder.  These files are useless across GIMP\n"
       "sessions and can be destroyed with impunity."),
    TREE_ITEM_MKDIR_ONLY, NULL
  },
  {
    TRUE, "curves",
    N_("This folder is used to store parameter files for the\n"
       "Curves tool."),
    TREE_ITEM_MKDIR_ONLY, NULL
  },
  {
    TRUE, "levels",
    N_("This folder is used to store parameter files for the\n"
       "Levels tool."),
    TREE_ITEM_MKDIR_ONLY, NULL
  },
  {
    TRUE, "fractalexplorer",
    N_("This is folder used to store user defined fractals to\n"
       "be used by the FractalExplorer plug-in.  The GIMP\n"
       "checks this folder in addition to the systemwide\n"
       "FractalExplorer installation when searching for fractals."),
    TREE_ITEM_MKDIR_ONLY, NULL
  },  
  {
    TRUE, "gfig",
    N_("This folder is used to store user defined figures to\n"
       "be used by the GFig plug-in.  The GIMP checks this\n"
       "folder in addition to the systemwide GFig installation\n"
       "when searching for gfig figures."),
    TREE_ITEM_MKDIR_ONLY, NULL
  },
  {
    TRUE, "gflare",
    N_("This folder is used to store user defined gflares to\n"
       "be used by the GFlare plug-in.  The GIMP checks this\n"
       "folder in addition to the systemwide GFlares\n"
       "installation when searching for gflares."),
    TREE_ITEM_MKDIR_ONLY, NULL
  },
  {
    TRUE, "gimpressionist",
    N_("This folder is used to store user defined data to be\n"
       "used by the Gimpressionist plug-in.  The GIMP checks\n"
       "this folder in addition to the systemwide Gimpressionist\n"
       "installation when searching for data."),
    TREE_ITEM_MKDIR_ONLY, NULL
  }  
};


static void
user_install_notebook_set_page (GtkNotebook *notebook,
				gint         index)
{
  GtkWidget *page;
  gchar     *title;
  gchar     *footer;

  page = gtk_notebook_get_nth_page (notebook, index);

  title  = g_object_get_data (G_OBJECT (page), "title");
  footer = g_object_get_data (G_OBJECT (page), "footer");

  gtk_label_set_text (GTK_LABEL (title_label), title);
  gtk_label_set_text (GTK_LABEL (footer_label), footer);

  if (index == EEK_PAGE && title_image)
    {
      GdkPixbuf *wilber;
      gchar     *filename;

      filename = g_build_filename (gimp_data_directory(),
                                   "images", "wilber-eek.png", NULL);
      wilber = gdk_pixbuf_new_from_file (filename, NULL);
      g_free (filename);

      if (wilber)
        {
          gtk_image_set_from_pixbuf (GTK_IMAGE (title_image), wilber);
          g_object_unref (wilber);
        }
    }
  
  gtk_notebook_set_current_page (notebook, index);
}

static void
user_install_continue_callback (GtkWidget *widget,
				GimpRc    *gimprc)
{
  static gint notebook_index = GPL_PAGE;

  switch (notebook_index)
    {
    case GPL_PAGE:
      user_install_notebook_set_page (GTK_NOTEBOOK (notebook), ++notebook_index);
      break;

    case TREE_PAGE:
      user_install_notebook_set_page (GTK_NOTEBOOK (notebook), ++notebook_index);

      /*  Creating the directories can take some time on NFS, so inform
       *  the user and set the buttons insensitive
       */
      gtk_widget_set_sensitive (continue_button, FALSE);
      gtk_widget_set_sensitive (cancel_button, FALSE);

      if (user_install_run ())
        {
          gtk_widget_set_sensitive (continue_button, TRUE);
          gtk_label_set_text (GTK_LABEL (footer_label),
                              _("Installation successful.\n"
                                "Click \"Continue\" to proceed."));
        }
      else
        {
          gtk_label_set_text (GTK_LABEL (footer_label),
                              _("Installation failed.\n"
                                "Contact system administrator."));
        }

      gtk_widget_set_sensitive (cancel_button, TRUE);
      break;

    case LOG_PAGE:
      user_install_notebook_set_page (GTK_NOTEBOOK (notebook), ++notebook_index);
      user_install_tuning (gimprc);
      break;

    case TUNING_PAGE:
      user_install_notebook_set_page (GTK_NOTEBOOK (notebook), ++notebook_index);
      user_install_resolution (gimprc);
      break;

    case RESOLUTION_PAGE:
      gimp_rc_save (gimprc);
      
      g_object_unref (G_OBJECT (title_style));
      g_object_unref (G_OBJECT (page_style));

      gtk_widget_destroy (user_install_dialog);

      gtk_main_quit ();
      return;
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
user_install_cancel_callback (GtkWidget *widget,
			      gpointer   data)
{
  static guint timeout_id = 0;

  if (timeout_id)
    exit (0);

  gtk_widget_destroy (continue_button);
  user_install_notebook_set_page (GTK_NOTEBOOK (notebook), EEK_PAGE);
  timeout_id = g_timeout_add (1024, (GSourceFunc) exit, NULL);
}

static gboolean
user_install_corner_expose (GtkWidget      *widget,
			    GdkEventExpose *eevent,
			    gpointer        data)
{
  GtkCornerType corner;

  /* call default handler explicitly, then draw the corners */
  if (GTK_WIDGET_GET_CLASS (widget)->expose_event)
    GTK_WIDGET_GET_CLASS (widget)->expose_event (widget, eevent);

  corner = GPOINTER_TO_INT (data);

  switch (corner)
    {
    case GTK_CORNER_TOP_LEFT:
      gdk_draw_arc (widget->window,
		    widget->style->white_gc,
		    TRUE,
		    0, 0,
		    widget->allocation.width * 2,
		    widget->allocation.height * 2,
		    90 * 64,
		    180 * 64);
      break;

    case GTK_CORNER_BOTTOM_LEFT:
      gdk_draw_arc (widget->window,
		    widget->style->white_gc,
		    TRUE,
		    0, -widget->allocation.height,
		    widget->allocation.width * 2,
		    widget->allocation.height * 2,
		    180 * 64,
		    270 * 64);
      break;

    case GTK_CORNER_TOP_RIGHT:
      gdk_draw_arc (widget->window,
		    widget->style->white_gc,
		    TRUE,
		    -widget->allocation.width, 0,
		    widget->allocation.width * 2,
		    widget->allocation.height * 2,
		    0 * 64,
		    90 * 64);
      break;

    case GTK_CORNER_BOTTOM_RIGHT:
      gdk_draw_arc (widget->window,
		    widget->style->white_gc,
		    TRUE,
		    -widget->allocation.width, -widget->allocation.height,
		    widget->allocation.width * 2,
		    widget->allocation.height * 2,
		    270 * 64,
		    360 * 64);
      break;

    default:
      break;
    }

  return TRUE;
}

static GtkWidget *
user_install_notebook_append_page (GtkNotebook *notebook,
				   gchar       *title,
				   gchar       *footer)
{
  GtkWidget *page;

  page = gtk_vbox_new (FALSE, 6);
  g_object_set_data (G_OBJECT (page), "title", title);
  g_object_set_data (G_OBJECT (page), "footer", footer);
  gtk_notebook_append_page (notebook, page, NULL);
  gtk_widget_show (page);

  return page;
}

static void
add_label (GtkBox   *box,
	   gchar    *text)
{
  GtkWidget *label;

  label = gtk_label_new (text);
  PAGE_STYLE (label);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_box_pack_start (box, label, FALSE, FALSE, 0);

  gtk_widget_show (label);
}

static void
user_install_sel_changed (GtkTreeSelection *sel,
			  gpointer          data)
{
  GtkNotebook  *notebook = GTK_NOTEBOOK (data);
  GtkTreeModel *model;
  GtkTreeIter   iter;
  gint          index = 0;

  if (gtk_tree_selection_get_selected (sel, &model, &iter))
    gtk_tree_model_get (model, &iter, DESC_COLUMN, &index, -1);

  gtk_notebook_set_current_page (notebook, index);
}

static void
user_install_tv_fix_size_request (GtkWidget     *widget,
				  GtkAllocation *allocation)
{
  gtk_widget_set_size_request (widget, allocation->width, allocation->height);
  g_signal_handlers_disconnect_by_func (widget,
					user_install_tv_fix_size_request,
					NULL);
}

void
user_install_dialog_create (const gchar *alternate_system_gimprc,
                            const gchar *alternate_gimprc,
                            gboolean     verbose)
{
  GimpRc    *gimprc;
  GtkWidget *dialog;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *ebox;
  GtkWidget *table;
  GtkWidget *darea;
  GtkWidget *page;
  GtkWidget *sep;
  GtkWidget *eek_box;
  GdkPixbuf *wilber;
  gchar     *filename;

  gimprc = gimp_rc_new (alternate_system_gimprc, alternate_gimprc, verbose);

  dialog = user_install_dialog =
    gimp_dialog_new (_("GIMP User Installation"), "user_installation",
		     NULL, NULL,
		     GTK_WIN_POS_CENTER,
		     FALSE, FALSE, FALSE,

		     GTK_STOCK_CANCEL, user_install_cancel_callback,
		     NULL, 1, &cancel_button, FALSE, TRUE,

		     _("Continue"), user_install_continue_callback,
		     gimprc, NULL, &continue_button, TRUE, FALSE,

		     NULL);

  g_object_weak_ref (G_OBJECT (dialog),
                     (GWeakNotify) g_object_unref, gimprc);

  gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->action_area), 8);

  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

  eek_box = gtk_hbox_new (FALSE, 8);

  g_object_ref (GTK_DIALOG (dialog)->action_area);
  gtk_container_remove (GTK_CONTAINER (GTK_DIALOG (dialog)->action_area->parent),
                        GTK_DIALOG (dialog)->action_area);
  gtk_box_pack_end (GTK_BOX (eek_box), GTK_DIALOG (dialog)->action_area,
                    FALSE, FALSE, 0);
  g_object_unref (GTK_DIALOG (dialog)->action_area);

  gtk_box_pack_end (GTK_BOX (GTK_DIALOG (dialog)->vbox), eek_box,
                    FALSE, FALSE, 0);
  gtk_widget_show (eek_box);

  gdk_color_parse ("black",       &black_color);
  gdk_color_parse ("white",       &white_color);
  gdk_color_parse ("dark orange", &title_color);

  gtk_widget_realize (dialog);

  /*  B/W Style for the page contents  */
  page_style = gtk_widget_get_modifier_style (dialog);
  g_object_ref (page_style);

  page_style->fg[GTK_STATE_NORMAL]   = black_color;
  page_style->bg[GTK_STATE_NORMAL]   = white_color;

  page_style->text[GTK_STATE_NORMAL] = black_color;
  page_style->base[GTK_STATE_NORMAL] = white_color;

  page_style->color_flags[GTK_STATE_NORMAL] |= (GTK_RC_FG   | GTK_RC_BG   |
						GTK_RC_TEXT | GTK_RC_BASE);

  page_style->fg[GTK_STATE_ACTIVE]   = black_color;
  page_style->bg[GTK_STATE_ACTIVE]   = white_color;

  page_style->color_flags[GTK_STATE_ACTIVE] |= (GTK_RC_FG | GTK_RC_BG);


  /*  B/Colored Style for the page title  */
  title_style = gtk_rc_style_copy (page_style);

  title_style->bg[GTK_STATE_NORMAL] = title_color;

  pango_font_description_free (title_style->font_desc);
  title_style->font_desc = pango_font_description_from_string ("sans 20");

  TITLE_STYLE (dialog);

  footer_label = gtk_label_new (NULL);
  PAGE_STYLE (footer_label);
  gtk_label_set_justify (GTK_LABEL (footer_label), GTK_JUSTIFY_RIGHT);
  gtk_box_pack_end (GTK_BOX (eek_box), footer_label, FALSE, FALSE,0 );
  gtk_widget_show (footer_label);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), vbox);

  ebox = gtk_event_box_new ();
  TITLE_STYLE (ebox);
  gtk_widget_set_events (ebox, GDK_EXPOSURE_MASK);
  gtk_box_pack_start (GTK_BOX (vbox), ebox, FALSE, FALSE, 0);
  gtk_widget_show (ebox);

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
  gtk_container_add (GTK_CONTAINER (ebox), hbox);
  gtk_widget_show (hbox);

  filename = g_build_filename (gimp_data_directory(),
                               "images", "wilber-wizard.png", NULL);
  wilber = gdk_pixbuf_new_from_file (filename, NULL);
  g_free (filename);

  if (wilber)
    {
      title_image = gtk_image_new_from_pixbuf (wilber);
      g_object_unref (wilber);

      gtk_box_pack_start (GTK_BOX (hbox), title_image, FALSE, FALSE, 8);
      gtk_widget_show (title_image);
    }

  title_label = gtk_label_new (NULL);
  TITLE_STYLE (title_label);  
  gtk_label_set_justify (GTK_LABEL (title_label), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (hbox), title_label, FALSE, FALSE, 0);
  gtk_widget_show (title_label);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  ebox = gtk_event_box_new ();
  TITLE_STYLE (ebox);  
  gtk_widget_set_size_request (ebox, 16, -1);
  gtk_box_pack_start (GTK_BOX (hbox), ebox, FALSE, FALSE, 0);
  gtk_widget_show (ebox);

  ebox = gtk_event_box_new ();
  PAGE_STYLE (ebox);  
  gtk_box_pack_start (GTK_BOX (hbox), ebox, TRUE, TRUE, 0);
  gtk_widget_show (ebox);

  table = gtk_table_new (3, 3, FALSE);
  gtk_table_set_col_spacing (GTK_TABLE (table), 1, 8);
  gtk_container_add (GTK_CONTAINER (ebox), table);
  gtk_widget_show (table);

  darea = gtk_drawing_area_new ();
  TITLE_STYLE (darea);
  gtk_widget_set_size_request (darea, 16, 16);
  g_signal_connect (G_OBJECT (darea), "expose_event",
                    G_CALLBACK (user_install_corner_expose),
                    GINT_TO_POINTER (GTK_CORNER_TOP_LEFT));
  gtk_table_attach (GTK_TABLE (table), darea, 0, 1, 0, 1,
		    GTK_SHRINK, GTK_SHRINK, 0, 0);
  gtk_widget_show (darea);

  darea = gtk_drawing_area_new ();
  TITLE_STYLE (darea);
  gtk_widget_set_size_request (darea, 16, 16);
  g_signal_connect (G_OBJECT (darea), "expose_event",
                    G_CALLBACK (user_install_corner_expose),
                    GINT_TO_POINTER (GTK_CORNER_BOTTOM_LEFT));
  gtk_table_attach (GTK_TABLE (table), darea, 0, 1, 2, 3,
		    GTK_SHRINK, GTK_SHRINK, 0, 0);
  gtk_widget_show (darea);

  notebook = gtk_notebook_new ();
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
  gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);
  gtk_table_attach_defaults (GTK_TABLE (table), notebook, 1, 2, 1, 2);
  gtk_widget_show (notebook);

  gtk_widget_show (vbox);

  /*  Page 1  */
  page = user_install_notebook_append_page (GTK_NOTEBOOK (notebook),
					    _("Welcome to\n"
					      "The GIMP User Installation"),
					    _("Click \"Continue\" to enter "
					      "the GIMP user installation."));

  add_label (GTK_BOX (page),
	     _("The GIMP - GNU Image Manipulation Program\n"
	       "Copyright (C) 1995-2002\n"
	       "Spencer Kimball, Peter Mattis and the GIMP Development Team."));

  sep = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (page), sep, FALSE, FALSE, 2);
  gtk_widget_show (sep);

  add_label 
    (GTK_BOX (page),
     _("This program is free software; you can redistribute it and/or modify\n"
       "it under the terms of the GNU General Public License as published by\n"
       "the Free Software Foundation; either version 2 of the License, or\n"
       "(at your option) any later version."));
  add_label
    (GTK_BOX (page),
     _("This program is distributed in the hope that it will be useful,\n"
       "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
       "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"
       "See the GNU General Public License for more details."));
  add_label
    (GTK_BOX (page),
     _("You should have received a copy of the GNU General Public License\n"
       "along with this program; if not, write to the Free Software\n"
       "Foundation, Inc., 59 Temple Place - Suite 330, Boston,\n"
       "MA 02111-1307, USA."));

  /*  Page 2  */
  {
    GtkWidget         *hbox;
    GtkWidget         *vbox;
    GtkWidget         *tv;
    GdkPixbuf         *file_pixbuf;
    GdkPixbuf         *folder_pixbuf;
    GtkWidget         *notebook2;
    GtkWidget         *page2;
    GtkWidget         *label;
    gchar             *str;
    GtkTreeStore      *tree;
    GtkTreeViewColumn *column;
    GtkCellRenderer   *cell;
    GtkTreeSelection  *sel;
    GtkTreeIter        iter, child;
    gint               i;

    page = user_install_notebook_append_page (GTK_NOTEBOOK (notebook),
					      _("Personal GIMP Folder"),
					      _("Click \"Continue\" to create "
						"your personal GIMP folder."));

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_box_pack_start (GTK_BOX (page), hbox, FALSE, FALSE, 0);
    gtk_widget_show (hbox);

    tree = gtk_tree_store_new (NUM_COLUMNS, G_TYPE_STRING, GDK_TYPE_PIXBUF,
					    G_TYPE_INT);
    tv = gtk_tree_view_new_with_model (GTK_TREE_MODEL (tree));
    g_object_unref (tree);

    column = gtk_tree_view_column_new ();

    cell = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, cell, FALSE);
    gtk_tree_view_column_set_attributes (column, cell,
					 "pixbuf", PIXBUF_COLUMN,
					 NULL);

    cell = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, cell, TRUE);
    gtk_tree_view_column_set_attributes (column, cell,
					 "text", DIRENT_COLUMN,
					 NULL);

    gtk_tree_view_append_column (GTK_TREE_VIEW (tv), column);

    PAGE_STYLE (tv);
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tv), FALSE);
    gtk_box_pack_start (GTK_BOX (hbox), tv, FALSE, FALSE, 0);
    gtk_widget_show (tv);

    vbox = gtk_vbox_new (FALSE, 6);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
    gtk_widget_show (vbox);

    str = g_strdup_printf (_("For a proper GIMP installation, a folder named\n"
			     "'%s' needs to be created."), gimp_directory ());
    add_label (GTK_BOX (vbox), str);
    g_free (str);

    add_label (GTK_BOX (vbox),
	       _("This folder will contain a number of important files.\n"
		 "Click on one of the files or folders in the tree\n"
		 "to get more information about the selected item."));

    notebook2 = gtk_notebook_new ();
    gtk_container_set_border_width (GTK_CONTAINER (notebook2), 8);
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook2), FALSE);
    gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook2), FALSE);
    gtk_box_pack_start (GTK_BOX (vbox), notebook2, TRUE, TRUE, 0);
    gtk_widget_show (notebook2);

    /*  empty page  */
    page2 = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (page2);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook2), page2, NULL);

    sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (tv));
    g_signal_connect (G_OBJECT (sel), "changed",
		      G_CALLBACK (user_install_sel_changed),
		      notebook2);

    file_pixbuf = gtk_widget_render_icon (tv,
                                          GTK_STOCK_NEW, GTK_ICON_SIZE_MENU,
                                          NULL);
    folder_pixbuf = gtk_widget_render_icon (tv,
                                            GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU,
                                            NULL);

    gtk_tree_store_append (tree, &iter, NULL);
    gtk_tree_store_set (tree, &iter,
			DIRENT_COLUMN, gimp_directory (),
			PIXBUF_COLUMN, folder_pixbuf,
			DESC_COLUMN, 0,
			-1);

    for (i = 0; i < G_N_ELEMENTS (tree_items); i++)
      {
	gtk_tree_store_append (tree, &child, &iter);
	gtk_tree_store_set (tree, &child,
			    DIRENT_COLUMN, tree_items[i].text,
			    PIXBUF_COLUMN,
			    tree_items[i].directory ? folder_pixbuf
						    : file_pixbuf,
			    DESC_COLUMN, i + 1,
			    -1);

	page2 = gtk_vbox_new (FALSE, 0);
	label = gtk_label_new (gettext (tree_items[i].description));
	PAGE_STYLE (label);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (page2), label, TRUE, TRUE, 0);
	gtk_widget_show (label);
	gtk_widget_show (page2);

	gtk_notebook_append_page (GTK_NOTEBOOK (notebook2), page2, NULL);
      }

    gtk_tree_view_expand_all (GTK_TREE_VIEW (tv));

    g_signal_connect (tv, "size_allocate",
		      G_CALLBACK (user_install_tv_fix_size_request),
		      NULL);

    g_object_unref (file_pixbuf);
    g_object_unref (folder_pixbuf);
  }
  
  /*  Page 3  */
  page = log_page =
    user_install_notebook_append_page (GTK_NOTEBOOK (notebook),
				       _("User Installation Log"),
                                       _("Please wait while your personal\n"
                                         "GIMP folder is being created..."));

  /*  Page 4  */
  page = tuning_page = 
    user_install_notebook_append_page (GTK_NOTEBOOK (notebook),
				       _("GIMP Performance Tuning"),
				       _("Click \"Continue\" to accept the settings above."));
  
  add_label (GTK_BOX (page),
	     _("For optimal GIMP performance, some settings may have to be adjusted."));

  sep = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (page), sep, FALSE, FALSE, 2);
  gtk_widget_show (sep);

  /*  Page 5  */
  page = resolution_page = 
    user_install_notebook_append_page (GTK_NOTEBOOK (notebook),
				       _("Monitor Resolution"),
				       _("Click \"Continue\" to start The GIMP."));

  add_label (GTK_BOX (resolution_page),
	     _("To display images in their natural size, "
	       "GIMP needs to know your monitor resolution."));

  sep = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (page), sep, FALSE, FALSE, 2);
  gtk_widget_show (sep);

  /*  EEK page  */
  page = user_install_notebook_append_page (GTK_NOTEBOOK (notebook),
					    _("Aborting Installation..."),
					    NULL);

  user_install_notebook_set_page (GTK_NOTEBOOK (notebook), 0);

  gtk_widget_grab_focus (continue_button);
  gtk_widget_grab_default (continue_button);

  gtk_widget_show (dialog);
}


/*********************/
/*  Local functions  */

static gboolean
copy_file (gchar   *source,
           gchar   *dest,
           GError **error)
{
  gchar  buffer[4096];
  FILE  *sfile, *dfile;
  gint   nbytes;

  sfile = fopen (source, "rb");
  if (sfile == NULL)
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                   _("Cannot open '%s' for reading: %s"),
                   source, g_strerror (errno));
      return FALSE;
    }

  dfile = fopen (dest, "wb");
  if (dfile == NULL)
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                   _("Cannot open '%s' for writing: %s"),
                   dest, g_strerror (errno));
      fclose (sfile);
      return FALSE;
    }

  while ((nbytes = fread (buffer, 1, sizeof (buffer), sfile)) > 0)
    {
      if (fwrite (buffer, 1, nbytes, dfile) < nbytes)
	{
	  g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                       _("Error while writing '%s': %s"),
                       dest, g_strerror (errno));
	  fclose (sfile);
	  fclose (dfile);
	  return FALSE;
	}
    }

  if (ferror (sfile))
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                   _("Error while reading '%s': %s"),
                   source, g_strerror (errno));
      fclose (sfile);
      fclose (dfile);
      return FALSE;
    }

  fclose (sfile);

  if (fclose (dfile) == EOF)
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                   _("Error while writing '%s': %s"),
                   dest, g_strerror (errno));
      return FALSE;
    }

  return TRUE;
}

static gboolean
user_install_run (void)
{
  GtkWidget     *scrolled_window;
  GtkTextBuffer *log_buffer;
  GtkWidget     *log_view;
  GError        *error = NULL;
  gchar          dest[1000];
  gchar          source[1000];
  gchar          log_line[1000];
  gint           i;

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (log_page), scrolled_window, TRUE, TRUE, 0);
  gtk_widget_show (scrolled_window);

  log_buffer = gtk_text_buffer_new (NULL);

  log_view = gtk_text_view_new_with_buffer (log_buffer);
  g_object_unref (log_buffer);

  PAGE_STYLE (log_view);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (log_view), FALSE);

  gtk_container_add (GTK_CONTAINER (scrolled_window), log_view);
  gtk_widget_show (log_view);

  g_snprintf (log_line, sizeof (log_line), _("Creating folder '%s'\n"),
              gimp_directory ());
  gtk_text_buffer_insert_at_cursor (log_buffer, log_line, -1);

  while (gtk_events_pending ())
    gtk_main_iteration ();

  if (mkdir (gimp_directory (), 0755) == -1)
    {
      g_set_error (&error, G_FILE_ERROR, g_file_error_from_errno (errno),
                   _("Cannot create folder '%s': %s"),
                   gimp_directory (), g_strerror (errno));
      goto break_out_of_loop;
    }
  gtk_text_buffer_insert_at_cursor (log_buffer, _("  Success\n"), -1);

  while (gtk_events_pending ())
    gtk_main_iteration ();

  for (i = 0; i < G_N_ELEMENTS (tree_items); i++)
    {
      g_snprintf (dest, sizeof (dest), "%s%c%s",
		  gimp_directory (), G_DIR_SEPARATOR, tree_items[i].text);

      switch (tree_items[i].type)
	{
	case TREE_ITEM_DONT:
	  break;

	case TREE_ITEM_MKDIR_ONLY:
	  g_snprintf (log_line, sizeof (log_line), _("Creating folder '%s'\n"),
		      dest);
	  gtk_text_buffer_insert_at_cursor (log_buffer, log_line, -1);

          while (gtk_events_pending ())
            gtk_main_iteration ();

	  if (mkdir (dest,
                     S_IRUSR | S_IWUSR | S_IXUSR |
                     S_IRGRP | S_IXGRP |
                     S_IROTH | S_IXOTH) == -1)
	    {
              g_set_error (&error,
                           G_FILE_ERROR, g_file_error_from_errno (errno),
                           _("Cannot create folder: %s"), g_strerror (errno));
	      goto break_out_of_loop;
	    }
	  break;

	case TREE_ITEM_FROM_SYSCONF_DIR:
	  g_snprintf (source, sizeof (source), "%s%c%s",
		      gimp_sysconf_directory (), G_DIR_SEPARATOR,
		      tree_items[i].source_filename ?
		      tree_items[i].source_filename : tree_items[i].text);
	  goto do_copy;

	case TREE_ITEM_FROM_DATA_DIR:
	  g_snprintf (source, sizeof (source), "%s%c%s",
		      gimp_data_directory (), G_DIR_SEPARATOR,
		      tree_items[i].source_filename ?
		      tree_items[i].source_filename : tree_items[i].text);

	do_copy:
          g_assert (! tree_items[i].directory);
	  g_snprintf (log_line, sizeof (log_line),
                      _("Copying file '%s' from '%s'\n"),
		      dest, source);
	  gtk_text_buffer_insert_at_cursor (log_buffer, log_line, -1);

          while (gtk_events_pending ())
            gtk_main_iteration ();

          if (! copy_file (source, dest, &error))
            goto break_out_of_loop;
	  break;

	default:
	  g_assert_not_reached ();
          break;
	}

      if (tree_items[i].type != TREE_ITEM_DONT)
	gtk_text_buffer_insert_at_cursor (log_buffer, _("  Success\n"), -1);
    }

 break_out_of_loop:

  if (error)
    {
      g_snprintf (log_line, sizeof (log_line), _("  Failure: %s\n"),
		  error->message);
      g_clear_error (&error);
      gtk_text_buffer_insert_at_cursor (log_buffer, log_line, -1);
      return FALSE;
    }

  return TRUE;
}

static void
user_install_tuning (GimpRc *gimprc)
{
  GtkWidget *hbox;
  GtkWidget *sep;
  GtkWidget *label;
  GtkWidget *entry;

  /*  tile cache size  */
  add_label (GTK_BOX (tuning_page),
	     _("GIMP uses a limited amount of memory to store image data, the so-called\n"
	       "\"Tile Cache\". You should adjust its size to fit into memory. Consider\n"
	       "the amount of memory used by other running processes."));

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (tuning_page), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);
  
  entry = gimp_prop_memsize_entry_new (G_OBJECT (gimprc), "tile-cache-size");
  gtk_box_pack_end (GTK_BOX (hbox), entry, FALSE, FALSE, 0);
  gtk_widget_show (entry);

  label = gtk_label_new (_("Tile Cache Size:"));
  PAGE_STYLE (label);
  gtk_box_pack_end (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);
  
  sep = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (tuning_page), sep, FALSE, FALSE, 2);
  gtk_widget_show (sep);

  /*  swap file location  */
  add_label (GTK_BOX (tuning_page),
	     _("All image and undo data which doesn't fit into the Tile Cache will be\n"
	       "written to a swap file. This file should be located on a local filesystem\n"
	       "with enough free space (several hundred MB). On a UNIX system, you\n"
	       "may want to use the system-wide temp-dir (\"/tmp\" or \"/var/tmp\")."));

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (tuning_page), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);
  
  entry = gimp_prop_file_entry_new (G_OBJECT (gimprc), "swap-path",
				    _("Select Swap Dir"),
				    TRUE, TRUE);
  gtk_box_pack_end (GTK_BOX (hbox), entry, FALSE, FALSE, 0);
  gtk_widget_show (entry);

  label = gtk_label_new (_("Swap Folder:"));
  PAGE_STYLE (label);
  gtk_box_pack_end (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);
}

static void
user_install_resolution_calibrate (GtkWidget *button,
				   gpointer   data)
{
  GdkPixbuf *pixbuf;
  gchar     *filename;

  filename = g_build_filename (gimp_data_directory (),
                               "themes", "Default", "images", "preferences",
                               "monitor.png", NULL);
  pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
  g_free (filename);

  resolution_calibrate_dialog (GTK_WIDGET (data), 
                               pixbuf,
			       title_style,
			       page_style,
			       G_CALLBACK (user_install_corner_expose));

  if (pixbuf)
    g_object_unref (pixbuf);
}

static void
user_install_resolution (GimpRc *gimprc)
{
  GimpDisplayConfig *config = GIMP_DISPLAY_CONFIG (gimprc);
  GtkWidget         *hbox;
  GtkWidget         *entry;
  GtkWidget         *sep;
  GimpChainButton   *chain;
  GtkWidget         *toggle;
  GtkWidget         *button;
  GList             *list;
  gdouble            xres, yres;
  gchar             *pixels_per_unit;
  gchar             *str;

  gui_get_screen_resolution (&xres, &yres);

  add_label (GTK_BOX (resolution_page),
	     _("GIMP can obtain this information from the windowing system.\n"
	       "However, usually this does not give useful values."));
  
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (resolution_page), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  str = g_strdup_printf
    (_("Get Resolution from windowing system (Currently %d x %d dpi)"),
     ROUND (xres), ROUND (yres));

  toggle =
    gimp_prop_check_button_new (G_OBJECT (gimprc),
				"monitor-resolution-from-windowing-system",
				str);
  g_free (str);

  PAGE_STYLE (GTK_BIN (toggle)->child);
  gtk_box_pack_end (GTK_BOX (hbox), toggle, FALSE, FALSE, 0);
  gtk_widget_show (toggle);

  sep = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (resolution_page), sep, FALSE, FALSE, 2);
  gtk_widget_show (sep);

  add_label (GTK_BOX (resolution_page),
	     _("Alternatively, you can set the monitor resolution manually."));

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (resolution_page), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);
  
  pixels_per_unit = g_strconcat (_("Pixels"), "/%s", NULL);
  
  entry = gimp_prop_coordinates_new (G_OBJECT (gimprc),
				     "monitor-xresolution",
				     "monitor-yresolution",
				     NULL,
				     pixels_per_unit,
				     GIMP_SIZE_ENTRY_UPDATE_RESOLUTION,
				     0.0, 0.0, TRUE);

  g_free (pixels_per_unit);
  
  gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (entry),
				_("Horizontal"), 0, 1, 0.0);
  gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (entry),
				_("Vertical"),   0, 2, 0.0);
  gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (entry),
				_("dpi"),        1, 4, 0.0);

  chain = GIMP_COORDINATES_CHAINBUTTON (entry);
  PAGE_STYLE (GTK_WIDGET (chain->line1));
  PAGE_STYLE (GTK_WIDGET (chain->line2));

  for (list = GTK_TABLE (entry)->children; list; list = g_list_next (list))
    {
      GtkTableChild *child = (GtkTableChild *) list->data;

      if (child && GTK_IS_LABEL (child->widget))
	PAGE_STYLE (GTK_WIDGET (child->widget));
    }

  gtk_box_pack_end (GTK_BOX (hbox), entry, FALSE, FALSE, 0);
  gtk_widget_show (entry);
  gtk_widget_set_sensitive (entry, !config->monitor_res_from_gdk);

  sep = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (resolution_page), sep, FALSE, FALSE, 2);
  gtk_widget_show (sep);

  add_label (GTK_BOX (resolution_page),
	     _("You can also press the \"Calibrate\" button to open a window\n"
	       "which lets you determine your monitor resolution interactively."));

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (resolution_page), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  button = gtk_button_new_with_label (_("Calibrate"));
  gtk_misc_set_padding (GTK_MISC (GTK_BIN (button)->child), 4, 0);
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_widget_set_sensitive (button, !config->monitor_res_from_gdk);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (user_install_resolution_calibrate),
                    entry);

  g_object_set_data (G_OBJECT (toggle), "inverse_sensitive",
                     entry);
  g_object_set_data (G_OBJECT (entry), "inverse_sensitive",
                     button);
  g_signal_connect (G_OBJECT (toggle), "toggled",
                    G_CALLBACK (gimp_toggle_button_sensitive_update),
                    NULL);
}
