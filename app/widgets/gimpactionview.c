/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpactionview.c
 * Copyright (C) 2004  Michael Natterer <mitch@gimp.org>
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

#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "widgets-types.h"

#include "gimpaction.h"
#include "gimpactiongroup.h"
#include "gimpactionview.h"
#include "gimpcellrendereraccel.h"
#include "gimpuimanager.h"
#include "gimpwidgets-utils.h"

#include "gimp-intl.h"


enum
{
  COLUMN_ACTION,
  COLUMN_STOCK_ID,
  COLUMN_LABEL,
  COLUMN_NAME,
  COLUMN_ACCEL_KEY,
  COLUMN_ACCEL_MASK,
  COLUMN_MENU_ITEM,
  NUM_COLUMNS
};


/*  local function prototypes  */

static void     gimp_action_view_class_init  (GimpActionViewClass *klass);
static void     gimp_action_view_init        (GimpActionView      *view);

static gboolean gimp_action_view_accel_find_func (GtkAccelKey     *key,
                                                  GClosure        *closure,
                                                  gpointer         data);
static void     gimp_action_view_accel_changed   (GtkAccelGroup   *accel_group,
                                                  guint            unused1,
                                                  GdkModifierType  unused2,
                                                  GClosure        *accel_closure,
                                                  GimpActionView  *view);
static void     gimp_action_view_accel_edited    (GimpCellRendererAccel *accel,
                                                  const char      *path_string,
                                                  gboolean         delete,
                                                  guint            accel_key,
                                                  GdkModifierType  accel_mask,
                                                  GimpActionView  *view);



GType
gimp_action_view_get_type (void)
{
  static GType view_type = 0;

  if (! view_type)
    {
      static const GTypeInfo view_info =
      {
        sizeof (GimpActionViewClass),
        NULL,           /* base_init      */
        NULL,           /* base_finalize  */
        (GClassInitFunc) gimp_action_view_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data     */
        sizeof (GimpActionView),
        0,              /* n_preallocs    */
        (GInstanceInitFunc) gimp_action_view_init
      };

      view_type = g_type_register_static (GTK_TYPE_TREE_VIEW,
                                          "GimpActionView",
                                          &view_info, 0);
    }

  return view_type;
}

static void
gimp_action_view_class_init (GimpActionViewClass *klass)
{
}

static void
gimp_action_view_init (GimpActionView *view)
{
}

