/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpfiledialog.c
 * Copyright (C) 2004 Michael Natterer <mitch@gimp.org>
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

#include "widgets-types.h"

#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimpprogress.h"

#include "config/gimpcoreconfig.h"

#include "file/file-utils.h"

#include "plug-in/plug-in-proc.h"
#include "plug-in/plug-ins.h"

#include "gimpfiledialog.h"
#include "gimpfileprocview.h"
#include "gimphelp-ids.h"
#include "gimppreview.h"
#include "gimppreviewrendererimagefile.h"
#include "gimpthumbbox.h"
#include "gimpwidgets-utils.h"

#include "gimp-intl.h"


static void  gimp_file_dialog_class_init          (GimpFileDialogClass   *klass);
static void  gimp_file_dialog_progress_iface_init (GimpProgressInterface *progress_iface);

static gboolean   gimp_file_dialog_delete_event    (GtkWidget        *widget,
                                                    GdkEventAny      *event);
static void       gimp_file_dialog_response        (GtkDialog        *dialog,
                                                    gint              response_id);

static GimpProgress *
                   gimp_file_dialog_progress_start (GimpProgress     *progress,
                                                    const gchar      *message,
                                                    gboolean          cancelable);
static void  gimp_file_dialog_progress_end         (GimpProgress     *progress);
static void  gimp_file_dialog_progress_set_text    (GimpProgress     *progress,
                                                    const gchar      *message);
static void  gimp_file_dialog_progress_set_value   (GimpProgress     *progress,
                                                    gdouble           percentage);
static gdouble gimp_file_dialog_progress_get_value (GimpProgress     *progress);

static void  gimp_file_dialog_add_preview          (GimpFileDialog   *dialog,
                                                    Gimp             *gimp);
static void  gimp_file_dialog_add_filters          (GimpFileDialog   *dialog,
                                                    Gimp             *gimp,
                                                    GSList           *file_procs);
static void  gimp_file_dialog_add_proc_selection   (GimpFileDialog   *dialog,
                                                    Gimp             *gimp,
                                                    GSList           *file_procs,
                                                    const gchar      *automatic,
                                                    const gchar      *automatic_help_id);

static void  gimp_file_dialog_selection_changed    (GtkFileChooser   *chooser,
                                                    GimpFileDialog   *dialog);
static void  gimp_file_dialog_update_preview       (GtkFileChooser   *chooser,
                                                    GimpFileDialog   *dialog);

static void  gimp_file_dialog_proc_changed         (GimpFileProcView *view,
                                                    GimpFileDialog   *dialog);

static void  gimp_file_dialog_help_func            (const gchar      *help_id,
                                                    gpointer          help_data);



