/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * Copyright (C) 2003  Henrik Brix Andersen <brix@gimp.org>
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

#include "gui-types.h"

#include "config/gimpconfig.h"

#include "core/gimp.h"
#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimpstrokeoptions.h"
#include "core/gimptoolinfo.h"

#include "widgets/gimpcontainermenuimpl.h"
#include "widgets/gimpviewabledialog.h"
#include "widgets/gimpstrokeeditor.h"

#include "stroke-dialog.h"

#include "gimp-intl.h"


/*  local functions  */

static void stroke_dialog_reset_callback      (GtkWidget    *widget,
                                               GtkWidget    *dialog);
static void stroke_dialog_ok_callback         (GtkWidget    *widget,
                                               GtkWidget    *dialog);
static void stroke_dialog_paint_info_selected (GtkWidget    *menu,
                                               GimpViewable *viewable,
                                               gpointer      insert_date,
                                               GtkWidget    *dialog);


/*  public function  */


GtkWidget *
stroke_dialog_new (GimpItem    *item,
                   const gchar *stock_id,
                   const gchar *help_id)
{
  static GimpStrokeOptions *options = NULL;

  GimpImage         *image;
  GtkWidget         *dialog;
  GtkWidget         *main_vbox;
  GtkWidget         *button;
  GSList            *group;
  GtkWidget         *frame;

  g_return_val_if_fail (GIMP_IS_ITEM (item), NULL);
  g_return_val_if_fail (stock_id != NULL, NULL);
  g_return_val_if_fail (help_id != NULL, NULL);

  image = gimp_item_get_image (item);

  if (!options)
    options = g_object_new (GIMP_TYPE_STROKE_OPTIONS,
                            "gimp", image->gimp,
                            NULL);

  gimp_context_set_parent (GIMP_CONTEXT (options),
                           gimp_get_user_context (image->gimp));
  gimp_context_define_properties (GIMP_CONTEXT (options),
                                  GIMP_CONTEXT_FOREGROUND_MASK |
                                  GIMP_CONTEXT_PATTERN_MASK,
                                  FALSE);

  /* the dialog */
  dialog =
    gimp_viewable_dialog_new (GIMP_VIEWABLE (item),
                              _("Stroke Options"), "stroke_options",
                              stock_id,
                              _("Choose Stroke Style"),
                              gimp_standard_help_func,
                              help_id,

                              GIMP_STOCK_RESET, stroke_dialog_reset_callback,
                              NULL, NULL, NULL, FALSE, FALSE,

                              GTK_STOCK_CANCEL, gtk_widget_destroy,
                              NULL, 1, NULL, FALSE, TRUE,

                              GTK_STOCK_OK, stroke_dialog_ok_callback,
                              NULL, NULL, NULL, TRUE, FALSE,

                              NULL);

  g_object_set_data (G_OBJECT (dialog), "gimp-item", item);
  g_object_set_data (G_OBJECT (dialog), "gimp-stroke-options", options);

  main_vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 4);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), main_vbox);
  gtk_widget_show (main_vbox);


  /*  the stroke frame  */

  frame = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  button = gtk_radio_button_new_with_label (NULL, _("Stroke"));
  group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
  gtk_frame_set_label_widget (GTK_FRAME (frame), button);
  gtk_widget_show (button);

  g_signal_connect (button, "toggled",
                    G_CALLBACK (gimp_toggle_button_sensitive_update),
                    NULL);

  g_object_set_data (G_OBJECT (dialog), "gimp-stroke-button", button);

  {
    GtkWidget *stroke_editor;

    stroke_editor = gimp_stroke_editor_new (options);
    gtk_container_set_border_width (GTK_CONTAINER (stroke_editor), 4);
    gtk_container_add (GTK_CONTAINER (frame), stroke_editor);
    gtk_widget_show (stroke_editor);

    g_object_set_data (G_OBJECT (button), "set_sensitive", stroke_editor);
  }


  /*  the paint tool frame  */

  frame = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  button = gtk_radio_button_new_with_label (group,
                                            _("Stroke Using a Paint Tool"));
  group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
  gtk_frame_set_label_widget (GTK_FRAME (frame), button);
  gtk_widget_show (button);

  g_signal_connect (button, "toggled",
                    G_CALLBACK (gimp_toggle_button_sensitive_update),
                    NULL);

  {
    GimpToolInfo *tool_info;
    GtkWidget    *hbox;
    GtkWidget    *label;
    GtkWidget    *optionmenu;
    GtkWidget    *menu;

    tool_info = gimp_context_get_tool (gimp_get_user_context (image->gimp));

    hbox = gtk_hbox_new (FALSE, 4);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    gtk_widget_show (hbox);

    gtk_widget_set_sensitive (GTK_WIDGET (hbox), FALSE);
    g_object_set_data (G_OBJECT (button), "set_sensitive", hbox);

    label = gtk_label_new (_("Paint Tool:"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    gtk_widget_show (label);

    optionmenu = gtk_option_menu_new ();
    gtk_box_pack_start (GTK_BOX (hbox), optionmenu, FALSE, FALSE, 0);
    gtk_widget_show (optionmenu);

    menu = gimp_container_menu_new (image->gimp->paint_info_list, NULL,
                                    16, 0);
    gtk_option_menu_set_menu (GTK_OPTION_MENU (optionmenu), menu);
    gtk_widget_show (menu);

    g_signal_connect (menu, "select_item",
                      G_CALLBACK (stroke_dialog_paint_info_selected),
                      dialog);

    gimp_container_menu_select_item (GIMP_CONTAINER_MENU (menu),
                                     GIMP_VIEWABLE (tool_info->paint_info));

    g_object_set_data (G_OBJECT (dialog), "gimp-tool-menu", menu);
    g_object_set_data (G_OBJECT (dialog), "gimp-paint-info",
                       tool_info->paint_info);
  }

  return dialog;
}


/*  private functions  */

static void
stroke_dialog_reset_callback (GtkWidget  *widget,
                              GtkWidget  *dialog)
{
  GimpItem     *item;
  GimpImage    *image;
  GObject      *options;
  GtkWidget    *button;
  GtkWidget    *menu;
  GimpToolInfo *tool_info;

  item    = g_object_get_data (G_OBJECT (dialog), "gimp-item");
  options = g_object_get_data (G_OBJECT (dialog), "gimp-stroke-options");
  button  = g_object_get_data (G_OBJECT (dialog), "gimp-stroke-button");
  menu    = g_object_get_data (G_OBJECT (dialog), "gimp-tool-menu");

  image = gimp_item_get_image (item);

  tool_info = gimp_context_get_tool (gimp_get_user_context (image->gimp));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
  gimp_container_menu_select_item (GIMP_CONTAINER_MENU (menu),
                                   GIMP_VIEWABLE (tool_info->paint_info));
  gimp_config_reset (options);
}

static void
stroke_dialog_ok_callback (GtkWidget  *widget,
                           GtkWidget  *dialog)
{
  GimpItem     *item;
  GimpDrawable *drawable;
  GtkWidget    *button;
  GimpObject   *options;

  item   = g_object_get_data (G_OBJECT (dialog), "gimp-item");
  button = g_object_get_data (G_OBJECT (dialog), "gimp-stroke-button");

  drawable = gimp_image_active_drawable (gimp_item_get_image (item));

  if (! drawable)
    {
      g_message (_("There is no active layer or channel to stroke to"));
      return;
    }

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    options = g_object_get_data (G_OBJECT (dialog), "gimp-stroke-options");
  else
    options = g_object_get_data (G_OBJECT (dialog), "gimp-paint-info");

  gimp_item_stroke (item, drawable, options);
  gimp_image_flush (gimp_item_get_image (item));

  gtk_widget_destroy (dialog);
}

static void
stroke_dialog_paint_info_selected (GtkWidget    *menu,
                                   GimpViewable *viewable,
                                   gpointer      insert_date,
                                   GtkWidget    *dialog)
{
  g_object_set_data (G_OBJECT (dialog), "gimp-paint-info", viewable);
}