GtkWidget *
gimp_action_view_new (GimpUIManager *manager,
                      gboolean       show_shortcuts)
{
  GtkTreeView       *view;
  GtkTreeViewColumn *column;
  GtkCellRenderer   *cell;
  GtkTreeStore      *store;
  GtkAccelGroup     *accel_group;
  GList             *list;

  g_return_val_if_fail (GIMP_IS_UI_MANAGER (manager), NULL);

  store = gtk_tree_store_new (NUM_COLUMNS,
                              GTK_TYPE_ACTION,        /* COLUMN_ACTION     */
                              G_TYPE_STRING,          /* COLUMN_STOCK_ID   */
                              G_TYPE_STRING,          /* COLUMN_LABEL      */
                              G_TYPE_STRING,          /* COLUMN_NAME       */
                              G_TYPE_UINT,            /* COLUMN_ACCEL_KEY  */
                              GDK_TYPE_MODIFIER_TYPE, /* COLUMN_ACCEL_MASK */
                              GTK_TYPE_MENU_ITEM);    /* COLUMN_MENU_ITEM  */

  accel_group = gtk_ui_manager_get_accel_group (GTK_UI_MANAGER (manager));

  for (list = gtk_ui_manager_get_action_groups (GTK_UI_MANAGER (manager));
       list;
       list = g_list_next (list))
    {
      GimpActionGroup *group = list->data;
      GList           *actions;
      GList           *list2;
      GtkTreeIter      group_iter;

      gtk_tree_store_append (store, &group_iter, NULL);

      gtk_tree_store_set (store, &group_iter,
                          COLUMN_STOCK_ID, group->stock_id,
                          COLUMN_LABEL,    group->label,
                          -1);

      actions = gtk_action_group_list_actions (GTK_ACTION_GROUP (group));

      actions = g_list_sort (actions, (GCompareFunc) gimp_action_name_compare);

      for (list2 = actions; list2; list2 = g_list_next (list2))
        {
          GtkAction   *action = list2->data;
          const gchar *name   = gtk_action_get_name (action);

          if (! strstr (name, "-menu") &&
              ! strstr (name, "-popup"))
            {
              GtkTreeIter      action_iter;
              gchar           *stock_id;
              gchar           *label;
              gchar           *stripped;
              guint            accel_key  = 0;
              GdkModifierType  accel_mask = 0;
              GtkWidget       *menu_item  = NULL;

              g_object_get (action,
                            "stock-id", &stock_id,
                            "label",    &label,
                            NULL);

              stripped = gimp_strip_uline (label);

              if (show_shortcuts)
                {
                  gtk_action_set_accel_group (action, accel_group);

                  menu_item = gtk_action_create_menu_item (action);

                  if (GTK_IS_MENU_ITEM (menu_item) &&
                      GTK_IS_ACCEL_LABEL (GTK_BIN (menu_item)->child))
                    {
                      GtkWidget *accel_label = GTK_BIN (menu_item)->child;
                      GClosure  *accel_closure;

                      g_object_get (accel_label,
                                    "accel-closure", &accel_closure,
                                    NULL);

                      if (accel_closure)
                        {
                          GtkAccelKey *key;

                          key = gtk_accel_group_find (accel_group,
                                                      gimp_action_view_accel_find_func,
                                                      accel_closure);

                          if (key            &&
                              key->accel_key &&
                              key->accel_flags & GTK_ACCEL_VISIBLE)
                            {
                              accel_key  = key->accel_key;
                              accel_mask = key->accel_mods;
                            }
                        }

                      g_object_ref (menu_item);
                      gtk_object_sink (GTK_OBJECT (menu_item));
                    }
                  else if (menu_item)
                    {
                      gtk_object_sink (GTK_OBJECT (menu_item));
                      menu_item = NULL;
                    }
                }

              gtk_tree_store_append (store, &action_iter, &group_iter);

              gtk_tree_store_set (store, &action_iter,
                                  COLUMN_ACTION,     action,
                                  COLUMN_STOCK_ID,   stock_id,
                                  COLUMN_LABEL,      stripped,
                                  COLUMN_NAME,       name,
                                  COLUMN_ACCEL_KEY,  accel_key,
                                  COLUMN_ACCEL_MASK, accel_mask,
                                  COLUMN_MENU_ITEM,  menu_item,
                                  -1);

              g_free (stock_id);
              g_free (label);
              g_free (stripped);

              if (menu_item)
                g_object_unref (menu_item);
            }
        }

      g_list_free (actions);
    }

  view = g_object_new (GIMP_TYPE_ACTION_VIEW,
                       "model",      store,
                       "rules-hint", TRUE,
                       NULL);

  g_object_unref (store);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, _("Action"));

  cell = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, cell, FALSE);
  gtk_tree_view_column_set_attributes (column, cell,
                                       "stock-id", COLUMN_STOCK_ID,
                                       NULL);

  cell = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, cell, TRUE);
  gtk_tree_view_column_set_attributes (column, cell,
                                       "text", COLUMN_LABEL,
                                       NULL);

  gtk_tree_view_append_column (view, column);

  if (show_shortcuts)
    {
      g_signal_connect_object (accel_group, "accel-changed",
                               G_CALLBACK (gimp_action_view_accel_changed),
                               view, 0);

      column = gtk_tree_view_column_new ();
      gtk_tree_view_column_set_title (column, _("Shortcut"));

      cell = gimp_cell_renderer_accel_new ();
      cell->mode = GTK_CELL_RENDERER_MODE_EDITABLE;
      GTK_CELL_RENDERER_TEXT (cell)->editable = TRUE;
      gtk_tree_view_column_pack_start (column, cell, TRUE);
      gtk_tree_view_column_set_attributes (column, cell,
                                           "accel-key",  COLUMN_ACCEL_KEY,
                                           "accel-mask", COLUMN_ACCEL_MASK,
                                           NULL);

      g_signal_connect (cell, "accel-edited",
                        G_CALLBACK (gimp_action_view_accel_edited),
                        view);

      gtk_tree_view_append_column (view, column);
    }
  else
    {
      column = gtk_tree_view_column_new ();
      gtk_tree_view_column_set_title (column, _("Name"));

      cell = gtk_cell_renderer_text_new ();
      gtk_tree_view_column_pack_start (column, cell, TRUE);
      gtk_tree_view_column_set_attributes (column, cell,
                                           "text", COLUMN_NAME,
                                           NULL);

      gtk_tree_view_append_column (view, column);
    }

  return GTK_WIDGET (view);
}


