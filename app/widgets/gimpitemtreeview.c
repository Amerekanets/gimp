/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpitemtreeview.c
 * Copyright (C) 2001-2003 Michael Natterer <mitch@gimp.org>
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

#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "widgets-types.h"

#include "core/gimpchannel.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpimage-undo-push.h"
#include "core/gimpitemundo.h"
#include "core/gimplayer.h"
#include "core/gimpmarshal.h"
#include "core/gimpundostack.h"

#include "vectors/gimpvectors.h"

#include "gimpchanneltreeview.h"
#include "gimpcellrenderertoggle.h"
#include "gimpdnd.h"
#include "gimpdocked.h"
#include "gimpitemtreeview.h"
#include "gimpitemfactory.h"
#include "gimplayertreeview.h"
#include "gimpmenufactory.h"
#include "gimppreviewrenderer.h"
#include "gimpvectorstreeview.h"
#include "gimpwidgets-utils.h"

#include "gimp-intl.h"


enum
{
  PROP_0,
  PROP_ITEM_TYPE,
  PROP_SIGNAL_NAME
};

enum
{
  SET_IMAGE,
  LAST_SIGNAL
};


static void   gimp_item_tree_view_class_init   (GimpItemTreeViewClass *klass);
static void   gimp_item_tree_view_init         (GimpItemTreeView      *view,
                                                GimpItemTreeViewClass *view_class);

static void  gimp_item_tree_view_docked_iface_init  (GimpDockedInterface *docked_iface);
static void  gimp_item_tree_view_set_docked_context (GimpDocked        *docked,
                                                     GimpContext       *context,
                                                     GimpContext       *prev_context);

static GObject * gimp_item_tree_view_constructor    (GType              type,
                                                     guint              n_params,
                                                     GObjectConstructParam *params);
static void   gimp_item_tree_view_set_property      (GObject           *object,
                                                     guint              property_id,
                                                     const GValue      *value,
                                                     GParamSpec        *pspec);
static void   gimp_item_tree_view_get_property      (GObject           *object,
                                                     guint              property_id,
                                                     GValue            *value,
                                                     GParamSpec        *pspec);
static void   gimp_item_tree_view_destroy           (GtkObject         *object);

static void   gimp_item_tree_view_real_set_image    (GimpItemTreeView  *view,
                                                     GimpImage         *gimage);

static void   gimp_item_tree_view_set_container     (GimpContainerView *view,
                                                     GimpContainer     *container);
static gpointer gimp_item_tree_view_insert_item     (GimpContainerView *view,
                                                     GimpViewable      *viewable,
                                                     gint               index);
static gboolean gimp_item_tree_view_select_item     (GimpContainerView *view,
                                                     GimpViewable      *item,
                                                     gpointer           insert_data);
static void   gimp_item_tree_view_activate_item     (GimpContainerView *view,
                                                     GimpViewable      *item,
                                                     gpointer           insert_data);
static void   gimp_item_tree_view_context_item      (GimpContainerView *view,
                                                     GimpViewable      *item,
                                                     gpointer           insert_data);

static gboolean gimp_item_tree_view_drop_possible   (GimpContainerTreeView *view,
                                                     GimpViewable      *src_viewable,
                                                     GimpViewable      *dest_viewable,
                                                     GtkTreeViewDropPosition  drop_pos,
                                                     GdkDragAction     *drag_action);
static void     gimp_item_tree_view_drop            (GimpContainerTreeView *view,
                                                     GimpViewable      *src_viewable,
                                                     GimpViewable      *dest_viewable,
                                                     GtkTreeViewDropPosition  drop_pos);

static void   gimp_item_tree_view_edit_clicked      (GtkWidget         *widget,
                                                     GimpItemTreeView  *view);

static void   gimp_item_tree_view_new_clicked       (GtkWidget         *widget,
                                                     GimpItemTreeView  *view);
static void   gimp_item_tree_view_new_extended_clicked
                                                    (GtkWidget         *widget,
                                                     guint              state,
                                                     GimpItemTreeView  *view);
static void   gimp_item_tree_view_new_dropped       (GtkWidget         *widget,
                                                     GimpViewable      *viewable,
                                                     gpointer           data);

static void   gimp_item_tree_view_raise_clicked     (GtkWidget         *widget,
                                                     GimpItemTreeView  *view);
static void   gimp_item_tree_view_raise_extended_clicked
                                                    (GtkWidget         *widget,
                                                     guint              state,
                                                     GimpItemTreeView  *view);
static void   gimp_item_tree_view_lower_clicked     (GtkWidget         *widget,
                                                     GimpItemTreeView  *view);
static void   gimp_item_tree_view_lower_extended_clicked
                                                    (GtkWidget         *widget,
                                                     guint              state,
                                                     GimpItemTreeView  *view);

static void   gimp_item_tree_view_duplicate_clicked (GtkWidget         *widget,
                                                     GimpItemTreeView  *view);
static void   gimp_item_tree_view_delete_clicked    (GtkWidget         *widget,
                                                     GimpItemTreeView  *view);

static void   gimp_item_tree_view_item_changed      (GimpImage         *gimage,
                                                     GimpItemTreeView  *view);
static void   gimp_item_tree_view_size_changed      (GimpImage         *gimage,
                                                     GimpItemTreeView  *view);

static void   gimp_item_tree_view_name_edited       (GtkCellRendererText *cell,
                                                     const gchar       *path,
                                                     const gchar       *name,
                                                     GimpItemTreeView  *view);

static void   gimp_item_tree_view_visible_changed   (GimpItem          *item,
                                                     GimpItemTreeView  *view);
static void   gimp_item_tree_view_linked_changed    (GimpItem          *item,
                                                     GimpItemTreeView  *view);

static void   gimp_item_tree_view_eye_clicked       (GtkCellRendererToggle *toggle,
                                                     gchar             *path,
                                                     GdkModifierType    state,
                                                     GimpItemTreeView  *view);
static void   gimp_item_tree_view_chain_clicked     (GtkCellRendererToggle *toggle,
                                                     gchar             *path,
                                                     GdkModifierType    state,
                                                     GimpItemTreeView  *view);

