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

#include "actions-types.h"

#include "core/gimpcontext.h"
#include "core/gimpdata.h"

#include "widgets/gimpactiongroup.h"
#include "widgets/gimpcontainereditor.h"
#include "widgets/gimpcontainerview.h"
#include "widgets/gimpdatafactoryview.h"
#include "widgets/gimphelp-ids.h"

#include "data-commands.h"
#include "gradients-actions.h"
#include "gradients-commands.h"

#include "gimp-intl.h"


static GimpActionEntry gradients_actions[] =
{
  { "gradients-popup", GIMP_STOCK_GRADIENT, N_("Gradients Menu"),
    NULL, NULL, NULL,
    GIMP_HELP_GRADIENT_DIALOG },

  { "gradients-edit", GIMP_STOCK_EDIT,
    N_("_Edit Gradient..."), NULL, NULL,
    G_CALLBACK (data_edit_data_cmd_callback),
    GIMP_HELP_GRADIENT_EDIT },

  { "gradients-new", GTK_STOCK_NEW,
    N_("_New Gradient"), "", NULL,
    G_CALLBACK (data_new_data_cmd_callback),
    GIMP_HELP_GRADIENT_NEW },

  { "gradients-duplicate", GIMP_STOCK_DUPLICATE,
    N_("D_uplicate Gradient"), NULL, NULL,
    G_CALLBACK (data_duplicate_data_cmd_callback),
    GIMP_HELP_GRADIENT_DUPLICATE },

  { "gradients-save-as-pov", GTK_STOCK_SAVE_AS,
    N_("Save as _POV-Ray..."), "", NULL,
    G_CALLBACK (gradients_save_as_pov_ray_cmd_callback),
    GIMP_HELP_GRADIENT_SAVE_AS_POV },

  { "gradients-delete", GTK_STOCK_DELETE,
    N_("_Delete Gradient..."), "", NULL,
    G_CALLBACK (data_delete_data_cmd_callback),
    GIMP_HELP_GRADIENT_DELETE },

  { "gradients-refresh", GTK_STOCK_REFRESH,
    N_("_Refresh Gradients"), "", NULL,
    G_CALLBACK (data_refresh_data_cmd_callback),
    GIMP_HELP_GRADIENT_REFRESH }
};


void
gradients_actions_setup (GimpActionGroup *group)
{
  gimp_action_group_add_actions (group,
                                 gradients_actions,
                                 G_N_ELEMENTS (gradients_actions));
}

void
gradients_actions_update (GimpActionGroup *group,
                          gpointer         user_data)
{
  GimpContainerEditor *editor;
  GimpGradient        *gradient;
  GimpData            *data = NULL;

  editor = GIMP_CONTAINER_EDITOR (user_data);

  gradient = gimp_context_get_gradient (editor->view->context);

  if (gradient)
    data = GIMP_DATA (gradient);

#define SET_SENSITIVE(action,condition) \
        gimp_action_group_set_action_sensitive (group, action, (condition) != 0)

  SET_SENSITIVE ("gradients-edit",
		 gradient && GIMP_DATA_FACTORY_VIEW (editor)->data_edit_func);
  SET_SENSITIVE ("gradients-duplicate",
		 gradient && GIMP_DATA_GET_CLASS (data)->duplicate);
  SET_SENSITIVE ("gradients-save-as-pov",
		 gradient);
  SET_SENSITIVE ("gradients-delete",
		 gradient && data->deletable);

#undef SET_SENSITIVE
}