/*  private functions  */

static gboolean
gimp_action_view_accel_find_func (GtkAccelKey *key,
                                  GClosure    *closure,
                                  gpointer     data)
{
  return (GClosure *) data == closure;
}

static void
gimp_action_view_accel_changed (GtkAccelGroup   *accel_group,
                                guint            unused1,
                                GdkModifierType  unused2,
                                GClosure        *accel_closure,
                                GimpActionView  *view)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  gboolean      iter_valid;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  if (! model)
    return;

  for (iter_valid = gtk_tree_model_get_iter_first (model, &iter);
       iter_valid;
       iter_valid = gtk_tree_model_iter_next (model, &iter))
    {
      GtkTreeIter child_iter;
      gboolean    child_valid;

      for (child_valid = gtk_tree_model_iter_children (model, &child_iter,
                                                       &iter);
           child_valid;
           child_valid = gtk_tree_model_iter_next (model, &child_iter))
        {
          GtkWidget *menu_item;

          gtk_tree_model_get (model, &child_iter,
                              COLUMN_MENU_ITEM, &menu_item,
                              -1);

          if (menu_item)
            {
              GClosure *item_closure;

              g_object_get (GTK_BIN (menu_item)->child,
                            "accel-closure", &item_closure,
                            NULL);

              g_object_unref (menu_item);

              if (accel_closure == item_closure)
                {
                  GtkAccelKey     *key;
                  guint            accel_key  = 0;
                  GdkModifierType  accel_mask = 0;

                  key = gtk_accel_group_find (accel_group,
                                              gimp_action_view_accel_find_func,
                                              accel_closure);

                  if (key            &&
                      key->accel_key &&
                      key->accel_flags & GTK_ACCEL_VISIBLE)
                    {
                      accel_key  = key->accel_key;
                      accel_mask = key->accel_mods;
                    }

                  gtk_tree_store_set (GTK_TREE_STORE (model), &child_iter,
                                      COLUMN_ACCEL_KEY,  accel_key,
                                      COLUMN_ACCEL_MASK, accel_mask,
                                      -1);

                  return;
                }
            }
        }
    }
}

typedef struct
{
  gchar           *accel_path;
  guint            accel_key;
  GdkModifierType  accel_mask;
} ConfirmData;

static void
gimp_action_view_accel_confirm (GtkWidget   *query_box,
                                gboolean     value,
                                gpointer     data)
{
  if (value)
    {
      ConfirmData *confirm_data = data;

      if (! gtk_accel_map_change_entry (confirm_data->accel_path,
                                        confirm_data->accel_key,
                                        confirm_data->accel_mask,
                                        TRUE))
        {
          g_message (_("Changing shortcut failed."));
        }
    }
}

