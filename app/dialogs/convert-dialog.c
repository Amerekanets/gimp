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

#include <gtk/gtk.h>

#include "libgimpwidgets/gimpwidgets.h"

#include "dialogs-types.h"

#include "core/gimp.h"
#include "core/gimpcontainer-filter.h"
#include "core/gimpcontext.h"
#include "core/gimpdatafactory.h"
#include "core/gimpimage.h"
#include "core/gimpimage-convert.h"
#include "core/gimplist.h"
#include "core/gimppalette.h"
#include "core/gimpprogress.h"

#include "widgets/gimpcontainerentry.h"
#include "widgets/gimpdialogfactory.h"
#include "widgets/gimpenumwidgets.h"
#include "widgets/gimphelp-ids.h"
#include "widgets/gimpviewablebutton.h"
#include "widgets/gimpviewabledialog.h"

#include "gimp-intl.h"


typedef struct
{
  GimpImage              *gimage;
  GimpProgress           *progress;

  GtkWidget              *shell;

  GimpContext            *context;
  GimpContainer          *container;
  GimpPalette            *custom_palette;
  gboolean                have_web_palette;

  GimpConvertDitherType   dither_type;
  gboolean                alpha_dither;
  gboolean                remove_dups;
  gint                    num_colors;
  GimpConvertPaletteType  palette_type;
} IndexedDialog;


static void        convert_dialog_response        (GtkWidget        *widget,
                                                   gint              response_id,
                                                   IndexedDialog    *dialog);
static GtkWidget * convert_dialog_palette_box     (IndexedDialog    *dialog);
static gboolean    convert_dialog_palette_filter  (const GimpObject *object,
                                                   gpointer          user_data);
static void        convert_dialog_palette_changed (GimpContext      *context,
                                                   GimpPalette      *palette,
                                                   IndexedDialog    *dialog);


/*  defaults  */

static GimpConvertDitherType   saved_dither_type  = GIMP_FS_DITHER;
static gboolean                saved_alpha_dither = FALSE;
static gboolean                saved_remove_dups  = TRUE;
static gint                    saved_num_colors   = 256;
static GimpConvertPaletteType  saved_palette_type = GIMP_MAKE_PALETTE;
static GimpPalette            *saved_palette      = NULL;


/*  public functions  */