/*  utility function to avoid code duplication  */
static void   gimp_item_tree_view_toggle_clicked    (GtkCellRendererToggle *toggle,
                                                     gchar             *path_str,
                                                     GdkModifierType    state,
                                                     GimpItemTreeView  *view,
                                                     GimpUndoType       undo_type);


static guint  view_signals[LAST_SIGNAL] = { 0 };

static GimpContainerTreeViewClass *parent_class = NULL;


GType
gimp_item_tree_view_get_type (void)
{
  static GType type = 0;

  if (! type)
    {
      static const GTypeInfo view_info =
      {
        sizeof (GimpItemTreeViewClass),
        NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) gimp_item_tree_view_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GimpItemTreeView),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gimp_item_tree_view_init,
      };
      static const GInterfaceInfo docked_iface_info =
      {
        (GInterfaceInitFunc) gimp_item_tree_view_docked_iface_init,
        NULL,           /* iface_finalize */
        NULL            /* iface_data     */
      };

      type = g_type_register_static (GIMP_TYPE_CONTAINER_TREE_VIEW,
                                     "GimpItemTreeView",
                                     &view_info, 0);

      g_type_add_interface_static (type, GIMP_TYPE_DOCKED,
                                   &docked_iface_info);
    }

  return type;
}

static void
gimp_item_tree_view_class_init (GimpItemTreeViewClass *klass)
{
  GObjectClass               *object_class;
  GtkObjectClass             *gtk_object_class;
  GimpContainerViewClass     *container_view_class;
  GimpContainerTreeViewClass *tree_view_class;

  object_class         = G_OBJECT_CLASS (klass);
  gtk_object_class     = GTK_OBJECT_CLASS (klass);
  container_view_class = GIMP_CONTAINER_VIEW_CLASS (klass);
  tree_view_class      = GIMP_CONTAINER_TREE_VIEW_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  view_signals[SET_IMAGE] =
    g_signal_new ("set_image",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GimpItemTreeViewClass, set_image),
		  NULL, NULL,
		  gimp_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GIMP_TYPE_OBJECT);

  object_class->constructor           = gimp_item_tree_view_constructor;
  object_class->set_property          = gimp_item_tree_view_set_property;
  object_class->get_property          = gimp_item_tree_view_get_property;

  gtk_object_class->destroy           = gimp_item_tree_view_destroy;

  container_view_class->set_container = gimp_item_tree_view_set_container;
  container_view_class->insert_item   = gimp_item_tree_view_insert_item;
  container_view_class->select_item   = gimp_item_tree_view_select_item;
  container_view_class->activate_item = gimp_item_tree_view_activate_item;
  container_view_class->context_item  = gimp_item_tree_view_context_item;

  tree_view_class->drop_possible      = gimp_item_tree_view_drop_possible;
  tree_view_class->drop               = gimp_item_tree_view_drop;

  klass->set_image                    = gimp_item_tree_view_real_set_image;

  klass->get_container                = NULL;
  klass->get_active_item              = NULL;
  klass->set_active_item              = NULL;
  klass->reorder_item                 = NULL;
  klass->add_item                     = NULL;
  klass->remove_item                  = NULL;

  klass->edit_desc                    = NULL;
  klass->edit_help_id                 = NULL;
  klass->new_desc                     = NULL;
  klass->new_help_id                  = NULL;
  klass->duplicate_desc               = NULL;
  klass->duplicate_help_id            = NULL;
  klass->delete_desc                  = NULL;
  klass->delete_help_id               = NULL;
  klass->raise_desc                   = NULL;
  klass->raise_help_id                = NULL;
  klass->raise_to_top_desc            = NULL;
  klass->raise_to_top_help_id         = NULL;
  klass->lower_desc                   = NULL;
  klass->lower_help_id                = NULL;
  klass->lower_to_bottom_desc         = NULL;
  klass->lower_to_bottom_help_id      = NULL;

  g_object_class_install_property (object_class,
				   PROP_ITEM_TYPE,
				   g_param_spec_pointer ("item-type",
                                                         NULL, NULL,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class,
				   PROP_SIGNAL_NAME,
				   g_param_spec_string ("signal-name",
                                                        NULL, NULL,
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY));
}