static void
gimp_action_view_accel_edited (GimpCellRendererAccel *accel,
                               const char            *path_string,
                               gboolean               delete,
                               guint                  accel_key,
                               GdkModifierType        accel_mask,
                               GimpActionView        *view)
{
  GtkTreeModel *model;
  GtkTreePath  *path;
  GtkTreeIter   iter;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  if (! model)
    return;

  path = gtk_tree_path_new_from_string (path_string);

  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      GtkAction      *action;
      GtkActionGroup *group;
      gchar          *accel_path;

      gtk_tree_model_get (model, &iter,
                          COLUMN_ACTION, &action,
                          -1);

      if (! action)
        goto done;

      g_object_get (action, "action-group", &group, NULL);

      if (! group)
        {
          g_object_unref (action);
          goto done;
        }

#ifdef __GNUC__
#warning FIXME: remove accel_path hack
#endif
      accel_path = g_object_get_data (G_OBJECT (action), "gimp-accel-path");

      if (accel_path)
        accel_path = g_strdup (accel_path);
      else
        accel_path = g_strdup_printf ("<Actions>/%s/%s",
                                      gtk_action_group_get_name (group),
                                      gtk_action_get_name (action));

      if (delete)
        {
          if (! gtk_accel_map_change_entry (accel_path, 0, 0, FALSE))
            {
              g_message (_("Removing shortcut failed."));
            }
        }
      else if (! accel_key)
        {
          g_message (_("Invalid shortcut."));
        }
      else
        {
          if (! gtk_accel_map_change_entry (accel_path,
                                            accel_key, accel_mask, FALSE))
            {
              GtkAction   *conflict_action = NULL;
              GtkTreeIter  iter;
              gboolean     iter_valid;

              for (iter_valid = gtk_tree_model_get_iter_first (model, &iter);
                   iter_valid;
                   iter_valid = gtk_tree_model_iter_next (model, &iter))
                {
                  GtkTreeIter child_iter;
                  gboolean    child_valid;

                  for (child_valid = gtk_tree_model_iter_children (model,
                                                                   &child_iter,
                                                                   &iter);
                       child_valid;
                       child_valid = gtk_tree_model_iter_next (model,
                                                               &child_iter))
                    {
                      guint           child_accel_key;
                      GdkModifierType child_accel_mask;

                      gtk_tree_model_get (model, &child_iter,
                                          COLUMN_ACCEL_KEY,  &child_accel_key,
                                          COLUMN_ACCEL_MASK, &child_accel_mask,
                                          -1);

                      if (accel_key  == child_accel_key &&
                          accel_mask == child_accel_mask)
                        {
                          gtk_tree_model_get (model, &child_iter,
                                              COLUMN_ACTION, &conflict_action,
                                              -1);
                          break;
                        }
                    }

                  if (conflict_action)
                    break;
                }

              if (conflict_action)
                {
                  GimpActionGroup *conflict_group;
                  gchar           *label;
                  gchar           *stripped;
                  gchar           *accel_string;
                  gchar           *message;
                  ConfirmData     *confirm_data;
                  GtkWidget       *query_box;

                  g_object_get (conflict_action,
                                "action-group", &conflict_group,
                                "label",        &label,
                                NULL);

                  stripped = gimp_strip_uline (label);

                  accel_string = gimp_get_accel_string (accel_key, accel_mask);

                  message =
                    g_strdup_printf ("Shortcut \"%s\" is already taken by "
                                     "\"%s\" from the \"%s\" group.\n"
                                     "\n"
                                     "Click \"Delete Old Shortcut\" to "
                                     "assign the shortcut anyway, "
                                     "deleting %s's shortcut.",
                                     accel_string,
                                     stripped,
                                     conflict_group->label,
                                     stripped);

                  confirm_data = g_new0 (ConfirmData, 1);

                  confirm_data->accel_path = g_strdup (accel_path);
                  confirm_data->accel_key  = accel_key;
                  confirm_data->accel_mask = accel_mask;

                  query_box =
                    gimp_query_boolean_box (_("Conflicting Shortcuts"),
                                            gtk_widget_get_toplevel (GTK_WIDGET (view)),
                                            gimp_standard_help_func,
                                            NULL,
                                            GIMP_STOCK_WARNING,
                                            message,
                                            _("Delete Old Shortcut"),
                                            GTK_STOCK_CANCEL,
                                            G_OBJECT (view), "destroy",
                                            gimp_action_view_accel_confirm,
                                            confirm_data);

                  g_object_weak_ref (G_OBJECT (query_box),
                                     (GWeakNotify) g_free,
                                     confirm_data);
                  g_object_weak_ref (G_OBJECT (query_box),
                                     (GWeakNotify) g_free,
                                     confirm_data->accel_path);

                  g_free (label);
                  g_free (stripped);
                  g_free (accel_string);
                  g_free (message);
                  g_object_unref (conflict_action);
                  g_object_unref (conflict_group);

                  gtk_widget_show (query_box);
                }
              else
                {
                  g_message (_("Changing shortcut failed."));
                }
            }
        }

      g_free (accel_path);
      g_object_unref (group);
      g_object_unref (action);
    }

 done:

  gtk_tree_path_free (path);
}