GType
gimp_file_dialog_get_type (void)
{
  static GType dialog_type = 0;

  if (! dialog_type)
    {
      static const GTypeInfo dialog_info =
      {
        sizeof (GimpFileDialogClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gimp_file_dialog_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data     */
        sizeof (GimpFileDialog),
        0,              /* n_preallocs    */
        NULL,           /* instance_init  */
      };

      static const GInterfaceInfo progress_iface_info =
      {
        (GInterfaceInitFunc) gimp_file_dialog_progress_iface_init,
        NULL,           /* iface_finalize */
        NULL            /* iface_data     */
      };

      dialog_type = g_type_register_static (GTK_TYPE_FILE_CHOOSER_DIALOG,
                                            "GimpFileDialog",
                                            &dialog_info, 0);

      g_type_add_interface_static (dialog_type, GIMP_TYPE_PROGRESS,
                                   &progress_iface_info);
    }

  return dialog_type;
}

static void
gimp_file_dialog_class_init (GimpFileDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

  widget_class->delete_event = gimp_file_dialog_delete_event;

  dialog_class->response     = gimp_file_dialog_response;
}

static void
gimp_file_dialog_progress_iface_init (GimpProgressInterface *progress_iface)
{
  progress_iface->start     = gimp_file_dialog_progress_start;
  progress_iface->end       = gimp_file_dialog_progress_end;
  progress_iface->set_text  = gimp_file_dialog_progress_set_text;
  progress_iface->set_value = gimp_file_dialog_progress_set_value;
  progress_iface->get_value = gimp_file_dialog_progress_get_value;
}

static gboolean
gimp_file_dialog_delete_event (GtkWidget   *widget,
                               GdkEventAny *event)
{
  return TRUE;
}

static void
gimp_file_dialog_response (GtkDialog *dialog,
                           gint       response_id)
{
  GimpFileDialog *file_dialog = GIMP_FILE_DIALOG (dialog);

  if (response_id != GTK_RESPONSE_OK && file_dialog->busy)
    {
      file_dialog->canceled = TRUE;

      if (file_dialog->progress_active && file_dialog->progress_cancelable)
        gimp_progress_cancel (GIMP_PROGRESS (dialog));
    }
}

static GimpProgress *
gimp_file_dialog_progress_start (GimpProgress *progress,
                                 const gchar  *message,
                                 gboolean      cancelable)
{
  GimpFileDialog *dialog = GIMP_FILE_DIALOG (progress);

  if (! dialog->progress_active)
    {
      GtkProgressBar *bar = GTK_PROGRESS_BAR (dialog->progress);

      gtk_progress_bar_set_text (bar, message);
      gtk_progress_bar_set_fraction (bar, 0.0);

      gtk_widget_show (dialog->progress);

      dialog->progress_active     = TRUE;
      dialog->progress_cancelable = cancelable;

      return progress;
    }

  return NULL;
}

static void
gimp_file_dialog_progress_end (GimpProgress *progress)
{
  GimpFileDialog *dialog = GIMP_FILE_DIALOG (progress);

  if (dialog->progress_active)
    {
      GtkProgressBar *bar = GTK_PROGRESS_BAR (dialog->progress);

      gtk_progress_bar_set_text (bar, "");
      gtk_progress_bar_set_fraction (bar, 0.0);

      gtk_widget_hide (dialog->progress);

      dialog->progress_active     = FALSE;
      dialog->progress_cancelable = FALSE;
    }
}

static void
gimp_file_dialog_progress_set_text (GimpProgress *progress,
                                    const gchar  *message)
{
  GimpFileDialog *dialog = GIMP_FILE_DIALOG (progress);

  if (dialog->progress_active)
    {
      GtkProgressBar *bar = GTK_PROGRESS_BAR (dialog->progress);

      gtk_progress_bar_set_text (bar, message);
    }
}

static void
gimp_file_dialog_progress_set_value (GimpProgress *progress,
                                     gdouble       percentage)
{
  GimpFileDialog *dialog = GIMP_FILE_DIALOG (progress);

  if (dialog->progress_active)
    {
      GtkProgressBar *bar = GTK_PROGRESS_BAR (dialog->progress);

      gtk_progress_bar_set_fraction (bar, percentage);
    }
}

static gdouble
gimp_file_dialog_progress_get_value (GimpProgress *progress)
{
  GimpFileDialog *dialog = GIMP_FILE_DIALOG (progress);

  if (dialog->progress_active)
    {
      GtkProgressBar *bar = GTK_PROGRESS_BAR (dialog->progress);

      return gtk_progress_bar_get_fraction (bar);
    }

  return 0.0;
}


/*  public functions  */

GtkWidget *
gimp_file_dialog_new (Gimp                 *gimp,
                      GtkFileChooserAction  action,
                      const gchar          *title,
                      const gchar          *role,
                      const gchar          *stock_id,
                      const gchar          *help_id)
{
  GimpFileDialog *dialog;
  GSList         *file_procs;
  const gchar    *automatic;
  const gchar    *automatic_help_id;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);
  g_return_val_if_fail (title != NULL, NULL);
  g_return_val_if_fail (role != NULL, NULL);
  g_return_val_if_fail (stock_id != NULL, NULL);
  g_return_val_if_fail (help_id != NULL, NULL);

  switch (action)
    {
    case GTK_FILE_CHOOSER_ACTION_OPEN:
      file_procs = gimp->load_procs;
      automatic  = _("Automatically Detected");
      automatic_help_id = GIMP_HELP_FILE_OPEN_BY_EXTENSION;
      break;

    case GTK_FILE_CHOOSER_ACTION_SAVE:
      file_procs = gimp->save_procs;
      automatic  = _("By Extension");
      automatic_help_id = GIMP_HELP_FILE_SAVE_BY_EXTENSION;
      break;

    default:
      g_return_val_if_reached (NULL);
      return NULL;
    }

  dialog = g_object_new (GIMP_TYPE_FILE_DIALOG,
                         "title",  title,
                         "role",   role,
                         "action", action,
                         NULL);

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                          stock_id,         GTK_RESPONSE_OK,
                          NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  gimp_help_connect (GTK_WIDGET (dialog),
                     gimp_file_dialog_help_func, help_id, dialog);

  gimp_file_dialog_add_preview (dialog, gimp);

  gimp_file_dialog_add_filters (dialog, gimp, file_procs);

  gimp_file_dialog_add_proc_selection (dialog, gimp, file_procs, automatic,
                                       automatic_help_id);

  dialog->progress = gtk_progress_bar_new ();
  gtk_box_pack_end (GTK_BOX (GTK_DIALOG (dialog)->vbox), dialog->progress,
                    FALSE, FALSE, 0);

  return GTK_WIDGET (dialog);
}