static void
gimp_item_tree_view_init (GimpItemTreeView      *view,
                          GimpItemTreeViewClass *view_class)
{
  GimpContainerTreeView *tree_view;
  GimpEditor            *editor;
  gchar                 *str;

  tree_view = GIMP_CONTAINER_TREE_VIEW (view);
  editor    = GIMP_EDITOR (view);

  /* The following used to read:
   *
   * tree_view->model_columns[tree_view->n_model_columns++] = ...
   *
   * but combining the two lead to gcc miscompiling the function on ppc/ia64
   * (model_column_mask and model_column_mask_visible would have the same
   * value, probably due to bad instruction reordering). See bug #113144 for
   * more info.
   */
  view->model_column_visible = tree_view->n_model_columns;
  tree_view->model_columns[tree_view->n_model_columns] = G_TYPE_BOOLEAN;
  tree_view->n_model_columns++;

  view->model_column_linked = tree_view->n_model_columns;
  tree_view->model_columns[tree_view->n_model_columns] = G_TYPE_BOOLEAN;
  tree_view->n_model_columns++;

  view->gimage      = NULL;
  view->item_type   = G_TYPE_NONE;
  view->signal_name = NULL;

  view->edit_button =
    gimp_editor_add_button (editor,
                            GIMP_STOCK_EDIT, view_class->edit_desc,
                            view_class->edit_help_id,
                            G_CALLBACK (gimp_item_tree_view_edit_clicked),
                            NULL,
                            view);

  str = g_strdup_printf (view_class->new_desc,
                         gimp_get_mod_name_shift ());

  view->new_button =
    gimp_editor_add_button (editor,
                            GTK_STOCK_NEW, str, view_class->new_help_id,
                            G_CALLBACK (gimp_item_tree_view_new_clicked),
                            G_CALLBACK (gimp_item_tree_view_new_extended_clicked),
                            view);

  g_free (str);

  str = g_strdup_printf (_("%s\n"
                           "%s  To Top"),
                         view_class->raise_desc,
                         gimp_get_mod_name_shift ());

  view->raise_button =
    gimp_editor_add_button (editor,
                            GTK_STOCK_GO_UP, str, view_class->raise_help_id,
                            G_CALLBACK (gimp_item_tree_view_raise_clicked),
                            G_CALLBACK (gimp_item_tree_view_raise_extended_clicked),
                            view);

  g_free (str);

  str = g_strdup_printf (_("%s\n"
                           "%s  To Bottom"),
                         view_class->lower_desc,
                         gimp_get_mod_name_shift ());

  view->lower_button =
    gimp_editor_add_button (editor,
                            GTK_STOCK_GO_DOWN, str, view_class->lower_help_id,
                            G_CALLBACK (gimp_item_tree_view_lower_clicked),
                            G_CALLBACK (gimp_item_tree_view_lower_extended_clicked),
                            view);

  g_free (str);

  view->duplicate_button =
    gimp_editor_add_button (editor,
                            GIMP_STOCK_DUPLICATE, view_class->duplicate_desc,
                            view_class->duplicate_help_id,
                            G_CALLBACK (gimp_item_tree_view_duplicate_clicked),
                            NULL,
                            view);

  view->delete_button =
    gimp_editor_add_button (editor,
                            GTK_STOCK_DELETE, view_class->delete_desc,
                            view_class->delete_help_id,
                            G_CALLBACK (gimp_item_tree_view_delete_clicked),
                            NULL,
                            view);

  gtk_widget_set_sensitive (view->edit_button,      FALSE);
  gtk_widget_set_sensitive (view->new_button,       FALSE);
  gtk_widget_set_sensitive (view->raise_button,     FALSE);
  gtk_widget_set_sensitive (view->lower_button,     FALSE);
  gtk_widget_set_sensitive (view->duplicate_button, FALSE);
  gtk_widget_set_sensitive (view->delete_button,    FALSE);

  view->visible_changed_handler_id = 0;
  view->linked_changed_handler_id  = 0;
}

static void
gimp_item_tree_view_docked_iface_init (GimpDockedInterface *docked_iface)
{
  docked_iface->get_preview = NULL;
  docked_iface->set_context = gimp_item_tree_view_set_docked_context;
}

static void
gimp_item_tree_view_docked_context_changed (GimpContext      *context,
                                            GimpImage        *gimage,
                                            GimpItemTreeView *view)
{
  gimp_item_tree_view_set_image (view, gimage);
}

static void
gimp_item_tree_view_set_docked_context (GimpDocked  *docked,
                                        GimpContext *context,
                                        GimpContext *prev_context)
{
  GimpItemTreeView *view   = GIMP_ITEM_TREE_VIEW (docked);
  GimpImage        *gimage = NULL;

  if (prev_context)
    g_signal_handlers_disconnect_by_func (prev_context,
                                          gimp_item_tree_view_docked_context_changed,
                                          view);

  if (context)
    {
      g_signal_connect (context, "image_changed",
                        G_CALLBACK (gimp_item_tree_view_docked_context_changed),
                        view);

      gimage = gimp_context_get_image (context);
    }

  gimp_item_tree_view_set_image (view, gimage);
}

static GObject *
gimp_item_tree_view_constructor (GType                  type,
                                 guint                  n_params,
                                 GObjectConstructParam *params)
{
  GimpContainerTreeView *tree_view;
  GimpItemTreeView      *item_view;
  GObject               *object;
  GtkTreeViewColumn     *column;

  object = G_OBJECT_CLASS (parent_class)->constructor (type, n_params, params);

  tree_view = GIMP_CONTAINER_TREE_VIEW (object);
  item_view = GIMP_ITEM_TREE_VIEW (object);

  tree_view->name_cell->mode = GTK_CELL_RENDERER_MODE_EDITABLE;
  GTK_CELL_RENDERER_TEXT (tree_view->name_cell)->editable = TRUE;

  tree_view->editable_cells = g_list_prepend (tree_view->editable_cells,
                                              tree_view->name_cell);

  g_signal_connect (tree_view->name_cell, "edited",
                    G_CALLBACK (gimp_item_tree_view_name_edited),
                    item_view);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_insert_column (tree_view->view, column, 0);

  item_view->eye_cell = gimp_cell_renderer_toggle_new (GIMP_STOCK_VISIBLE);
  gtk_tree_view_column_pack_start (column, item_view->eye_cell, FALSE);
  gtk_tree_view_column_set_attributes (column, item_view->eye_cell,
                                       "active",
                                       item_view->model_column_visible,
                                       NULL);

  tree_view->toggle_cells = g_list_prepend (tree_view->toggle_cells,
                                            item_view->eye_cell);

  g_signal_connect (item_view->eye_cell, "clicked",
                    G_CALLBACK (gimp_item_tree_view_eye_clicked),
                    item_view);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_insert_column (tree_view->view, column, 1);

  item_view->chain_cell = gimp_cell_renderer_toggle_new (GIMP_STOCK_LINKED);
  gtk_tree_view_column_pack_start (column, item_view->chain_cell, FALSE);
  gtk_tree_view_column_set_attributes (column, item_view->chain_cell,
                                       "active",
                                       item_view->model_column_linked,
                                       NULL);

  tree_view->toggle_cells = g_list_prepend (tree_view->toggle_cells,
                                            item_view->chain_cell);

  g_signal_connect (item_view->chain_cell, "clicked",
                    G_CALLBACK (gimp_item_tree_view_chain_clicked),
                    item_view);

  /*  disable the default GimpContainerView drop handler  */
  GIMP_CONTAINER_VIEW (tree_view)->dnd_widget = NULL;

  gimp_dnd_drag_dest_set_by_type (GTK_WIDGET (tree_view->view),
                                  GTK_DEST_DEFAULT_ALL,
                                  item_view->item_type,
                                  GDK_ACTION_MOVE | GDK_ACTION_COPY);

  /*  connect "drop to new" manually as it makes a difference whether
   *  it was clicked or dropped
   */
  gimp_dnd_viewable_dest_add (item_view->new_button,
			      item_view->item_type,
			      gimp_item_tree_view_new_dropped,
			      item_view);

  gimp_container_view_enable_dnd (GIMP_CONTAINER_VIEW (item_view),
				  GTK_BUTTON (item_view->edit_button),
				  item_view->item_type);
  gimp_container_view_enable_dnd (GIMP_CONTAINER_VIEW (item_view),
				  GTK_BUTTON (item_view->duplicate_button),
				  item_view->item_type);
  gimp_container_view_enable_dnd (GIMP_CONTAINER_VIEW (item_view),
				  GTK_BUTTON (item_view->delete_button),
				  item_view->item_type);

  return object;
}

