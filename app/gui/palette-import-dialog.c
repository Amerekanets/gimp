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

#include <string.h>

#include <gtk/gtk.h>

#include "libgimpwidgets/gimpwidgets.h"

#include "gui-types.h"

#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimpdatafactory.h"
#include "core/gimpgradient.h"
#include "core/gimpimage.h"
#include "core/gimppalette.h"
#include "core/gimppalette-import.h"

#include "file/file-utils.h"

#include "widgets/gimpcontainermenuimpl.h"
#include "widgets/gimpdnd.h"
#include "widgets/gimppreview.h"
#include "widgets/gimpviewabledialog.h"

#include "gradient-select.h"
#include "palette-import-dialog.h"

#include "gimp-intl.h"


#define IMPORT_PREVIEW_SIZE 80


typedef enum
{
  GRADIENT_IMPORT,
  IMAGE_IMPORT
} ImportType;


typedef struct _ImportDialog ImportDialog;

struct _ImportDialog
{
  GtkWidget      *dialog;

  ImportType      import_type;
  GimpContext    *context;
  GimpPalette    *palette;

  GradientSelect *gradient_select;

  GtkWidget      *gradient_menu;
  GtkWidget      *image_menu;

  GtkWidget      *entry;
  GtkWidget      *image_radio;
  GtkWidget      *gradient_radio;
  GtkAdjustment  *threshold;
  GtkAdjustment  *num_colors;
  GtkAdjustment  *columns;

  GtkWidget      *preview;
};


static ImportDialog * palette_import_dialog_new   (Gimp          *gimp);
static void   palette_import_close_callback       (GtkWidget     *widget,
                                                   ImportDialog  *import_dialog);
static void   palette_import_import_callback      (GtkWidget     *widget,
                                                   ImportDialog  *import_dialog);
static void   palette_import_gradient_changed     (GimpContext   *context,
                                                   GimpGradient  *gradient,
                                                   ImportDialog  *import_dialog);
static void   palette_import_image_changed        (GimpContext   *context,
                                                   GimpImage     *gimage,
                                                   ImportDialog  *import_dialog);
static void   import_dialog_drop_callback         (GtkWidget     *widget,
                                                   GimpViewable  *viewable,
                                                   gpointer       data);
static void   palette_import_grad_callback        (GtkWidget     *widget,
                                                   ImportDialog  *import_dialog);
static void   palette_import_image_callback       (GtkWidget     *widget,
                                                   ImportDialog  *import_dialog);
static void   palette_import_columns_changed      (GtkAdjustment *adjustment,
                                                   ImportDialog  *import_dialog);
static void   palette_import_image_add            (GimpContainer *container,
                                                   GimpImage     *gimage,
                                                   ImportDialog  *import_dialog);
static void   palette_import_image_remove         (GimpContainer *container,
                                                   GimpImage     *gimage,
                                                   ImportDialog  *import_dialog);
static void   palette_import_make_palette         (ImportDialog  *import_dialog);


static ImportDialog *the_import_dialog = NULL;


/*  public functions  */

void
palette_import_dialog_show (Gimp *gimp)
{
  g_return_if_fail (GIMP_IS_GIMP (gimp));

  if (! the_import_dialog)
    the_import_dialog = palette_import_dialog_new (gimp);

  gtk_window_present (GTK_WINDOW (the_import_dialog->dialog));
}

void
palette_import_dialog_destroy (void)
{
  if (the_import_dialog)
    palette_import_close_callback (NULL, the_import_dialog);
}


/*  private functions  */

/*  the palette import dialog constructor  ***********************************/