void
gimp_file_dialog_set_sensitive (GimpFileDialog *dialog,
                                gboolean        sensitive)
{
  g_return_if_fail (GIMP_IS_FILE_DIALOG (dialog));

  gimp_dialog_set_sensitive (GTK_DIALOG (dialog), sensitive);

  dialog->busy     = ! sensitive;
  dialog->canceled = FALSE;
}

void
gimp_file_dialog_set_file_proc (GimpFileDialog *dialog,
                                PlugInProcDef  *file_proc)
{
  g_return_if_fail (GIMP_IS_FILE_DIALOG (dialog));

  if (file_proc != dialog->file_proc)
    gimp_file_proc_view_set_proc (GIMP_FILE_PROC_VIEW (dialog->proc_view),
                                  file_proc);
}

void
gimp_file_dialog_set_uri (GimpFileDialog  *dialog,
                          GimpImage       *gimage,
                          const gchar     *uri)
{
  gchar    *real_uri  = NULL;
  gboolean  is_folder = FALSE;

  g_return_if_fail (GIMP_IS_FILE_DIALOG (dialog));
  g_return_if_fail (gimage == NULL || GIMP_IS_IMAGE (gimage));

  if (gimage)
    {
      gchar *filename = gimp_image_get_filename (gimage);

      if (filename)
        {
          gchar *dirname = g_path_get_dirname (filename);

          g_free (filename);

          real_uri = g_filename_to_uri (dirname, NULL, NULL);
          g_free (dirname);

          is_folder = TRUE;
        }
    }
  else if (uri)
    {
      real_uri = g_strdup (uri);
    }

  if (! real_uri)
    {
      gchar *current = g_get_current_dir ();

      real_uri = g_filename_to_uri (current, NULL, NULL);
      g_free (current);

      is_folder = TRUE;
    }

  if (is_folder)
    gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (dialog),
                                             real_uri);
  else
    gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (dialog), real_uri);

  g_free (real_uri);
}

void
gimp_file_dialog_set_image (GimpFileDialog *dialog,
                            GimpImage      *gimage,
                            gboolean        set_uri_and_proc,
                            gboolean        set_image_clean)
{
  const gchar *uri;

  g_return_if_fail (GIMP_IS_FILE_DIALOG (dialog));
  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  dialog->gimage           = gimage;
  dialog->set_uri_and_proc = set_uri_and_proc;
  dialog->set_image_clean  = set_image_clean;

  uri = gimp_object_get_name (GIMP_OBJECT (gimage));

  if (uri)
    gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (dialog), uri);
  else
    gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), "");

  gimp_file_dialog_set_file_proc (dialog, NULL);
}


/*  private functions  */

static void
gimp_file_dialog_add_preview (GimpFileDialog *dialog,
                              Gimp           *gimp)
{
  if (gimp->config->thumbnail_size <= 0)
    return;

  gtk_file_chooser_set_use_preview_label (GTK_FILE_CHOOSER (dialog), FALSE);

  g_signal_connect (dialog, "selection-changed",
                    G_CALLBACK (gimp_file_dialog_selection_changed),
                    dialog);
  g_signal_connect (dialog, "update-preview",
                    G_CALLBACK (gimp_file_dialog_update_preview),
                    dialog);

  dialog->thumb_box = gimp_thumb_box_new (gimp);
  gtk_widget_set_sensitive (GTK_WIDGET (dialog->thumb_box), FALSE);
  gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (dialog),
                                       dialog->thumb_box);
  gtk_widget_show (dialog->thumb_box);

#ifdef ENABLE_FILE_SYSTEM_ICONS
  GIMP_PREVIEW_RENDERER_IMAGEFILE (GIMP_PREVIEW (GIMP_THUMB_BOX (dialog->thumb_box)->preview)->renderer)->file_system = _gtk_file_chooser_get_file_system (GTK_FILE_CHOOSER (dialog));
#endif
}

static void
gimp_file_dialog_add_filters (GimpFileDialog *dialog,
                              Gimp           *gimp,
                              GSList         *file_procs)
{
  GtkFileFilter *filter;
  GSList        *list;

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("All Files (*.*)"));
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);

  for (list = file_procs; list; list = g_slist_next (list))
    {
      PlugInProcDef *file_proc = list->data;

      if (file_proc->menu_paths && file_proc->extensions_list)
        {
          const gchar *domain;
          GString     *label;
          GSList      *ext;
          gboolean     first = TRUE;

          domain = plug_ins_locale_domain (gimp, file_proc->prog, NULL);

          label = g_string_new (plug_in_proc_def_get_label (file_proc, domain));

          filter = gtk_file_filter_new ();

          for (ext = file_proc->extensions_list; ext; ext = g_slist_next (ext))
            {
              const gchar *extension = ext->data;
              gchar       *pattern   = g_strdup_printf ("*.%s", extension);

              gtk_file_filter_add_pattern (filter, pattern);
              g_free (pattern);

              if (first)
                {
                  g_string_append (label, " (*.");
                  first = FALSE;
                }

              g_string_append (label, extension);
              g_string_append (label, ext->next ? ", *." : ")");
            }

          gtk_file_filter_set_name (filter, label->str);
          g_string_free (label, TRUE);

          gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
        }
    }
}