static void
gimp_item_tree_view_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GimpItemTreeView *view = GIMP_ITEM_TREE_VIEW (object);

  switch (property_id)
    {
    case PROP_ITEM_TYPE:
      view->item_type = (GType) g_value_get_pointer (value);
      break;
    case PROP_SIGNAL_NAME:
      view->signal_name = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_item_tree_view_get_property (GObject    *object,
                                  guint       property_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GimpItemTreeView *view = GIMP_ITEM_TREE_VIEW (object);

  switch (property_id)
    {
    case PROP_ITEM_TYPE:
      g_value_set_pointer (value, (gpointer) view->item_type);
      break;
    case PROP_SIGNAL_NAME:
      g_value_set_string (value, view->signal_name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_item_tree_view_destroy (GtkObject *object)
{
  GimpItemTreeView *view = GIMP_ITEM_TREE_VIEW (object);

  if (view->gimage)
    gimp_item_tree_view_set_image (view, NULL);

  if (view->signal_name)
    {
      g_free (view->signal_name);
      view->signal_name = NULL;
    }

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

GtkWidget *
gimp_item_tree_view_new (gint                  preview_size,
                         gint                  preview_border_width,
                         GimpImage            *gimage,
                         GType                 item_type,
                         const gchar          *signal_name,
                         GimpEditItemFunc      edit_item_func,
                         GimpNewItemFunc       new_item_func,
                         GimpActivateItemFunc  activate_item_func,
                         GimpMenuFactory      *menu_factory,
                         const gchar          *menu_identifier)
{
  GimpItemTreeView      *item_view;
  GimpContainerView     *view;
  GimpContainerTreeView *tree_view;
  GType                  view_type;

  g_return_val_if_fail (preview_size >  0 &&
			preview_size <= GIMP_VIEWABLE_MAX_PREVIEW_SIZE, NULL);
  g_return_val_if_fail (preview_border_width >= 0 &&
                        preview_border_width <= GIMP_PREVIEW_MAX_BORDER_WIDTH,
                        NULL);
  g_return_val_if_fail (gimage == NULL || GIMP_IS_IMAGE (gimage), NULL);
  g_return_val_if_fail (signal_name != NULL, NULL);
  g_return_val_if_fail (edit_item_func != NULL, NULL);
  g_return_val_if_fail (new_item_func != NULL, NULL);
  g_return_val_if_fail (activate_item_func != NULL, NULL);
  g_return_val_if_fail (GIMP_IS_MENU_FACTORY (menu_factory), NULL);
  g_return_val_if_fail (menu_identifier != NULL, NULL);

  if (item_type == GIMP_TYPE_LAYER)
    {
      view_type = GIMP_TYPE_LAYER_TREE_VIEW;
    }
  else if (item_type == GIMP_TYPE_CHANNEL)
    {
      view_type = GIMP_TYPE_CHANNEL_TREE_VIEW;
    }
  else if (item_type == GIMP_TYPE_VECTORS)
    {
      view_type = GIMP_TYPE_VECTORS_TREE_VIEW;
    }
  else
    {
      g_warning ("gimp_item_tree_view_new: unsupported item type '%s'\n",
                 g_type_name (item_type));
      return NULL;
    }

  item_view = g_object_new (view_type,
                            "item-type",   item_type,
                            "signal-name", signal_name,
                            NULL);

  view      = GIMP_CONTAINER_VIEW (item_view);
  tree_view = GIMP_CONTAINER_TREE_VIEW (item_view);

  gimp_container_view_construct (GIMP_CONTAINER_VIEW (item_view),
                                 NULL, NULL,
                                 preview_size, preview_border_width, TRUE);

  item_view->edit_item_func     = edit_item_func;
  item_view->new_item_func      = new_item_func;
  item_view->activate_item_func = activate_item_func;

  gimp_editor_create_menu (GIMP_EDITOR (item_view),
                           menu_factory, menu_identifier, item_view);

  gimp_item_tree_view_set_image (item_view, gimage);

  return GTK_WIDGET (item_view);
}

void
gimp_item_tree_view_set_image (GimpItemTreeView *view,
                               GimpImage        *gimage)
{
  g_return_if_fail (GIMP_IS_ITEM_TREE_VIEW (view));
  g_return_if_fail (! gimage || GIMP_IS_IMAGE (gimage));

  g_signal_emit (view, view_signals[SET_IMAGE], 0, gimage);
}

static void
gimp_item_tree_view_real_set_image (GimpItemTreeView *view,
                                    GimpImage        *gimage)
{
  g_return_if_fail (GIMP_IS_ITEM_TREE_VIEW (view));
  g_return_if_fail (! gimage || GIMP_IS_IMAGE (gimage));

  if (view->gimage == gimage)
    return;

  if (view->gimage)
    {
      g_signal_handlers_disconnect_by_func (view->gimage,
					    gimp_item_tree_view_item_changed,
					    view);
      g_signal_handlers_disconnect_by_func (view->gimage,
					    gimp_item_tree_view_size_changed,
					    view);

      gimp_container_view_set_container (GIMP_CONTAINER_VIEW (view), NULL);
    }

  view->gimage = gimage;

  if (view->gimage)
    {
      GimpContainer *container;

      container =
        GIMP_ITEM_TREE_VIEW_GET_CLASS (view)->get_container (view->gimage);

      gimp_container_view_set_container (GIMP_CONTAINER_VIEW (view), container);

      g_signal_connect (view->gimage, view->signal_name,
			G_CALLBACK (gimp_item_tree_view_item_changed),
			view);
      g_signal_connect (view->gimage, "size_changed",
			G_CALLBACK (gimp_item_tree_view_size_changed),
			view);

      gimp_item_tree_view_item_changed (view->gimage, view);
    }

  gtk_widget_set_sensitive (view->new_button, (view->gimage != NULL));
}


/*  GimpContainerView methods  */

static void
gimp_item_tree_view_set_container (GimpContainerView *view,
                                   GimpContainer     *container)
{
  GimpItemTreeView *item_view;

  item_view = GIMP_ITEM_TREE_VIEW (view);

  if (view->container)
    {
      gimp_container_remove_handler (view->container,
				     item_view->visible_changed_handler_id);
      gimp_container_remove_handler (view->container,
				     item_view->linked_changed_handler_id);

      item_view->visible_changed_handler_id = 0;
      item_view->linked_changed_handler_id  = 0;
    }

  GIMP_CONTAINER_VIEW_CLASS (parent_class)->set_container (view, container);

  if (view->container)
    {
      item_view->visible_changed_handler_id =
	gimp_container_add_handler (view->container, "visibility_changed",
				    G_CALLBACK (gimp_item_tree_view_visible_changed),
				    view);
      item_view->linked_changed_handler_id =
	gimp_container_add_handler (view->container, "linked_changed",
				    G_CALLBACK (gimp_item_tree_view_linked_changed),
				    view);
    }
}

static gpointer
gimp_item_tree_view_insert_item (GimpContainerView *view,
                                 GimpViewable      *viewable,
                                 gint               index)
{
  GimpContainerTreeView *tree_view;
  GimpItemTreeView      *item_view;
  GimpItem              *item;
  GtkTreeIter           *iter;

  tree_view = GIMP_CONTAINER_TREE_VIEW (view);
  item_view = GIMP_ITEM_TREE_VIEW (view);

  iter = GIMP_CONTAINER_VIEW_CLASS (parent_class)->insert_item (view, viewable,
                                                                index);

  item = GIMP_ITEM (viewable);

  gtk_list_store_set (GTK_LIST_STORE (tree_view->model), iter,
                      item_view->model_column_visible,
                      gimp_item_get_visible (item),
                      item_view->model_column_linked,
                      gimp_item_get_linked (item),
                      -1);

  return iter;
}

static gboolean
gimp_item_tree_view_select_item (GimpContainerView *view,
                                 GimpViewable      *item,
                                 gpointer           insert_data)
{
  GimpItemTreeView *tree_view;
  gboolean          edit_sensitive      = FALSE;
  gboolean          raise_sensitive     = FALSE;
  gboolean          lower_sensitive     = FALSE;
  gboolean          duplicate_sensitive = FALSE;
  gboolean          delete_sensitive    = FALSE;
  gboolean          success;

  tree_view = GIMP_ITEM_TREE_VIEW (view);

  success = GIMP_CONTAINER_VIEW_CLASS (parent_class)->select_item (view, item,
                                                                   insert_data);

  if (item)
    {
      GimpItemTreeViewClass *item_view_class;
      GimpItem              *active_item;
      gint                   index;

      item_view_class = GIMP_ITEM_TREE_VIEW_GET_CLASS (tree_view);

      active_item = item_view_class->get_active_item (tree_view->gimage);

      if (active_item != (GimpItem *) item)
	{
	  item_view_class->set_active_item (tree_view->gimage,
                                            GIMP_ITEM (item));

	  gimp_image_flush (tree_view->gimage);
	}

      index = gimp_container_get_child_index (view->container,
					      GIMP_OBJECT (item));

      if (view->container->num_children > 1)
	{
	  if (index > 0)
	    raise_sensitive = TRUE;

	  if (index < (view->container->num_children - 1))
	    lower_sensitive = TRUE;
	}

      edit_sensitive      = TRUE;
      duplicate_sensitive = TRUE;
      delete_sensitive    = TRUE;
    }

  gtk_widget_set_sensitive (tree_view->edit_button,      edit_sensitive);
  gtk_widget_set_sensitive (tree_view->raise_button,     raise_sensitive);
  gtk_widget_set_sensitive (tree_view->lower_button,     lower_sensitive);
  gtk_widget_set_sensitive (tree_view->duplicate_button, duplicate_sensitive);
  gtk_widget_set_sensitive (tree_view->delete_button,    delete_sensitive);

  return success;
}

static void
gimp_item_tree_view_activate_item (GimpContainerView *view,
                                   GimpViewable      *item,
                                   gpointer           insert_data)
{
  GimpItemTreeView *item_view;

  item_view = GIMP_ITEM_TREE_VIEW (view);

  if (GIMP_CONTAINER_VIEW_CLASS (parent_class)->activate_item)
    GIMP_CONTAINER_VIEW_CLASS (parent_class)->activate_item (view,
							     item,
							     insert_data);

  item_view->activate_item_func (GIMP_ITEM (item), GTK_WIDGET (view));
}

static void
gimp_item_tree_view_context_item (GimpContainerView *view,
                                  GimpViewable      *item,
                                  gpointer           insert_data)
{
  GimpEditor *editor;

  if (GIMP_CONTAINER_VIEW_CLASS (parent_class)->context_item)
    GIMP_CONTAINER_VIEW_CLASS (parent_class)->context_item (view,
							    item,
							    insert_data);

  editor = GIMP_EDITOR (view);

  if (editor->item_factory)
    gimp_item_factory_popup_with_data (editor->item_factory,
                                       editor->item_factory_data,
                                       GTK_WIDGET (editor),
                                       NULL, NULL, NULL);
}

static gboolean
gimp_item_tree_view_drop_possible (GimpContainerTreeView   *tree_view,
                                   GimpViewable            *src_viewable,
                                   GimpViewable            *dest_viewable,
                                   GtkTreeViewDropPosition  drop_pos,
                                   GdkDragAction           *drag_action)
{
  GimpItemTreeView *item_view;

  item_view = GIMP_ITEM_TREE_VIEW (tree_view);

  if (gimp_item_get_image (GIMP_ITEM (src_viewable)) !=
      gimp_item_get_image (GIMP_ITEM (dest_viewable)))
    {
      if (drag_action)
        *drag_action = GDK_ACTION_COPY;

      return TRUE;
    }

  return GIMP_CONTAINER_TREE_VIEW_CLASS (parent_class)->drop_possible (tree_view,
                                                                       src_viewable,
                                                                       dest_viewable,
                                                                       drop_pos,
                                                                       drag_action);
}

static void
gimp_item_tree_view_drop (GimpContainerTreeView   *tree_view,
                          GimpViewable            *src_viewable,
                          GimpViewable            *dest_viewable,
                          GtkTreeViewDropPosition  drop_pos)
{
  GimpContainerView     *container_view;
  GimpItemTreeView      *item_view;
  GimpItemTreeViewClass *item_view_class;
  GimpObject            *src_object;
  GimpObject            *dest_object;
  gint                   src_index;
  gint                   dest_index;

  container_view = GIMP_CONTAINER_VIEW (tree_view);
  item_view      = GIMP_ITEM_TREE_VIEW (tree_view);

  src_object  = GIMP_OBJECT (src_viewable);
  dest_object = GIMP_OBJECT (dest_viewable);

  src_index  = gimp_container_get_child_index (container_view->container,
                                               src_object);
  dest_index = gimp_container_get_child_index (container_view->container,
                                               dest_object);

  item_view_class = GIMP_ITEM_TREE_VIEW_GET_CLASS (item_view);

  if (item_view->gimage != gimp_item_get_image (GIMP_ITEM (src_viewable)))
    {
      GimpItem *new_item;

      if (drop_pos == GTK_TREE_VIEW_DROP_AFTER)
        dest_index++;

      new_item = gimp_item_convert (GIMP_ITEM (src_viewable),
                                    item_view->gimage,
                                    G_TYPE_FROM_INSTANCE (src_viewable),
                                    TRUE);

      item_view_class->add_item (item_view->gimage,
                                 new_item,
                                 dest_index);
    }
  else
    {
      if (drop_pos == GTK_TREE_VIEW_DROP_AFTER && src_index > dest_index)
        {
          dest_index++;
        }
      else if (drop_pos == GTK_TREE_VIEW_DROP_BEFORE && src_index < dest_index)
        {
          dest_index--;
        }

      item_view_class->reorder_item (item_view->gimage,
                                     GIMP_ITEM (src_object),
                                     dest_index,
                                     TRUE,
                                     item_view_class->reorder_desc);
    }

  gimp_image_flush (item_view->gimage);
}


/*  "Edit" functions  */

static void
gimp_item_tree_view_edit_clicked (GtkWidget        *widget,
                                  GimpItemTreeView *view)
{
  GimpItem *item;

  item = GIMP_ITEM_TREE_VIEW_GET_CLASS (view)->get_active_item (view->gimage);

  if (item)
    view->edit_item_func (item, GTK_WIDGET (view));
}


/*  "New" functions  */

static void
gimp_item_tree_view_new_clicked (GtkWidget        *widget,
                                 GimpItemTreeView *view)
{
  view->new_item_func (view->gimage, NULL, FALSE, GTK_WIDGET (view));
  gimp_image_flush (view->gimage);
}

static void
gimp_item_tree_view_new_extended_clicked (GtkWidget        *widget,
                                          guint             state,
                                          GimpItemTreeView *view)
{
  view->new_item_func (view->gimage, NULL, TRUE, GTK_WIDGET (view));
}

static void
gimp_item_tree_view_new_dropped (GtkWidget    *widget,
                                 GimpViewable *viewable,
                                 gpointer      data)
{
  GimpItemTreeView *view;

  view = GIMP_ITEM_TREE_VIEW (data);

  if (viewable && gimp_container_have (GIMP_CONTAINER_VIEW (view)->container,
				       GIMP_OBJECT (viewable)))
    {
      view->new_item_func (view->gimage, GIMP_ITEM (viewable), FALSE,
                           GTK_WIDGET (view));

      gimp_image_flush (view->gimage);
    }
}


/*  "Duplicate" functions  */

static void
gimp_item_tree_view_duplicate_clicked (GtkWidget        *widget,
                                       GimpItemTreeView *view)
{
  GimpItemTreeViewClass *item_view_class;
  GimpItem              *item;

  item_view_class = GIMP_ITEM_TREE_VIEW_GET_CLASS (view);

  item = item_view_class->get_active_item (view->gimage);

  if (item)
    {
      GimpItem *new_item;

      new_item = gimp_item_duplicate (item,
                                      G_TYPE_FROM_INSTANCE (item),
                                      TRUE);

      if (new_item)
        {
          item_view_class->add_item (view->gimage, new_item, -1);

          gimp_image_flush (view->gimage);
        }
    }
}


/*  "Raise/Lower" functions  */

static void
gimp_item_tree_view_raise_clicked (GtkWidget        *widget,
                                   GimpItemTreeView *view)
{
  GimpItemTreeViewClass *item_view_class;
  GimpItem              *item;

  item_view_class = GIMP_ITEM_TREE_VIEW_GET_CLASS (view);

  item = item_view_class->get_active_item (view->gimage);

  if (item)
    {
      GimpContainer *container;
      gint           index;

      container = GIMP_CONTAINER_VIEW (view)->container;

      index = gimp_container_get_child_index (container, GIMP_OBJECT (item));

      if (index > 0)
        {
          item_view_class->reorder_item (view->gimage, item, index - 1, TRUE,
                                         item_view_class->raise_desc);

          gimp_image_flush (view->gimage);
        }
    }
}

static void
gimp_item_tree_view_raise_extended_clicked (GtkWidget        *widget,
                                            guint             state,
                                            GimpItemTreeView *view)
{
  GimpItemTreeViewClass *item_view_class;
  GimpItem              *item;

  item_view_class = GIMP_ITEM_TREE_VIEW_GET_CLASS (view);

  item = item_view_class->get_active_item (view->gimage);

  if (item)
    {
      GimpContainer *container;
      gint           index;

      container = GIMP_CONTAINER_VIEW (view)->container;

      index = gimp_container_get_child_index (container, GIMP_OBJECT (item));

      if ((state & GDK_SHIFT_MASK) && (index > 0))
        {
          item_view_class->reorder_item (view->gimage, item, 0, TRUE,
                                         item_view_class->raise_to_top_desc);

          gimp_image_flush (view->gimage);
        }
    }
}

static void
gimp_item_tree_view_lower_clicked (GtkWidget        *widget,
                                   GimpItemTreeView *view)
{
  GimpItemTreeViewClass *item_view_class;
  GimpItem              *item;

  item_view_class = GIMP_ITEM_TREE_VIEW_GET_CLASS (view);

  item = item_view_class->get_active_item (view->gimage);

  if (item)
    {
      GimpContainer *container;
      gint           index;

      container = GIMP_CONTAINER_VIEW (view)->container;

      index = gimp_container_get_child_index (container, GIMP_OBJECT (item));

      if (index < container->num_children - 1)
        {
          item_view_class->reorder_item (view->gimage, item, index + 1, TRUE,
                                         item_view_class->lower_desc);

          gimp_image_flush (view->gimage);
        }
    }
}

static void
gimp_item_tree_view_lower_extended_clicked (GtkWidget        *widget,
                                            guint             state,
                                            GimpItemTreeView *view)
{
  GimpItemTreeViewClass *item_view_class;
  GimpItem              *item;

  item_view_class = GIMP_ITEM_TREE_VIEW_GET_CLASS (view);

  item = item_view_class->get_active_item (view->gimage);

  if (item)
    {
      GimpContainer *container;
      gint           index;

      container = GIMP_CONTAINER_VIEW (view)->container;

      index = gimp_container_get_child_index (container, GIMP_OBJECT (item));

      if ((state & GDK_SHIFT_MASK) && (index < container->num_children - 1))
        {
          item_view_class->reorder_item (view->gimage, item,
                                         container->num_children - 1, TRUE,
                                         item_view_class->lower_to_bottom_desc);

          gimp_image_flush (view->gimage);
        }
    }
}


/*  "Delete" functions  */

static void
gimp_item_tree_view_delete_clicked (GtkWidget        *widget,
                                    GimpItemTreeView *view)
{
  GimpItemTreeViewClass *item_view_class;
  GimpItem              *item;

  item_view_class = GIMP_ITEM_TREE_VIEW_GET_CLASS (view);

  item = item_view_class->get_active_item (view->gimage);

  if (item)
    {
      item_view_class->remove_item (view->gimage, item);

      gimp_image_flush (view->gimage);
    }
}


/*  GimpImage callbacks  */

static void
gimp_item_tree_view_item_changed (GimpImage        *gimage,
                                  GimpItemTreeView *view)
{
  GimpItem *item;

  item = GIMP_ITEM_TREE_VIEW_GET_CLASS (view)->get_active_item (view->gimage);

  gimp_container_view_select_item (GIMP_CONTAINER_VIEW (view),
                                   (GimpViewable *) item);
}

static void
gimp_item_tree_view_size_changed (GimpImage        *gimage,
                                  GimpItemTreeView *view)
{
  GimpContainerView *container_view;

  container_view = GIMP_CONTAINER_VIEW (view);

  gimp_container_view_set_preview_size (GIMP_CONTAINER_VIEW (view),
					container_view->preview_size,
                                        container_view->preview_border_width);
}

static void
gimp_item_tree_view_name_edited (GtkCellRendererText *cell,
                                 const gchar         *path_str,
                                 const gchar         *new_text,
                                 GimpItemTreeView    *view)
{
  GimpContainerTreeView *tree_view;
  GtkTreePath           *path;
  GtkTreeIter            iter;

  tree_view = GIMP_CONTAINER_TREE_VIEW (view);

  path = gtk_tree_path_new_from_string (path_str);

  if (gtk_tree_model_get_iter (tree_view->model, &iter, path))
    {
      GimpPreviewRenderer *renderer;
      GimpItem            *item;

      gtk_tree_model_get (tree_view->model, &iter,
                          tree_view->model_column_renderer, &renderer,
                          -1);

      item = GIMP_ITEM (renderer->viewable);

      if (gimp_item_rename (item, new_text))
        {
          gimp_image_flush (gimp_item_get_image (item));
        }
      else
        {
          gchar *name = gimp_viewable_get_description (renderer->viewable, NULL);

          gtk_list_store_set (GTK_LIST_STORE (tree_view->model), &iter,
                              tree_view->model_column_name, name,
                              -1);
          g_free (name);
        }

      g_object_unref (renderer);
    }

  gtk_tree_path_free (path);
}


/*  "Visible" callbacks  */

static void
gimp_item_tree_view_visible_changed (GimpItem         *item,
                                     GimpItemTreeView *view)
{
  GimpContainerView     *container_view;
  GimpContainerTreeView *tree_view;
  GtkTreeIter           *iter;

  container_view = GIMP_CONTAINER_VIEW (view);
  tree_view      = GIMP_CONTAINER_TREE_VIEW (view);

  iter = g_hash_table_lookup (container_view->hash_table, item);

  if (iter)
    gtk_list_store_set (GTK_LIST_STORE (tree_view->model), iter,
                        view->model_column_visible,
                        gimp_item_get_visible (item),
                        -1);
}

static void
gimp_item_tree_view_eye_clicked (GtkCellRendererToggle *toggle,
                                 gchar                 *path_str,
                                 GdkModifierType        state,
                                 GimpItemTreeView      *view)
{
  gimp_item_tree_view_toggle_clicked (toggle, path_str, state, view,
                                      GIMP_UNDO_ITEM_VISIBILITY);
}

/*  "Linked" callbacks  */

static void
gimp_item_tree_view_linked_changed (GimpItem         *item,
                                    GimpItemTreeView *view)
{
  GimpContainerView     *container_view;
  GimpContainerTreeView *tree_view;
  GtkTreeIter           *iter;

  container_view = GIMP_CONTAINER_VIEW (view);
  tree_view      = GIMP_CONTAINER_TREE_VIEW (view);

  iter = g_hash_table_lookup (container_view->hash_table, item);

  if (iter)
    gtk_list_store_set (GTK_LIST_STORE (tree_view->model), iter,
                        view->model_column_linked,
                        gimp_item_get_linked (item),
                        -1);
}

static void
gimp_item_tree_view_chain_clicked (GtkCellRendererToggle *toggle,
                                   gchar                 *path_str,
                                   GdkModifierType        state,
                                   GimpItemTreeView      *view)
{
  gimp_item_tree_view_toggle_clicked (toggle, path_str, state, view,
                                      GIMP_UNDO_ITEM_LINKED);
}


/*  Utility functions used from eye_clicked and chain_clicked.
 *  Would make sense to do this in a generic fashion using
 *  properties, but for now it's better than duplicating the code.
 */
static void
gimp_item_tree_view_toggle_clicked (GtkCellRendererToggle *toggle,
                                    gchar                 *path_str,
                                    GdkModifierType        state,
                                    GimpItemTreeView      *view,
                                    GimpUndoType           undo_type)
{
  GimpContainerTreeView *tree_view = GIMP_CONTAINER_TREE_VIEW (view);
  GtkTreePath           *path;
  GtkTreeIter            iter;
  GimpUndoType           group_type;
  const gchar           *undo_desc;

  gboolean (* getter)  (const GimpItem *item);
  void     (* setter)  (GimpItem       *item,
                        gboolean        value,
                        gboolean        push_undo);
  gboolean (* pusher)  (GimpImage      *gimage,
                        const gchar    *undo_desc,
                        GimpItem       *item);

  switch (undo_type)
    {
    case GIMP_UNDO_ITEM_VISIBILITY:
      getter     = gimp_item_get_visible;
      setter     = gimp_item_set_visible;
      pusher     = gimp_image_undo_push_item_visibility;
      group_type = GIMP_UNDO_GROUP_ITEM_VISIBILITY;
      undo_desc  = _("Set Item Exclusive Visible");
      break;

    case GIMP_UNDO_ITEM_LINKED:
      getter     = gimp_item_get_linked;
      setter     = gimp_item_set_linked;
      pusher     = gimp_image_undo_push_item_linked;
      group_type = GIMP_UNDO_GROUP_ITEM_LINKED;
      undo_desc  = _("Set Item Exclusive Linked");
      break;

    default:
      return;
    }

  path = gtk_tree_path_new_from_string (path_str);

  if (gtk_tree_model_get_iter (tree_view->model, &iter, path))
    {
      GimpPreviewRenderer *renderer;
      GimpItem            *item;
      GimpImage           *gimage;
      gboolean             active;

      gtk_tree_model_get (tree_view->model, &iter,
                          tree_view->model_column_renderer, &renderer,
                          -1);
      g_object_get (toggle,
                    "active", &active,
                    NULL);

      item = GIMP_ITEM (renderer->viewable);
      g_object_unref (renderer);

      gimage = gimp_item_get_image (item);

      if (state & GDK_SHIFT_MASK)
        {
          GList    *on  = NULL;
          GList    *off = NULL;
          GList    *list;
          gboolean  iter_valid;

          for (iter_valid = gtk_tree_model_get_iter_first (tree_view->model,
                                                           &iter);
               iter_valid;
               iter_valid = gtk_tree_model_iter_next (tree_view->model,
                                                      &iter))
            {
              gtk_tree_model_get (tree_view->model, &iter,
                                  tree_view->model_column_renderer, &renderer,
                                  -1);

              if ((GimpItem *) renderer->viewable != item)
                {
                  if (getter (GIMP_ITEM (renderer->viewable)))
                    on = g_list_prepend (on, renderer->viewable);
                  else
                    off = g_list_prepend (off, renderer->viewable);
                }

              g_object_unref (renderer);
            }

          if (on || off || ! getter (item))
            {
              GimpUndo *undo      = gimp_undo_stack_peek (gimage->undo_stack);
              gboolean  push_undo = TRUE;

              /*  compress exclusive visibility/linked undos  */
              if (! gimp_undo_stack_peek (gimage->redo_stack) &&
                  GIMP_IS_UNDO_STACK (undo)                   &&
                  undo->undo_type == group_type               &&
                  g_object_get_data (G_OBJECT (undo), "item-type")
                  == (gpointer) view->item_type)
                push_undo = FALSE;

              if (push_undo)
                {
                  if (gimp_image_undo_group_start (gimage, group_type,
                                                   undo_desc))
                    {
                      undo = gimp_undo_stack_peek (gimage->undo_stack);

                      if (GIMP_IS_UNDO_STACK (undo) &&
                          undo->undo_type == group_type)
                        {
                          g_object_set_data (G_OBJECT (undo), "item-type",
                                             (gpointer) view->item_type);
                        }
                    }

                  pusher (gimage, NULL, item);

                  for (list = on; list; list = g_list_next (list))
                    pusher (gimage, NULL, list->data);

                  for (list = off; list; list = g_list_next (list))
                    pusher (gimage, NULL, list->data);

                  gimp_image_undo_group_end (gimage);
                }
            }

          setter (item, TRUE, FALSE);

          if (on)
            {
              for (list = on; list; list = g_list_next (list))
                setter (GIMP_ITEM (list->data), FALSE, FALSE);
            }
          else if (off)
            {
              for (list = off; list; list = g_list_next (list))
                setter (GIMP_ITEM (list->data), TRUE, FALSE);
            }

          g_list_free (on);
          g_list_free (off);
        }
      else
        {
          GimpUndo *undo;
          gboolean  push_undo = TRUE;

          undo = gimp_undo_stack_peek (gimage->undo_stack);

          /*  compress undos  */
          if (! gimp_undo_stack_peek (gimage->redo_stack) &&
              GIMP_IS_ITEM_UNDO (undo)                    &&
              GIMP_ITEM_UNDO (undo)->item == item         &&
              undo->undo_type == undo_type)
            push_undo = FALSE;

          setter (item, ! active, push_undo);
        }

      gimp_image_flush (gimage);
    }

  gtk_tree_path_free (path);
}