static ImportDialog *
palette_import_dialog_new (Gimp *gimp)
{
  ImportDialog *import_dialog;
  GimpGradient *gradient;
  GtkWidget    *main_hbox;
  GtkWidget    *frame;
  GtkWidget    *vbox;
  GtkWidget    *table;
  GtkWidget    *abox;
  GtkWidget    *menu;
  GSList       *group;

  gradient = gimp_context_get_gradient (gimp_get_user_context (gimp));

  import_dialog = g_new0 (ImportDialog, 1);

  import_dialog->import_type = GRADIENT_IMPORT;
  import_dialog->context     = gimp_context_new (gimp, "Palette Import",
                                                 gimp_get_user_context (gimp));

  import_dialog->dialog =
    gimp_viewable_dialog_new (NULL, _("Import Palette"), "import_palette",
                              GTK_STOCK_CONVERT,
                              _("Import a New Palette"),
                              gimp_standard_help_func,
                              "dialogs/palette_editor/import_palette.html",

                              GTK_STOCK_CANCEL, palette_import_close_callback,
                              import_dialog, NULL, NULL, FALSE, TRUE,

                              _("_Import"), palette_import_import_callback,
                              import_dialog, NULL, NULL, TRUE, FALSE,

                              NULL);

  gimp_dnd_viewable_dest_add (import_dialog->dialog,
                              GIMP_TYPE_GRADIENT,
                              import_dialog_drop_callback,
                              import_dialog);
  gimp_dnd_viewable_dest_add (import_dialog->dialog,
                              GIMP_TYPE_IMAGE,
                              import_dialog_drop_callback,
                              import_dialog);

  main_hbox = gtk_hbox_new (FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER (main_hbox), 4);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (import_dialog->dialog)->vbox),
                     main_hbox);
  gtk_widget_show (main_hbox);

  vbox = gtk_vbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (main_hbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);


  /*  The "Source" frame  */
  frame = gtk_frame_new (_("Select Source"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  table = gtk_table_new (2, 2, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  import_dialog->gradient_radio =
    gtk_radio_button_new_with_mnemonic (NULL, _("_Gradient"));
  gtk_table_attach_defaults (GTK_TABLE (table), import_dialog->gradient_radio,
                             0, 1, 0, 1);
  gtk_widget_show (import_dialog->gradient_radio);

  group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (import_dialog->gradient_radio));

  g_signal_connect (import_dialog->gradient_radio, "toggled",
                    G_CALLBACK (palette_import_grad_callback),
                    import_dialog);

  import_dialog->image_radio =
    gtk_radio_button_new_with_mnemonic (group, _("I_mage"));
  gtk_table_attach_defaults (GTK_TABLE (table), import_dialog->image_radio,
                             0, 1, 1, 2);
  gtk_widget_show (import_dialog->image_radio);

  g_signal_connect (import_dialog->image_radio, "toggled",
                    G_CALLBACK (palette_import_image_callback),
                    import_dialog);

  gtk_widget_set_sensitive (import_dialog->image_radio,
			    gimp_container_num_children (gimp->images) > 0);

  /*  The gradient menu  */
  import_dialog->gradient_menu = gtk_option_menu_new ();
  gimp_table_attach_aligned (GTK_TABLE (table), 0, 0,
                             NULL, 1.0, 0.5,
                             import_dialog->gradient_menu, 1, FALSE);

  menu = gimp_container_menu_new (gimp->gradient_factory->container,
                                  import_dialog->context, 24, 1);
  gtk_option_menu_set_menu (GTK_OPTION_MENU (import_dialog->gradient_menu),
                            menu);
  gtk_widget_show (menu);

  /*  The image menu  */
  import_dialog->image_menu = gtk_option_menu_new ();
  gimp_table_attach_aligned (GTK_TABLE (table), 0, 1,
                             NULL, 1.0, 0.5,
                             import_dialog->image_menu, 1, FALSE);

  menu = gimp_container_menu_new (gimp->images, import_dialog->context, 24, 1);
  gtk_option_menu_set_menu (GTK_OPTION_MENU (import_dialog->image_menu), menu);
  gtk_widget_show (menu);

  {
    gint focus_line_width;
    gint focus_padding;
    gint ythickness;
    gint ysize;

    gtk_widget_style_get (import_dialog->gradient_menu,
                          "focus_line_width", &focus_line_width,
                          "focus_padding",    &focus_padding,
                          NULL);

    ythickness = import_dialog->gradient_menu->style->ythickness;

    ysize = 24 + (2 * (1 /* CHILD_SPACING */ +
                       ythickness            +
                       focus_padding         +
                       focus_line_width));

    gtk_widget_set_size_request (import_dialog->gradient_menu, -1, ysize);
    gtk_widget_set_size_request (import_dialog->image_menu, -1, ysize);
  }


  /*  The "Import" frame  */
  frame = gtk_frame_new (_("Import Options"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  table = gtk_table_new (4, 3, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  /*  The source's name  */
  import_dialog->entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (import_dialog->entry),
                      gradient ? GIMP_OBJECT (gradient)->name : _("New Import"));
  gimp_table_attach_aligned (GTK_TABLE (table), 0, 0,
                             _("Palette _Name:"), 1.0, 0.5,
                             import_dialog->entry, 2, FALSE);

  /*  The # of colors  */
  import_dialog->num_colors =
    GTK_ADJUSTMENT (gimp_scale_entry_new (GTK_TABLE (table), 0, 1,
                                          _("N_umber of Colors:"), -1, 5,
                                          256, 2, 10000, 1, 10, 0,
                                          TRUE, 0.0, 0.0,
                                          NULL, NULL));

  g_signal_connect_swapped (import_dialog->num_colors,
                            "value_changed",
                            G_CALLBACK (palette_import_make_palette),
                            import_dialog);

  /*  The columns  */
  import_dialog->columns =
    GTK_ADJUSTMENT (gimp_scale_entry_new (GTK_TABLE (table), 0, 2,
                                          _("C_olumns:"), -1, 5,
                                          16, 0, 64, 1, 8, 0,
                                          TRUE, 0.0, 0.0,
                                          NULL, NULL));

  g_signal_connect (import_dialog->columns, "value_changed",
                    G_CALLBACK (palette_import_columns_changed),
                    import_dialog);

  /*  The interval  */
  import_dialog->threshold =
    GTK_ADJUSTMENT (gimp_scale_entry_new (GTK_TABLE (table), 0, 3,
                                          _("I_nterval:"), -1, 5,
                                          1, 1, 128, 1, 8, 0,
                                          TRUE, 0.0, 0.0,
                                          NULL, NULL));
  gimp_scale_entry_set_sensitive (GTK_OBJECT (import_dialog->threshold), FALSE);

  g_signal_connect_swapped (import_dialog->threshold, "value_changed",
                            G_CALLBACK (palette_import_make_palette),
                            import_dialog);


  /*  The "Preview" frame  */
  frame = gtk_frame_new (_("Preview"));
  gtk_box_pack_start (GTK_BOX (main_hbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  abox = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_container_set_border_width (GTK_CONTAINER (abox), 4);
  gtk_container_add (GTK_CONTAINER (frame), abox);
  gtk_widget_show (abox);

  import_dialog->preview = gimp_preview_new_full_by_types (GIMP_TYPE_PREVIEW,
                                                           GIMP_TYPE_PALETTE,
                                                           192, 192, 1,
                                                           TRUE, FALSE, FALSE);
  gtk_container_add (GTK_CONTAINER (abox), import_dialog->preview);
  gtk_widget_show (import_dialog->preview);

  /*  keep the dialog up-to-date  */

  g_signal_connect (gimp->images, "add",
                    G_CALLBACK (palette_import_image_add),
                    import_dialog);
  g_signal_connect (gimp->images, "remove",
                    G_CALLBACK (palette_import_image_remove),
                    import_dialog);

  g_signal_connect (import_dialog->context, "gradient_changed",
		    G_CALLBACK (palette_import_gradient_changed),
		    import_dialog);
  g_signal_connect (import_dialog->context, "image_changed",
		    G_CALLBACK (palette_import_image_changed),
		    import_dialog);

  palette_import_make_palette (import_dialog);

  return import_dialog;
}


/*  the palette import action area callbacks  ********************************/

static void
palette_import_close_callback (GtkWidget    *widget,
			       ImportDialog *import_dialog)
{
  Gimp *gimp;

  if (import_dialog->gradient_select)
    gradient_select_free (import_dialog->gradient_select);

  gimp = import_dialog->context->gimp;

  g_signal_handlers_disconnect_by_func (gimp->images,
                                        palette_import_image_add,
                                        import_dialog);
  g_signal_handlers_disconnect_by_func (gimp->images,
                                        palette_import_image_remove,
                                        import_dialog);

  g_object_unref (import_dialog->context);

  if (import_dialog->palette)
    g_object_unref (import_dialog->palette);

  gtk_widget_destroy (import_dialog->dialog);
  g_free (import_dialog);

  the_import_dialog = NULL;
}

static void
palette_import_import_callback (GtkWidget    *widget,
				ImportDialog *import_dialog)
{
  if (import_dialog->palette)
    gimp_container_add (import_dialog->context->gimp->palette_factory->container,
			GIMP_OBJECT (import_dialog->palette));

  palette_import_close_callback (NULL, import_dialog);
}


/*  functions to create & update the import dialog's gradient selection  *****/

static void
palette_import_gradient_changed (GimpContext  *context,
                                 GimpGradient *gradient,
                                 ImportDialog *import_dialog)
{
  if (gradient && import_dialog->import_type == GRADIENT_IMPORT)
    {
      gtk_entry_set_text (GTK_ENTRY (import_dialog->entry),
			  GIMP_OBJECT (gradient)->name);

      palette_import_make_palette (import_dialog);
    }
}

static void
palette_import_image_changed (GimpContext *context,
                              GimpImage   *gimage,
                              ImportDialog *import_dialog)
{
  if (gimage && import_dialog->import_type == IMAGE_IMPORT)
    {
      gchar *basename;
      gchar *label;

      basename = file_utils_uri_to_utf8_basename (gimp_image_get_uri (gimage));
      label = g_strdup_printf ("%s-%d", basename, gimp_image_get_ID (gimage));
      g_free (basename);

      gtk_entry_set_text (GTK_ENTRY (import_dialog->entry), label);

      g_free (label);

      palette_import_make_palette (import_dialog);
    }
}

static void
import_dialog_drop_callback (GtkWidget    *widget,
                             GimpViewable *viewable,
                             gpointer      data)
{
  ImportDialog *import_dialog;

  import_dialog = (ImportDialog *) data;

  gimp_context_set_by_type (import_dialog->context,
                            G_TYPE_FROM_INSTANCE (viewable),
                            GIMP_OBJECT (viewable));

  if (GIMP_IS_GRADIENT (viewable) &&
      import_dialog->import_type != GRADIENT_IMPORT)
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (import_dialog->gradient_radio),
                                    TRUE);
    }
  else if (GIMP_IS_IMAGE (viewable) &&
           import_dialog->import_type != IMAGE_IMPORT)
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (import_dialog->image_radio),
                                    TRUE);
    }
}



/*  the import source menu item callbacks  ***********************************/

static void
palette_import_grad_callback (GtkWidget    *widget,
			      ImportDialog *import_dialog)
{
  GimpGradient *gradient;

  gradient = gimp_context_get_gradient (import_dialog->context);

  import_dialog->import_type = GRADIENT_IMPORT;

  gtk_widget_set_sensitive (import_dialog->image_menu, FALSE);
  gtk_widget_set_sensitive (import_dialog->gradient_menu, TRUE);

  gtk_entry_set_text (GTK_ENTRY (import_dialog->entry),
                      GIMP_OBJECT (gradient)->name);
  gimp_scale_entry_set_sensitive (GTK_OBJECT (import_dialog->threshold), FALSE);

  palette_import_make_palette (import_dialog);
}

static void
palette_import_image_callback (GtkWidget    *widget,
			       ImportDialog *import_dialog)
{
  GimpImage    *gimage;

  gimage = gimp_context_get_image (import_dialog->context);

  if (! gimage)
    {
      gimage = (GimpImage *)
        gimp_container_get_child_by_index (import_dialog->context->gimp->images,
                                           0);
    }

  import_dialog->import_type = IMAGE_IMPORT;

  gtk_widget_set_sensitive (import_dialog->gradient_menu, FALSE);
  gtk_widget_set_sensitive (import_dialog->image_menu, TRUE);

  palette_import_image_changed (import_dialog->context, gimage, import_dialog);

  gimp_scale_entry_set_sensitive (GTK_OBJECT (import_dialog->threshold), TRUE);
}

static void
palette_import_columns_changed (GtkAdjustment *adjustment,
                                ImportDialog  *import_dialog)
{
  if (import_dialog->palette)
    gimp_palette_set_n_columns (import_dialog->palette,
                                ROUND (adjustment->value));
}

/*  functions & callbacks to keep the import dialog uptodate  ****************/

static void
palette_import_image_add (GimpContainer *container,
			  GimpImage     *gimage,
			  ImportDialog  *import_dialog)
{
  if (! GTK_WIDGET_IS_SENSITIVE (import_dialog->image_radio))
    {
      gtk_widget_set_sensitive (import_dialog->image_radio, TRUE);
      gimp_context_set_image (import_dialog->context, gimage);
    }
}

static void
palette_import_image_remove (GimpContainer *container,
                             GimpImage     *gimage,
                             ImportDialog  *import_dialog)
{
  if (! gimp_container_num_children (import_dialog->context->gimp->images))
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (import_dialog->gradient_radio),
                                    TRUE);
      gtk_widget_set_sensitive (import_dialog->image_radio, FALSE);
    }
}