static void
gimp_file_dialog_add_proc_selection (GimpFileDialog *dialog,
                                     Gimp           *gimp,
                                     GSList         *file_procs,
                                     const gchar    *automatic,
                                     const gchar    *automatic_help_id)
{
  GtkWidget *scrolled_window;

  dialog->proc_expander = gtk_expander_new_with_mnemonic (NULL);
  gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (dialog),
                                     dialog->proc_expander);
  gtk_widget_show (dialog->proc_expander);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
                                       GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (dialog->proc_expander), scrolled_window);
  gtk_widget_show (scrolled_window);

  gtk_widget_set_size_request (scrolled_window, -1, 200);

  dialog->proc_view = gimp_file_proc_view_new (gimp, file_procs, automatic,
                                               automatic_help_id);
  gtk_container_add (GTK_CONTAINER (scrolled_window), dialog->proc_view);
  gtk_widget_show (dialog->proc_view);

  g_signal_connect (dialog->proc_view, "changed",
                    G_CALLBACK (gimp_file_dialog_proc_changed),
                    dialog);

  gimp_file_proc_view_set_proc (GIMP_FILE_PROC_VIEW (dialog->proc_view), NULL);
}

static void
gimp_file_dialog_selection_changed (GtkFileChooser *chooser,
                                    GimpFileDialog *dialog)
{
  gimp_thumb_box_take_uris (GIMP_THUMB_BOX (dialog->thumb_box),
                            gtk_file_chooser_get_uris (chooser));
}

static void
gimp_file_dialog_update_preview (GtkFileChooser *chooser,
                                 GimpFileDialog *dialog)
{
  gchar *uri = gtk_file_chooser_get_preview_uri (chooser);

  gimp_thumb_box_set_uri (GIMP_THUMB_BOX (dialog->thumb_box), uri);

  g_free (uri);
}

static void
gimp_file_dialog_proc_changed (GimpFileProcView *view,
                               GimpFileDialog   *dialog)
{
  GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
  gchar          *name;
  gchar          *label;

  dialog->file_proc = gimp_file_proc_view_get_proc (view, &name);

  label = g_strdup_printf (_("Select File _Type (%s)"), name);

  gtk_expander_set_label (GTK_EXPANDER (dialog->proc_expander), label);

  g_free (label);
  g_free (name);

  if (gtk_file_chooser_get_action (chooser) == GTK_FILE_CHOOSER_ACTION_SAVE)
    {
      PlugInProcDef *proc = dialog->file_proc;

      if (proc && proc->extensions_list)
        {
          gchar *uri = gtk_file_chooser_get_uri (chooser);

          if (uri && strlen (uri))
            {
              const gchar *last_dot = strrchr (uri, '.');

              if (last_dot != uri)
                {
                  GString *s = g_string_new (uri);
                  gchar   *basename;

                  if (last_dot)
                    g_string_truncate (s, last_dot - uri);

                  g_string_append (s, ".");
                  g_string_append (s, (gchar *) proc->extensions_list->data);

                  gtk_file_chooser_set_uri (chooser, s->str);

                  basename = file_utils_uri_to_utf8_basename (s->str);
                  gtk_file_chooser_set_current_name (chooser, basename);
                  g_free (basename);

                  g_string_free (s, TRUE);
                }
            }

          g_free (uri);
        }
    }
}

static void
gimp_file_dialog_help_func (const gchar *help_id,
                            gpointer     help_data)
{
  GimpFileDialog *dialog = GIMP_FILE_DIALOG (help_data);
  GtkWidget      *focus;

  focus = gtk_window_get_focus (GTK_WINDOW (dialog));

  if (focus == dialog->proc_view)
    {
      gchar *proc_help_id;

      proc_help_id =
        gimp_file_proc_view_get_help_id (GIMP_FILE_PROC_VIEW (dialog->proc_view));

      gimp_standard_help_func (proc_help_id, NULL);

      g_free (proc_help_id);
    }
  else
    {
      gimp_standard_help_func (help_id, NULL);
    }
}