GtkWidget *
convert_dialog_new (GimpImage    *gimage,
                    GtkWidget    *parent,
                    GimpProgress *progress)
{
  IndexedDialog *dialog;
  GtkWidget     *main_vbox;
  GtkWidget     *vbox;
  GtkWidget     *hbox;
  GtkWidget     *label;
  GtkObject     *adjustment;
  GtkWidget     *spinbutton;
  GtkWidget     *frame;
  GtkWidget     *toggle;
  GtkWidget     *palette_box;
  GSList        *group = NULL;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (parent), NULL);
  g_return_val_if_fail (progress == NULL || GIMP_IS_PROGRESS (progress), NULL);

  dialog = g_new0 (IndexedDialog, 1);

  dialog->gimage   = gimage;
  dialog->progress = progress;

  dialog->dither_type           = saved_dither_type;
  dialog->alpha_dither          = saved_alpha_dither;
  dialog->remove_dups           = saved_remove_dups;
  dialog->num_colors            = saved_num_colors;
  dialog->palette_type          = saved_palette_type;

  dialog->shell =
    gimp_viewable_dialog_new (GIMP_VIEWABLE (gimage),
                              _("Indexed Color Conversion"),
                              "gimp-image-convert-indexed",
                              GIMP_STOCK_CONVERT_INDEXED,
                              _("Convert Image to Indexed Colors"),
                              parent,
                              gimp_standard_help_func,
                              GIMP_HELP_IMAGE_CONVERT_INDEXED,

                              GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                              GTK_STOCK_OK,     GTK_RESPONSE_OK,

                              NULL);

  g_signal_connect (dialog->shell, "response",
                    G_CALLBACK (convert_dialog_response),
                    dialog);

  g_object_weak_ref (G_OBJECT (dialog->shell),
                     (GWeakNotify) g_free,
                     dialog);

  palette_box = convert_dialog_palette_box (dialog);

  if (dialog->context)
    g_object_weak_ref (G_OBJECT (dialog->shell),
                       (GWeakNotify) g_object_unref,
                       dialog->context);

  if (dialog->container)
    g_object_weak_ref (G_OBJECT (dialog->shell),
                       (GWeakNotify) g_object_unref,
                       dialog->container);

  main_vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog->shell)->vbox),
		     main_vbox);
  gtk_widget_show (main_vbox);

  frame = gimp_frame_new (_("General Palette Options"));
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  vbox = gtk_vbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);

  /*  'generate palette'  */
  hbox = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  toggle = gtk_radio_button_new_with_label (NULL,
                                            _("Generate optimum palette:"));
  group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (toggle));
  gtk_box_pack_start (GTK_BOX (hbox), toggle, FALSE, FALSE, 0);
  gtk_widget_show (toggle);

  g_object_set_data (G_OBJECT (toggle), "gimp-item-data",
                     GINT_TO_POINTER (GIMP_MAKE_PALETTE));
  g_signal_connect (toggle, "toggled",
		    G_CALLBACK (gimp_radio_button_update),
		    &dialog->palette_type);

  if (dialog->num_colors == 256 && gimp_image_has_alpha (gimage))
    dialog->num_colors = 255;

  spinbutton = gimp_spin_button_new (&adjustment, dialog->num_colors,
				     2, 256, 1, 8, 0, 1, 0);
  gtk_box_pack_end (GTK_BOX (hbox), spinbutton, FALSE, FALSE, 0);
  gtk_widget_show (spinbutton);

  g_signal_connect (adjustment, "value_changed",
		    G_CALLBACK (gimp_int_adjustment_update),
		    &dialog->num_colors);

  label = gtk_label_new (_("Max. number of colors:"));
  gtk_box_pack_end (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  gtk_widget_set_sensitive (GTK_WIDGET (spinbutton), dialog->num_colors);
  gtk_widget_set_sensitive (GTK_WIDGET (label), dialog->num_colors);
  g_object_set_data (G_OBJECT (toggle), "set_sensitive", spinbutton);
  g_object_set_data (G_OBJECT (spinbutton), "set_sensitive", label);

  gtk_widget_show (hbox);

  if (! dialog->have_web_palette)
    {
      /*  'web palette'
       * Only presented as an option to the user if they do not
       * already have the 'Web' GIMP palette installed on their
       * system.
       */
      toggle = gtk_radio_button_new_with_label (group,
                                                _("Use WWW-Optimized Palette"));
      group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (toggle));
      gtk_box_pack_start (GTK_BOX (vbox), toggle, FALSE, FALSE, 0);
      gtk_widget_show (toggle);

      g_object_set_data (G_OBJECT (toggle), "gimp-item-data",
                         GINT_TO_POINTER (GIMP_WEB_PALETTE));
      g_signal_connect (toggle, "toggled",
			G_CALLBACK (gimp_radio_button_update),
			&dialog->palette_type);
    }

  /*  'mono palette'  */
  toggle = gtk_radio_button_new_with_label (group, _("Use black and "
                                                     "white (1-bit) palette"));
  group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (toggle));
  gtk_box_pack_start (GTK_BOX (vbox), toggle, FALSE, FALSE, 0);
  gtk_widget_show (toggle);

  g_object_set_data (G_OBJECT (toggle), "gimp-item-data",
                     GINT_TO_POINTER (GIMP_MONO_PALETTE));
  g_signal_connect (toggle, "toggled",
		    G_CALLBACK (gimp_radio_button_update),
		    &dialog->palette_type);

  /* 'custom' palette from dialog */
  if (palette_box)
    {
      GtkWidget *remove_toggle;

      remove_toggle = gtk_check_button_new_with_label (_("Remove unused colors "
                                                         "from final palette"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (remove_toggle),
                                    dialog->remove_dups);
      g_signal_connect (remove_toggle, "toggled",
			G_CALLBACK (gimp_toggle_button_update),
			&dialog->remove_dups);

      /* 'custom' palette from dialog */
      hbox = gtk_hbox_new (FALSE, 6);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
      gtk_widget_show (hbox);

      toggle = gtk_radio_button_new_with_label (group,
                                                _("Use custom palette:"));
      group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (toggle));
      gtk_box_pack_start (GTK_BOX (hbox), toggle, FALSE, FALSE, 0);
      gtk_widget_show (toggle);

      g_object_set_data (G_OBJECT (toggle), "gimp-item-data",
                         GINT_TO_POINTER (GIMP_CUSTOM_PALETTE));
      g_signal_connect (toggle, "toggled",
			G_CALLBACK (gimp_radio_button_update),
			&dialog->palette_type);
      g_object_set_data (G_OBJECT (toggle), "set_sensitive", remove_toggle);

      gtk_box_pack_end (GTK_BOX (hbox), palette_box, TRUE, TRUE, 0);
      gtk_widget_show (palette_box);

      gtk_widget_set_sensitive (remove_toggle,
				dialog->palette_type == GIMP_CUSTOM_PALETTE);
      gtk_widget_set_sensitive (palette_box,
                                dialog->palette_type == GIMP_CUSTOM_PALETTE);
      g_object_set_data (G_OBJECT (toggle), "set_sensitive", remove_toggle);
      g_object_set_data (G_OBJECT (remove_toggle), "set_sensitive",
			 palette_box);

      /*  add the remove-duplicates toggle  */
      hbox = gtk_hbox_new (FALSE, 6);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
      gtk_widget_show (hbox);

      gtk_box_pack_start (GTK_BOX (hbox), remove_toggle, TRUE, TRUE, 20);
      gtk_widget_show (remove_toggle);
    }

  gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (toggle),
                                   dialog->palette_type);

  /*  the dither type  */
  frame = gimp_frame_new (_("Dithering Options"));
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  vbox = gimp_enum_radio_box_new (GIMP_TYPE_CONVERT_DITHER_TYPE,
                                  G_CALLBACK (gimp_radio_button_update),
                                  &dialog->dither_type,
                                  &toggle);
  gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (toggle),
                                   dialog->dither_type);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show(vbox);

  /*  the alpha-dither toggle  */
  toggle =
    gtk_check_button_new_with_label (_("Enable dithering of transparency"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
				dialog->alpha_dither);
  gtk_box_pack_start (GTK_BOX (vbox), toggle, FALSE, FALSE, 0);
  gtk_widget_show (toggle);

  g_signal_connect (toggle, "toggled",
		    G_CALLBACK (gimp_toggle_button_update),
		    &dialog->alpha_dither);

  /* if the image isn't non-alpha/layered, set the default number of
     colours to one less than max, to leave room for a transparent index
     for transparent/animated GIFs */
  if (gimp_image_has_alpha (gimage))
    {
      frame = gimp_frame_new (_("[ Warning ]"));
      gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
      gtk_widget_show (frame);

      label = gtk_label_new
	(_("You are attempting to convert an image with an alpha channel "
           "to indexed colors.\n"
           "Do not generate a palette of more than 255 colors if you "
           "intend to create a transparent or animated GIF file."));
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
      gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
      gtk_container_add (GTK_CONTAINER (frame), label);
      gtk_widget_show (label);
    }

  return dialog->shell;
}