static void
palette_import_make_palette (ImportDialog *import_dialog)
{
  GimpPalette  *palette = NULL;
  gchar        *palette_name;
  GimpGradient *gradient;
  GimpImage    *gimage;
  gint          n_colors;
  gint          n_columns;
  gint          threshold;

  palette_name = (gchar *) gtk_entry_get_text (GTK_ENTRY (import_dialog->entry));

  if (palette_name && strlen (palette_name))
    palette_name = g_strdup (palette_name);
  else
    palette_name = g_strdup (_("Untitled"));

  gradient  = gimp_context_get_gradient (import_dialog->context);
  gimage    = gimp_context_get_image (import_dialog->context);

  n_colors  = ROUND (import_dialog->num_colors->value);
  n_columns = ROUND (import_dialog->columns->value);
  threshold = ROUND (import_dialog->threshold->value);

  switch (import_dialog->import_type)
    {
    case GRADIENT_IMPORT:
      palette = gimp_palette_import_from_gradient (gradient,
						   palette_name,
						   n_colors);
      break;

    case IMAGE_IMPORT:
      if (gimp_image_base_type (gimage) == GIMP_INDEXED)
        {
          palette = 
            gimp_palette_import_from_indexed_image (gimage,
                                                    palette_name);
        }
      else
        {
          palette = gimp_palette_import_from_image (gimage,
                                                    palette_name,
                                                    n_colors,
                                                    threshold);
        }
      break;
    }

  g_free (palette_name);

  if (palette)
    {
      if (import_dialog->palette)
        g_object_unref (import_dialog->palette);

      palette->n_columns = n_columns;

      gimp_preview_set_viewable (GIMP_PREVIEW (import_dialog->preview),
                                 GIMP_VIEWABLE (palette));

      import_dialog->palette = palette;
    }
}
