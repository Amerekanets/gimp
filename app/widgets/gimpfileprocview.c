/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpfileprocview.c
 * Copyright (C) 2004  Sven Neumann <sven@gimp.org>
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

#include "widgets-types.h"

#include "core/gimp.h"
#include "core/gimpmarshal.h"

#include "plug-in/plug-in-proc.h"
#include "plug-in/plug-ins.h"

#include "gimpfileprocview.h"

#include "gimp-intl.h"


enum
{
  COLUMN_PROC,
  COLUMN_LABEL,
  COLUMN_EXTENSIONS,
  COLUMN_STOCK_ID,
  COLUMN_PIXBUF,
  NUM_COLUMNS
};

enum
{
  CHANGED,
  LAST_SIGNAL
};


static void  gimp_file_proc_view_class_init   (GimpFileProcViewClass *klass);

static void  gimp_file_proc_view_selection_changed (GtkTreeSelection *selection,
                                                    GimpFileProcView *view);


static guint  view_signals[LAST_SIGNAL] = { 0 };


GType
gimp_file_proc_view_get_type (void)
{
  static GType view_type = 0;

  if (! view_type)
    {
      static const GTypeInfo view_info =
      {
        sizeof (GimpFileProcViewClass),
        NULL,           /* base_init      */
        NULL,           /* base_finalize  */
        (GClassInitFunc) gimp_file_proc_view_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data     */
        sizeof (GimpFileProcView),
        0,              /* n_preallocs    */
        NULL            /* instance_init  */
      };

      view_type = g_type_register_static (GTK_TYPE_TREE_VIEW,
                                          "GimpFileProcView",
                                          &view_info, 0);
    }

  return view_type;
}

static void
gimp_file_proc_view_class_init (GimpFileProcViewClass *klass)
{
  klass->changed = NULL;

  view_signals[CHANGED] = g_signal_new ("changed",
                                        G_TYPE_FROM_CLASS (klass),
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (GimpFileProcViewClass,
                                                         changed),
                                        NULL, NULL,
                                        gimp_marshal_VOID__VOID,
                                        G_TYPE_NONE, 0);
}


GtkWidget *
gimp_file_proc_view_new (Gimp        *gimp,
                         GSList      *procedures,
                         const gchar *automatic)
{
  GtkTreeView       *view;
  GtkTreeViewColumn *column;
  GtkCellRenderer   *cell;
  GtkListStore      *store;
  GSList            *list;
  GtkTreeIter        iter;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);

  store = gtk_list_store_new (NUM_COLUMNS,
                              G_TYPE_POINTER,    /* COLUMN_PROC       */
                              G_TYPE_STRING,     /* COLUMN_LABEL      */
                              G_TYPE_STRING,     /* COLUMN_EXTENSIONS */
                              G_TYPE_STRING,     /* COLUMN_STOCK_ID   */
                              GDK_TYPE_PIXBUF);  /* COLUMN_PIXBUF     */

  for (list = procedures; list; list = g_slist_next (list))
    {
      PlugInProcDef *proc = list->data;

      if (! proc->prefixes_list) /*  skip URL loaders  */
        {
          const gchar   *domain   = plug_ins_locale_domain (gimp,
                                                            proc->prog, NULL);
          gchar         *label    = plug_in_proc_def_get_label (proc, domain);
          const gchar   *stock_id = plug_in_proc_def_get_stock_id (proc);
          GdkPixbuf     *pixbuf   = plug_in_proc_def_get_pixbuf (proc);

          if (label)
            {
              gtk_list_store_append (store, &iter);
              gtk_list_store_set (store, &iter,
                                  COLUMN_PROC,       proc,
                                  COLUMN_LABEL,      label,
                                  COLUMN_EXTENSIONS, proc->extensions,
                                  COLUMN_STOCK_ID,   stock_id,
                                  COLUMN_PIXBUF,     pixbuf,
                                  -1);

              g_free (label);
            }

          if (pixbuf)
            g_object_unref (pixbuf);
        }
    }

  if (automatic)
    {
      gtk_list_store_prepend (store, &iter);

      gtk_list_store_set (store, &iter,
                          COLUMN_PROC,  NULL,
                          COLUMN_LABEL, automatic,
                          -1);
    }

  view = g_object_new (GIMP_TYPE_FILE_PROC_VIEW,
                       "model",      store,
                       "rules_hint", TRUE,
                       NULL);

  g_object_unref (store);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, _("File Type"));
  gtk_tree_view_column_set_expand (column, TRUE);

  cell = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, cell, FALSE);
  gtk_tree_view_column_set_attributes (column, cell,
                                       "stock_id", COLUMN_STOCK_ID,
                                       "pixbuf",   COLUMN_PIXBUF,
                                       NULL);

  cell = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, cell, TRUE);
  gtk_tree_view_column_set_attributes (column, cell,
                                       "text", COLUMN_LABEL,
                                       NULL);

  gtk_tree_view_append_column (view, column);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, _("Extensions"));

  cell = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, cell, TRUE);
  gtk_tree_view_column_set_attributes (column, cell,
                                       "text", COLUMN_EXTENSIONS,
                                       NULL);

  gtk_tree_view_append_column (view, column);

  g_signal_connect (gtk_tree_view_get_selection (view), "changed",
                    G_CALLBACK (gimp_file_proc_view_selection_changed),
                    view);

  return GTK_WIDGET (view);
}

PlugInProcDef  *
gimp_file_proc_view_get_proc (GimpFileProcView  *view,
                              gchar            **label)
{
  GtkTreeModel     *model;
  GtkTreeSelection *selection;
  GtkTreeIter       iter;

  g_return_val_if_fail (GIMP_IS_FILE_PROC_VIEW (view), NULL);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      PlugInProcDef *proc;

      if (label)
        gtk_tree_model_get (model, &iter,
                            COLUMN_PROC,  &proc,
                            COLUMN_LABEL, label,
                            -1);
      else
        gtk_tree_model_get (model, &iter,
                            COLUMN_PROC,  &proc,
                            -1);

      return proc;
    }

  if (label)
    label = NULL;

  return NULL;
}


gboolean
gimp_file_proc_view_set_proc (GimpFileProcView *view,
                              PlugInProcDef    *proc)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  gboolean      iter_valid;

  g_return_val_if_fail (GIMP_IS_FILE_PROC_VIEW (view), FALSE);

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

  for (iter_valid = gtk_tree_model_get_iter_first (model, &iter);
       iter_valid;
       iter_valid = gtk_tree_model_iter_next (model, &iter))
    {
      PlugInProcDef *this;

      gtk_tree_model_get (model, &iter,
                          COLUMN_PROC, &this,
                          -1);
      if (this == proc)
        break;
    }

  if (iter_valid)
    {
      GtkTreeSelection *selection;

      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

      gtk_tree_selection_select_iter (selection, &iter);
    }

  return iter_valid;
}

static void
gimp_file_proc_view_selection_changed (GtkTreeSelection *selection,
                                       GimpFileProcView *view)
{
  g_signal_emit (view, view_signals[CHANGED], 0);
}
