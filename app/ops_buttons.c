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

#include "ops_buttons.h"

#include "libgimp/gimpintl.h"


typedef enum
{
  OPS_BUTTON_MODIFIER_NONE,
  OPS_BUTTON_MODIFIER_SHIFT,
  OPS_BUTTON_MODIFIER_CTRL,
  OPS_BUTTON_MODIFIER_ALT,
  OPS_BUTTON_MODIFIER_SHIFT_CTRL,
  OPS_BUTTON_MODIFIER_LAST
} OpsButtonModifier;


static void ops_button_extended_clicked (GtkWidget *widget,
                                         guint      modifier_state,
                                         gpointer   data);


GtkWidget *
ops_button_box_new (OpsButton     *ops_button,
		    OpsButtonType  ops_type)   
{
  GtkWidget *button;
  GtkWidget *button_box;
  GtkWidget *pixmap;
  GSList    *group = NULL;

  button_box = gtk_hbox_new (TRUE, 1);

  while (ops_button->xpm_data)
    {
      pixmap = gimp_pixmap_new (ops_button->xpm_data);

      switch (ops_type)
	{
	case OPS_BUTTON_NORMAL:
	  button = gimp_button_new ();
	  break;

	case OPS_BUTTON_RADIO:
	  button = gtk_radio_button_new (group);
	  group = gtk_radio_button_group (GTK_RADIO_BUTTON (button));
	  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
	  break;

	default:
	  g_warning ("ops_button_box_new: unknown type %d\n", ops_type);
	  continue;
	}

      gtk_container_add (GTK_CONTAINER (button), pixmap);

      if (ops_button->ext_callbacks == NULL)
	{
	  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				     GTK_SIGNAL_FUNC (ops_button->callback),
				     NULL);
	}
      else
	{
	  gtk_signal_connect (GTK_OBJECT (button), "extended_clicked",
			      GTK_SIGNAL_FUNC (ops_button_extended_clicked),
			      ops_button);	  
	}

      gimp_help_set_help_data (button,
			       gettext (ops_button->tooltip),
			       ops_button->private_tip);

      gtk_box_pack_start (GTK_BOX (button_box), button, TRUE, TRUE, 0); 

      gtk_widget_show (pixmap);
      gtk_widget_show (button);

      ops_button->widget = button;

      ops_button++;
    }

  return button_box;
}

static void
ops_button_extended_clicked (GtkWidget *widget, 
                             guint      modifier_state,
                             gpointer   data)
{
  OpsButton         *ops_button;
  OpsButtonModifier  modifier;

  g_return_if_fail (data != NULL);
  ops_button = (OpsButton *) data;

  if (modifier_state & GDK_SHIFT_MASK)
    {
      if (modifier_state & GDK_CONTROL_MASK)
        modifier = OPS_BUTTON_MODIFIER_SHIFT_CTRL;
      else 
        modifier = OPS_BUTTON_MODIFIER_SHIFT;
    }
  else if (modifier_state & GDK_CONTROL_MASK)
    modifier = OPS_BUTTON_MODIFIER_CTRL;
  else if (modifier_state & GDK_MOD1_MASK)
    modifier = OPS_BUTTON_MODIFIER_ALT;
  else 
    modifier = OPS_BUTTON_MODIFIER_NONE;

  if (modifier > OPS_BUTTON_MODIFIER_NONE &&
      modifier < OPS_BUTTON_MODIFIER_LAST)
    {
      if (ops_button->ext_callbacks[modifier - 1] != NULL)
	(ops_button->ext_callbacks[modifier - 1]) (widget, NULL);
      else
	(ops_button->callback) (widget, NULL);
    } 
  else 
    (ops_button->callback) (widget, NULL);
}