/*  private functions  */

static void
convert_dialog_response (GtkWidget     *widget,
                         gint           response_id,
                         IndexedDialog *dialog)
{
  if (response_id == GTK_RESPONSE_OK)
    {
      GimpProgress *progress;

      progress = gimp_progress_start (dialog->progress,
                                      _("Converting to indexed..."), FALSE);

      /*  Convert the image to indexed color  */
      gimp_image_convert (dialog->gimage,
                          GIMP_INDEXED,
                          dialog->num_colors,
                          dialog->dither_type,
                          dialog->alpha_dither,
                          dialog->remove_dups,
                          dialog->palette_type,
                          dialog->custom_palette,
                          progress);

      if (progress)
        gimp_progress_end (progress);

      gimp_image_flush (dialog->gimage);

      /* Save defaults for next time */
      saved_dither_type  = dialog->dither_type;
      saved_alpha_dither = dialog->alpha_dither;
      saved_remove_dups  = dialog->remove_dups;
      saved_num_colors   = dialog->num_colors;
      saved_palette_type = dialog->palette_type;
      saved_palette      = dialog->custom_palette;
    }

  gtk_widget_destroy (dialog->shell);
}

static GtkWidget *
convert_dialog_palette_box (IndexedDialog *dialog)
{
  Gimp        *gimp;
  GList       *list;
  GimpPalette *palette;
  GimpPalette *web_palette   = NULL;
  gboolean     default_found = FALSE;

  gimp = dialog->gimage->gimp;

  dialog->have_web_palette = FALSE;

  /* We can't dither to > 256 colors */
  dialog->container = gimp_container_filter (gimp->palette_factory->container,
                                             convert_dialog_palette_filter,
                                             NULL);

  if (! gimp_container_num_children (dialog->container))
    {
      g_object_unref (dialog->container);
      return NULL;
    }

  dialog->context = gimp_context_new (gimp, "convert-dialog", NULL);

  for (list = GIMP_LIST (dialog->container)->list;
       list;
       list = g_list_next (list))
    {
      palette = (GimpPalette *) list->data;

      /* Preferentially, the initial default is 'Web' if available */
      if (web_palette == NULL &&
	  g_ascii_strcasecmp (GIMP_OBJECT (palette)->name, "Web") == 0)
	{
	  web_palette = palette;
	  dialog->have_web_palette = TRUE;
	}

      if (saved_palette == palette)
        {
          dialog->custom_palette = saved_palette;
          default_found = TRUE;
        }
    }

  if (! default_found)
    {
      if (web_palette)
        dialog->custom_palette = web_palette;
      else
        dialog->custom_palette = GIMP_LIST (dialog->container)->list->data;
    }

  gimp_context_set_palette (dialog->context, dialog->custom_palette);

  g_signal_connect (dialog->context, "palette-changed",
                    G_CALLBACK (convert_dialog_palette_changed),
                    dialog);

  {
    GtkWidget *hbox;
    GtkWidget *button;
    GtkWidget *entry;

    hbox = gtk_hbox_new (FALSE, 4);

    button = gimp_viewable_button_new (dialog->container,
                                       dialog->context,
                                       GIMP_VIEW_SIZE_MEDIUM, 1,
                                       gimp_dialog_factory_from_name ("dock"),
                                       "gimp-palette-list|gimp-palette-grid",
                                       GIMP_STOCK_PALETTE,
                                       _("Open the palette selection dialog"));
    gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
    gtk_widget_show (button);

    entry = gimp_container_entry_new (dialog->container,
                                      dialog->context,
                                      GIMP_VIEW_SIZE_MEDIUM, 1);
    gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
    gtk_widget_show (entry);

    return hbox;
  }
}

static gboolean
convert_dialog_palette_filter (const GimpObject *object,
                               gpointer          user_data)
{
  return GIMP_PALETTE (object)->n_colors <= 256;
}

static void
convert_dialog_palette_changed (GimpContext   *context,
				GimpPalette   *palette,
                                IndexedDialog *dialog)
{
  if (! palette)
    return;

  if (palette->n_colors > 256)
    {
      g_message (_("Cannot convert to a palette with more than 256 colors."));
    }
  else
    {
      dialog->custom_palette = palette;
    }
}
