/* The GIMP -- an image manipulation program
 * Copyright (C) 1995-1997 Spencer Kimball and Peter Mattis
 *
 * gimppropwidgets.c
 * Copyright (C) 2002 Michael Natterer <mitch@gimp.org>
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

#include "libgimpmath/gimpmath.h"
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "widgets-types.h"

#include "config/gimpconfig-params.h"
#include "config/gimpconfig-path.h"

#include "gimpcolorpanel.h"
#include "gimpenummenu.h"
#include "gimppropwidgets.h"

#include "libgimp/gimpintl.h"


/*  utility function prototypes  */

static void         set_param_spec   (GObject     *object,
                                      GParamSpec  *value);
static GParamSpec * get_param_spec   (GObject     *object);

static GParamSpec * find_param_spec  (GObject     *object,
                                      const gchar *property_name,
                                      const gchar *strloc);
static GParamSpec * check_param_spec (GObject     *object,
                                      const gchar *property_name,
                                      GType        type,
                                      const gchar *strloc);

static void         connect_notify   (GObject     *config,
                                      const gchar *property_name,
                                      GCallback    callback,
                                      gpointer     callback_data);


/******************/
/*  check button  */
/******************/

static void   gimp_prop_check_button_callback (GtkWidget  *widget,
                                               GObject    *config);
static void   gimp_prop_check_button_notify   (GObject    *config,
                                               GParamSpec *param_spec,
                                               GtkWidget  *button);

GtkWidget *
gimp_prop_check_button_new (GObject     *config,
                            const gchar *property_name,
                            const gchar *label)
{
  GParamSpec *param_spec;
  GtkWidget  *button;
  gboolean    value;

  param_spec = check_param_spec (config, property_name,
                                 G_TYPE_PARAM_BOOLEAN, G_STRLOC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  button = gtk_check_button_new_with_mnemonic (label);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), value);

  set_param_spec (G_OBJECT (button), param_spec);

  g_signal_connect (button, "toggled",
		    G_CALLBACK (gimp_prop_check_button_callback),
		    config);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_check_button_notify),
                  button);

  return button;
}

static void
gimp_prop_check_button_callback (GtkWidget *widget,
                                 GObject   *config)
{
  GParamSpec *param_spec;

  param_spec = get_param_spec (G_OBJECT (widget));
  if (! param_spec)
    return;

  g_object_set (config,
                param_spec->name, GTK_TOGGLE_BUTTON (widget)->active,
                NULL);
}

static void
gimp_prop_check_button_notify (GObject    *config,
                               GParamSpec *param_spec,
                               GtkWidget  *button)
{
  gboolean value;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  if (GTK_TOGGLE_BUTTON (button)->active != value)
    {
      g_signal_handlers_block_by_func (button,
                                       gimp_prop_check_button_callback,
                                       config);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), value);

      g_signal_handlers_unblock_by_func (button,
                                         gimp_prop_check_button_callback,
                                         config);
    }
}


/******************/
/*  option menus  */
/******************/

static GtkWidget * gimp_prop_enum_option_menu_new_full (GObject     *config,
                                                        const gchar *property_name,
                                                        gint         minimum,
                                                        gint         maximum,
                                                        gint         n_values,
                                                        va_list      args);
static void        gimp_prop_option_menu_callback      (GtkWidget   *widget,
                                                        GObject     *config);
static void        gimp_prop_option_menu_notify        (GObject     *config,
                                                        GParamSpec  *param_spec,
                                                        GtkWidget   *menu);

GtkWidget *
gimp_prop_boolean_option_menu_new (GObject     *config,
                                   const gchar *property_name,
                                   const gchar *true_text,
                                   const gchar *false_text)
{
  GParamSpec *param_spec;
  GtkWidget  *optionmenu;
  gboolean    value;

  param_spec = check_param_spec (config, property_name,
                                 G_TYPE_PARAM_BOOLEAN, G_STRLOC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  optionmenu =
    gimp_option_menu_new2 (FALSE,
                           G_CALLBACK (gimp_prop_option_menu_callback),
			   config,
			   GINT_TO_POINTER (value),

			   true_text,  GINT_TO_POINTER (TRUE),  NULL,
			   false_text, GINT_TO_POINTER (FALSE), NULL,

			   NULL);

  set_param_spec (G_OBJECT (optionmenu), param_spec);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_option_menu_notify),
                  optionmenu);

  return optionmenu;
}

GtkWidget *
gimp_prop_enum_option_menu_new (GObject     *config,
                                const gchar *property_name,
                                gint         minimum,
                                gint         maximum)
{
  return gimp_prop_enum_option_menu_new_full (config, property_name,
                                              minimum, maximum, 
                                              0, NULL);
}

GtkWidget *
gimp_prop_enum_option_menu_new_with_values (GObject     *config,
                                            const gchar *property_name,
                                            gint         n_values,
                                            ...)
{
  GtkWidget  *optionmenu;
  va_list     args;

  va_start (args, n_values);

  optionmenu = gimp_prop_enum_option_menu_new_full (config, property_name,
                                                    0, 0,
                                                    n_values, args);

  va_end (args);

  return optionmenu;
}

GtkWidget *
gimp_prop_enum_option_menu_new_valist (GObject     *config,
                                       const gchar *property_name,
                                       gint         n_values,
                                       va_list      args)
{
  return gimp_prop_enum_option_menu_new_full (config, property_name,
                                              0, 0,
                                              n_values, args);
}

static GtkWidget *
gimp_prop_enum_option_menu_new_full (GObject     *config,
                                     const gchar *property_name,
                                     gint         minimum,
                                     gint         maximum,
                                     gint         n_values,
                                     va_list      args)
{
  GParamSpec *param_spec;
  GtkWidget  *optionmenu;
  gint        value;

  param_spec = check_param_spec (config, property_name,
                                 G_TYPE_PARAM_ENUM, G_STRLOC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  if (n_values > 0)
    {
      optionmenu =
        gimp_enum_option_menu_new_with_values_valist (param_spec->value_type,
                                                      G_CALLBACK (gimp_prop_option_menu_callback),
                                                      config,
                                                      n_values, args);
    }
  else
    {
      if (minimum != maximum)
        {
          optionmenu =
            gimp_enum_option_menu_new_with_range (param_spec->value_type,
                                                  minimum, maximum,
                                                  G_CALLBACK (gimp_prop_option_menu_callback),
                                                  config);
        }
      else
        {
          optionmenu =
            gimp_enum_option_menu_new (param_spec->value_type,
                                       G_CALLBACK (gimp_prop_option_menu_callback),
                                       config);
        }
    }

  gimp_option_menu_set_history (GTK_OPTION_MENU (optionmenu),
                                GINT_TO_POINTER (value));

  set_param_spec (G_OBJECT (optionmenu), param_spec);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_option_menu_notify),
                  optionmenu);

  return optionmenu;
}

static void
gimp_prop_option_menu_callback (GtkWidget *widget,
                                GObject   *config)
{
  if (GTK_IS_MENU (widget->parent))
    {
      GtkWidget *optionmenu;

      optionmenu = gtk_menu_get_attach_widget (GTK_MENU (widget->parent));

      if (optionmenu)
        {
          GParamSpec *param_spec;
          gint        value;

          param_spec = get_param_spec (G_OBJECT (optionmenu));
          if (! param_spec)
            return;

          value = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget),
                                                      "gimp-item-data"));

          g_object_set (config,
                        param_spec->name, value,
                        NULL);
        }
    }
}

static void
gimp_prop_option_menu_notify (GObject    *config,
                              GParamSpec *param_spec,
                              GtkWidget  *menu)
{
  gint value;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  gimp_option_menu_set_history (GTK_OPTION_MENU (menu),
                                GINT_TO_POINTER (value));
}


/*****************/
/*  adjustments  */
/*****************/

static void   gimp_prop_adjustment_callback (GtkAdjustment *adjustment,
                                             GObject       *config);
static void   gimp_prop_adjustment_notify   (GObject       *config,
                                             GParamSpec    *param_spec,
                                             GtkAdjustment *adjustment);

GtkWidget *
gimp_prop_spin_button_new (GObject     *config,
                           const gchar *property_name,
                           gdouble      step_increment,
                           gdouble      page_increment,
                           gint         digits)
{
  GParamSpec *param_spec;
  GtkWidget  *spinbutton;
  GtkObject  *adjustment;

  param_spec = find_param_spec (config, property_name, G_STRLOC);
  if (! param_spec)
    return NULL;

  if (G_IS_PARAM_SPEC_INT (param_spec))
    {
      GParamSpecInt *int_spec;
      gint           value;

      int_spec = G_PARAM_SPEC_INT (param_spec);

      g_object_get (config, property_name, &value, NULL);

      spinbutton = gimp_spin_button_new (&adjustment, value,
                                         int_spec->minimum,
                                         int_spec->maximum,
                                         step_increment,
                                         page_increment,
                                         0.0, 1.0, 0);
    }
  else if (G_IS_PARAM_SPEC_UINT (param_spec))
    {
      GParamSpecUInt *uint_spec;
      guint           value;

      uint_spec = G_PARAM_SPEC_UINT (param_spec);

      g_object_get (config, property_name, &value, NULL);

      spinbutton = gimp_spin_button_new (&adjustment, value,
                                         uint_spec->minimum,
                                         uint_spec->maximum,
                                         step_increment,
                                         page_increment,
                                         0.0, 1.0, 0);
    }
  else if (G_IS_PARAM_SPEC_DOUBLE (param_spec))
    {
      GParamSpecDouble *double_spec;
      gdouble           value;

      double_spec = G_PARAM_SPEC_DOUBLE (param_spec);

      g_object_get (config, property_name, &value, NULL);

      spinbutton = gimp_spin_button_new (&adjustment, value,
                                         double_spec->minimum,
                                         double_spec->maximum,
                                         step_increment,
                                         page_increment,
                                         0.0, 1.0, digits);
    }
  else
    {
      g_warning (G_STRLOC ": property '%s' of %s is not numeric",
                 property_name,
                 g_type_name (G_TYPE_FROM_INSTANCE (config)));
      return NULL;
    }

  set_param_spec (G_OBJECT (adjustment), param_spec);

  g_signal_connect (adjustment, "value_changed",
		    G_CALLBACK (gimp_prop_adjustment_callback),
		    config);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_adjustment_notify),
                  adjustment);

  return spinbutton;
}

static void
gimp_prop_adjustment_callback (GtkAdjustment *adjustment,
                               GObject       *config)
{
  GParamSpec *param_spec;

  param_spec = get_param_spec (G_OBJECT (adjustment));
  if (! param_spec)
    return;

  if (G_IS_PARAM_SPEC_INT (param_spec))
    {
      g_object_set (config,
                    param_spec->name, (gint) adjustment->value,
                    NULL);
    }
  else if (G_IS_PARAM_SPEC_UINT (param_spec))
    {
      g_object_set (config,
                    param_spec->name, (guint) adjustment->value,
                    NULL);
    }
  else if (G_IS_PARAM_SPEC_LONG (param_spec))
    {
      g_object_set (config,
                    param_spec->name, (glong) adjustment->value,
                    NULL);
    }
  else if (G_IS_PARAM_SPEC_ULONG (param_spec))
    {
      g_object_set (config,
                    param_spec->name, (gulong) adjustment->value,
                    NULL);
    }
  else if (G_IS_PARAM_SPEC_DOUBLE (param_spec))
    {
      g_object_set (config,
                    param_spec->name, adjustment->value,
                    NULL);
    }
}

static void
gimp_prop_adjustment_notify (GObject       *config,
                             GParamSpec    *param_spec,
                             GtkAdjustment *adjustment)
{
  gdouble value;

  if (G_IS_PARAM_SPEC_INT (param_spec))
    {
      gint int_value;

      g_object_get (config,
                    param_spec->name, &int_value,
                    NULL);

      value = int_value;
    }
  else if (G_IS_PARAM_SPEC_UINT (param_spec))
    {
      guint uint_value;

      g_object_get (config,
                    param_spec->name, &uint_value,
                    NULL);

      value = uint_value;
    }
  else if (G_IS_PARAM_SPEC_LONG (param_spec))
    {
      glong long_value;

      g_object_get (config,
                    param_spec->name, &long_value,
                    NULL);

      value = long_value;
    }
  else if (G_IS_PARAM_SPEC_ULONG (param_spec))
    {
      gulong ulong_value;

      g_object_get (config,
                    param_spec->name, &ulong_value,
                    NULL);

      value = ulong_value;
    }
  else if (G_IS_PARAM_SPEC_DOUBLE (param_spec))
    {
      g_object_get (config,
                    param_spec->name, &value,
                    NULL);
    }
  else
    {
      return;
    }

  if (adjustment->value != value)
    {
      g_signal_handlers_block_by_func (adjustment,
                                       gimp_prop_adjustment_callback,
                                       config);

      gtk_adjustment_set_value (adjustment, value);

      g_signal_handlers_unblock_by_func (adjustment,
                                         gimp_prop_adjustment_callback,
                                         config);
    }
}


/*************/
/*  memsize  */
/*************/

static void   gimp_prop_memsize_callback (GimpMemsizeEntry *entry,
					  GObject          *config);
static void   gimp_prop_memsize_notify   (GObject          *config,
					  GParamSpec       *param_spec,
					  GimpMemsizeEntry *entry);

GtkWidget *
gimp_prop_memsize_entry_new (GObject     *config,
                             const gchar *property_name)
{
  GParamSpec      *param_spec;
  GParamSpecULong *ulong_spec;
  GtkWidget       *entry;
  gulong           value;

  param_spec = check_param_spec (config, property_name,
                                 GIMP_TYPE_PARAM_MEMSIZE, G_STRLOC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  ulong_spec = G_PARAM_SPEC_ULONG (param_spec);

  entry = gimp_memsize_entry_new (value,
				  ulong_spec->minimum,
				  ulong_spec->maximum);

  set_param_spec (G_OBJECT (entry), param_spec);

  g_signal_connect (entry, "value_changed",
		    G_CALLBACK (gimp_prop_memsize_callback),
		    config);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_memsize_notify),
                  entry);

  return entry;
}


static void
gimp_prop_memsize_callback (GimpMemsizeEntry *entry,
			    GObject          *config)
{
  GParamSpec *param_spec;

  param_spec = get_param_spec (G_OBJECT (entry));
  if (! param_spec)
    return;

  g_return_if_fail (G_IS_PARAM_SPEC_ULONG (param_spec));

  g_object_set (config,
		param_spec->name, gimp_memsize_entry_get_value (entry),
		NULL);
}

static void
gimp_prop_memsize_notify (GObject          *config,
			  GParamSpec       *param_spec,
			  GimpMemsizeEntry *entry)
{
  gulong value;

  g_return_if_fail (G_IS_PARAM_SPEC_ULONG (param_spec));

  g_object_get (config,
		param_spec->name, &value,
		NULL);

  if (entry->value != value)
    {
      g_signal_handlers_block_by_func (entry,
                                       gimp_prop_memsize_callback,
                                       config);

      gimp_memsize_entry_set_value (entry, value);

      g_signal_handlers_unblock_by_func (entry,
                                         gimp_prop_memsize_callback,
                                         config);
    }
}


/***********/
/*  entry  */
/***********/

static void   gimp_prop_entry_callback (GtkWidget  *entry,
                                        GObject    *config);
static void   gimp_prop_entry_notify   (GObject    *config,
                                        GParamSpec *param_spec,
                                        GtkWidget  *entry);

GtkWidget *
gimp_prop_entry_new (GObject     *config,
                     const gchar *property_name,
                     gint         max_len)
{
  GParamSpec *param_spec;
  GtkWidget  *entry;
  gchar      *value;

  param_spec = check_param_spec (config, property_name,
                                 G_TYPE_PARAM_STRING, G_STRLOC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (entry), value);

  g_free (value);

  if (max_len > 0)
    gtk_entry_set_max_length (GTK_ENTRY (entry), max_len);

  set_param_spec (G_OBJECT (entry), param_spec);

  g_signal_connect (entry, "changed",
		    G_CALLBACK (gimp_prop_entry_callback),
		    config);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_entry_notify),
                  entry);

  return entry;
}

static void
gimp_prop_entry_callback (GtkWidget *entry,
                          GObject   *config)
{
  GParamSpec  *param_spec;
  const gchar *text;

  param_spec = get_param_spec (G_OBJECT (entry));
  if (! param_spec)
    return;

  text = gtk_entry_get_text (GTK_ENTRY (entry));

  g_signal_handlers_block_by_func (config,
                                   gimp_prop_entry_notify,
                                   entry);

  g_object_set (config,
                param_spec->name, text,
                NULL);

  g_signal_handlers_unblock_by_func (config,
                                     gimp_prop_entry_notify,
                                     entry);
}

static void
gimp_prop_entry_notify (GObject    *config,
                        GParamSpec *param_spec,
                        GtkWidget  *entry)
{
  gchar *value;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  g_signal_handlers_block_by_func (entry,
                                   gimp_prop_entry_callback,
                                   config);

  gtk_entry_set_text (GTK_ENTRY (entry), value);

  g_signal_handlers_unblock_by_func (entry,
                                     gimp_prop_entry_callback,
                                     config);

  g_free (value);
}


/*****************/
/*  text buffer  */
/*****************/

static void   gimp_prop_text_buffer_callback (GtkTextBuffer *text_buffer,
                                              GObject       *config);
static void   gimp_prop_text_buffer_notify   (GObject       *config,
                                              GParamSpec    *param_spec,
                                              GtkTextBuffer *text_buffer);

GtkTextBuffer *
gimp_prop_text_buffer_new (GObject     *config,
                           const gchar *property_name,
                           gint         max_len)
{
  GParamSpec    *param_spec;
  GtkTextBuffer *text_buffer;
  gchar         *value;

  param_spec = check_param_spec (config, property_name,
                                 G_TYPE_PARAM_STRING, G_STRLOC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  text_buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_set_text (text_buffer, value, -1);

  g_free (value);

  if (max_len > 0)
    g_object_set_data (G_OBJECT (text_buffer), "max-len",
                       GINT_TO_POINTER (max_len));

  set_param_spec (G_OBJECT (text_buffer), param_spec);

  g_signal_connect (text_buffer, "changed",
		    G_CALLBACK (gimp_prop_text_buffer_callback),
		    config);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_text_buffer_notify),
                  text_buffer);

  return text_buffer;
}

static void
gimp_prop_text_buffer_callback (GtkTextBuffer *text_buffer,
                                GObject       *config)
{
  GParamSpec  *param_spec;
  GtkTextIter  start_iter;
  GtkTextIter  end_iter;
  gchar       *text;
  gint         max_len;

  param_spec = get_param_spec (G_OBJECT (text_buffer));
  if (! param_spec)
    return;

  gtk_text_buffer_get_bounds (text_buffer, &start_iter, &end_iter);
  text = gtk_text_buffer_get_text (text_buffer, &start_iter, &end_iter, FALSE);

  max_len = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (text_buffer),
                                                "max-length"));

  if (max_len > 0 && strlen (text) > max_len)
    {
      g_message (_("This text input field is limited to %d characters."), 
                 max_len);

      gtk_text_buffer_get_iter_at_offset (text_buffer, &start_iter,
                                          max_len - 1);
      gtk_text_buffer_get_end_iter (text_buffer, &end_iter);

      /*  this calls us recursivaly, but in the else branch  */
      gtk_text_buffer_delete (text_buffer, &start_iter, &end_iter);
    }
  else
    {
      g_signal_handlers_block_by_func (config,
                                       gimp_prop_text_buffer_notify,
                                       text_buffer);

      g_object_set (config,
                    param_spec->name, text,
                    NULL);

      g_signal_handlers_unblock_by_func (config,
                                         gimp_prop_text_buffer_notify,
                                         text_buffer);
    }

  g_free (text);
}

static void
gimp_prop_text_buffer_notify (GObject       *config,
                              GParamSpec    *param_spec,
                              GtkTextBuffer *text_buffer)
{
  gchar *value;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  g_signal_handlers_block_by_func (text_buffer,
                                   gimp_prop_text_buffer_callback,
                                   config);

  gtk_text_buffer_set_text (text_buffer, value, -1);

  g_signal_handlers_unblock_by_func (text_buffer,
                                     gimp_prop_text_buffer_callback,
                                     config);

  g_free (value);
}


/****************/
/*  file entry  */
/****************/


static void   gimp_prop_file_entry_callback (GimpFileSelection *entry,
                                             GObject           *config);
static void   gimp_prop_file_entry_notify   (GObject           *config,
                                             GParamSpec        *param_spec,
                                             GimpFileSelection *entry);

GtkWidget *
gimp_prop_file_entry_new (GObject     *config,
                          const gchar *property_name,
                          const gchar *filesel_title,
                          gboolean     dir_only,
                          gboolean     check_valid)
{
  GParamSpec *param_spec;
  GtkWidget  *entry;
  gchar      *filename;
  gchar      *value;

  param_spec = check_param_spec (config, property_name,
                                 GIMP_TYPE_PARAM_PATH, G_STRLOC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  filename = value ? gimp_config_path_expand (value, FALSE, NULL) : NULL;

  entry = gimp_file_selection_new (filesel_title,
                                   filename, dir_only, check_valid);

  g_free (value);
  g_free (filename);

  set_param_spec (G_OBJECT (entry), param_spec);

  g_signal_connect (entry, "filename_changed",
		    G_CALLBACK (gimp_prop_file_entry_callback),
		    config);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_file_entry_notify),
                  entry);

  return entry;
}

static void
gimp_prop_file_entry_callback (GimpFileSelection *entry,
                               GObject           *config)
{
  GParamSpec *param_spec;
  gchar      *value;

  param_spec = get_param_spec (G_OBJECT (entry));
  if (! param_spec)
    return;

  value = gimp_file_selection_get_filename (entry);

  g_object_set (config,
                param_spec->name, value,
                NULL);

  g_free (value);
}

static void
gimp_prop_file_entry_notify (GObject           *config,
                             GParamSpec        *param_spec,
                             GimpFileSelection *entry)
{
  gchar *value;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  g_signal_handlers_block_by_func (entry,
                                   gimp_prop_file_entry_callback,
                                   config);

  gimp_file_selection_set_filename (entry, value);

  g_signal_handlers_unblock_by_func (entry,
                                     gimp_prop_file_entry_callback,
                                     config);

  g_free (value);
}


/*****************/
/*  path editor  */
/*****************/

static void   gimp_prop_path_editor_callback (GimpPathEditor *editor,
                                              GObject        *config);
static void   gimp_prop_path_editor_notify   (GObject        *config,
                                              GParamSpec     *param_spec,
                                              GimpPathEditor *editor);

GtkWidget *
gimp_prop_path_editor_new (GObject     *config,
                           const gchar *property_name,
                           const gchar *filesel_title)
{
  GParamSpec *param_spec;
  GtkWidget  *editor;
  gchar      *value;
  gchar      *filename;

  param_spec = check_param_spec (config, property_name,
                                 GIMP_TYPE_PARAM_PATH, G_STRLOC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  filename = value ? gimp_config_path_expand (value, FALSE, NULL) : NULL;

  editor = gimp_path_editor_new (filesel_title, filename);

  g_free (filename);
  g_free (value);

  set_param_spec (G_OBJECT (editor), param_spec);

  g_signal_connect (editor, "path_changed",
		    G_CALLBACK (gimp_prop_path_editor_callback),
		    config);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_path_editor_notify),
                  editor);

  return editor;
}

static void
gimp_prop_path_editor_callback (GimpPathEditor *editor,
                                GObject        *config)
{
  GParamSpec *param_spec;
  gchar      *value;

  param_spec = get_param_spec (G_OBJECT (editor));
  if (! param_spec)
    return;

  value = gimp_path_editor_get_path (editor);

  g_object_set (config,
                param_spec->name, value,
                NULL);

  g_free (value);
}

static void
gimp_prop_path_editor_notify (GObject        *config,
                              GParamSpec     *param_spec,
                              GimpPathEditor *editor)
{
  gchar *value;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  g_signal_handlers_block_by_func (editor,
                                   gimp_prop_path_editor_callback,
                                   config);

  gimp_path_editor_set_path (editor, value);

  g_signal_handlers_unblock_by_func (editor,
                                     gimp_prop_path_editor_callback,
                                     config);

  g_free (value);
}


/*****************/
/*  coordinates  */
/*****************/

static void   gimp_prop_coordinates_callback    (GimpSizeEntry *sizeentry,
                                                 GObject       *config);
static void   gimp_prop_coordinates_notify_x    (GObject       *config,
                                                 GParamSpec    *param_spec,
                                                 GimpSizeEntry *sizeentry);
static void   gimp_prop_coordinates_notify_y    (GObject       *config,
                                                 GParamSpec    *param_spec,
                                                 GimpSizeEntry *sizeentry);
static void   gimp_prop_coordinates_notify_unit (GObject       *config,
                                                 GParamSpec    *param_spec,
                                                 GimpSizeEntry *sizeentry);

GtkWidget *
gimp_prop_coordinates_new (GObject                   *config,
                           const gchar               *x_property_name,
                           const gchar               *y_property_name,
                           const gchar               *unit_property_name,
                           const gchar               *unit_format,
                           GimpSizeEntryUpdatePolicy  update_policy,
                           gdouble                    xresolution,
                           gdouble                    yresolution,
                           gboolean                   has_chainbutton)
{
  GParamSpec *x_param_spec;
  GParamSpec *y_param_spec;
  GParamSpec *unit_param_spec;
  GtkWidget  *sizeentry;
  gdouble     x_value;
  gdouble     y_value;
  GimpUnit    unit_value;
  gboolean    chain_checked = FALSE;

  x_param_spec = find_param_spec (config, x_property_name, G_STRLOC);
  if (! x_param_spec)
    return NULL;

  y_param_spec = find_param_spec (config, y_property_name, G_STRLOC);
  if (! y_param_spec)
    return NULL;

  if (unit_property_name)
    {
      unit_param_spec = check_param_spec (config, unit_property_name,
                                          GIMP_TYPE_PARAM_UNIT, G_STRLOC);
      if (! unit_param_spec)
        return NULL;

      g_object_get (config,
                    unit_property_name, &unit_value,
                    NULL);
    }
  else
    {
      unit_param_spec = NULL;
      unit_value      = GIMP_UNIT_INCH;
    }

  if (G_IS_PARAM_SPEC_INT (x_param_spec) &&
      G_IS_PARAM_SPEC_INT (y_param_spec))
    {
      gint int_x;
      gint int_y;

      g_object_get (config,
                    x_property_name, &int_x,
                    y_property_name, &int_y,
                    NULL);

      x_value = int_x;
      y_value = int_y;
    }
  else if (G_IS_PARAM_SPEC_DOUBLE (x_param_spec) &&
           G_IS_PARAM_SPEC_DOUBLE (y_param_spec))
    {
      g_object_get (config,
                    x_property_name, &x_value,
                    y_property_name, &y_value,
                    NULL);
    }
  else
    {
      g_warning (G_STRLOC ": properties '%s' and '%s' of %s are not either "
                 " both int or both double",
                 x_property_name,
                 y_property_name,
                 g_type_name (G_TYPE_FROM_INSTANCE (config)));
      return NULL;
    }

  sizeentry = gimp_size_entry_new (2, unit_value, unit_format,
                                   FALSE, FALSE, TRUE, 10,
                                   update_policy);

  switch (update_policy)
    {
    case GIMP_SIZE_ENTRY_UPDATE_SIZE:
      gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (sizeentry), 0,
                                      xresolution, FALSE);
      gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (sizeentry), 1,
                                      yresolution, FALSE);

      gimp_size_entry_set_refval_boundaries (GIMP_SIZE_ENTRY (sizeentry), 0,
                                             GIMP_MIN_IMAGE_SIZE,
                                             GIMP_MAX_IMAGE_SIZE);
      gimp_size_entry_set_refval_boundaries (GIMP_SIZE_ENTRY (sizeentry), 1,
                                             GIMP_MIN_IMAGE_SIZE,
                                             GIMP_MAX_IMAGE_SIZE);

      if (ABS (x_value - y_value) < 1)
        chain_checked = TRUE;
      break;

    case GIMP_SIZE_ENTRY_UPDATE_RESOLUTION:
      gimp_size_entry_set_refval_boundaries (GIMP_SIZE_ENTRY (sizeentry), 0,
                                             GIMP_MIN_RESOLUTION,
                                             GIMP_MAX_RESOLUTION);
      gimp_size_entry_set_refval_boundaries (GIMP_SIZE_ENTRY (sizeentry), 1,
                                             GIMP_MIN_RESOLUTION,
                                             GIMP_MAX_RESOLUTION);

      if (ABS (x_value - y_value) < GIMP_MIN_RESOLUTION)
        chain_checked = TRUE;
      break;

    default:
      break;
    }

  gimp_size_entry_set_unit   (GIMP_SIZE_ENTRY (sizeentry), unit_value);
  gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (sizeentry), 0, x_value);
  gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (sizeentry), 1, y_value);

  g_object_set_data (G_OBJECT (sizeentry), "gimp-config-param-spec-x",
                     x_param_spec);
  g_object_set_data (G_OBJECT (sizeentry), "gimp-config-param-spec-y",
                     y_param_spec);
  g_object_set_data (G_OBJECT (sizeentry), "gimp-config-param-spec-unit",
                     unit_param_spec);

  if (has_chainbutton)
    {
      GtkWidget *button;

      button = gimp_chain_button_new (GIMP_CHAIN_BOTTOM);

      if (chain_checked)
        gimp_chain_button_set_active (GIMP_CHAIN_BUTTON (button), TRUE);

      gtk_table_attach_defaults (GTK_TABLE (sizeentry), button, 1, 3, 3, 4);
      gtk_widget_show (button);

      g_object_set_data (G_OBJECT (sizeentry), "chainbutton", button);
    }

  g_signal_connect (sizeentry, "value_changed",
		    G_CALLBACK (gimp_prop_coordinates_callback),
		    config);
  g_signal_connect (sizeentry, "refval_changed",
		    G_CALLBACK (gimp_prop_coordinates_callback),
		    config);

  if (unit_property_name)
    {
      g_signal_connect (sizeentry, "unit_changed",
                        G_CALLBACK (gimp_prop_coordinates_callback),
                        config);
    }

  connect_notify (config, x_property_name,
                  G_CALLBACK (gimp_prop_coordinates_notify_x),
                  sizeentry);
  connect_notify (config, y_property_name,
                  G_CALLBACK (gimp_prop_coordinates_notify_y),
                  sizeentry);

  if (unit_property_name)
    {
      connect_notify (config, unit_property_name,
                      G_CALLBACK (gimp_prop_coordinates_notify_unit),
                      sizeentry);
    }

  return sizeentry;
}

static void
gimp_prop_coordinates_callback (GimpSizeEntry *sizeentry,
                                GObject       *config)
{
  GParamSpec *x_param_spec;
  GParamSpec *y_param_spec;
  GParamSpec *unit_param_spec;
  gdouble     x_value;
  gdouble     y_value;
  GimpUnit    unit_value;
  gdouble    *old_x_value;
  gdouble    *old_y_value;

  x_param_spec = g_object_get_data (G_OBJECT (sizeentry),
                                    "gimp-config-param-spec-x");
  y_param_spec = g_object_get_data (G_OBJECT (sizeentry),
                                    "gimp-config-param-spec-y");
  unit_param_spec = g_object_get_data (G_OBJECT (sizeentry),
                                       "gimp-config-param-spec-unit");

  if (! x_param_spec || ! y_param_spec)
    return;

  x_value    = gimp_size_entry_get_refval (sizeentry, 0);
  y_value    = gimp_size_entry_get_refval (sizeentry, 1);
  unit_value = gimp_size_entry_get_unit (sizeentry);

  old_x_value = g_object_get_data (G_OBJECT (sizeentry), "old-x-value");

  if (! old_x_value)
    {
      old_x_value = g_new0 (gdouble, 1);
      *old_x_value = 0.0;
      g_object_set_data_full (G_OBJECT (sizeentry), "old-x-value",
                              old_x_value,
                              (GDestroyNotify) g_free);
    }

  old_y_value = g_object_get_data (G_OBJECT (sizeentry), "old-y-value");

  if (! old_y_value)
    {
      old_y_value = g_new0 (gdouble, 1);
      *old_y_value = 0.0;
      g_object_set_data_full (G_OBJECT (sizeentry), "old-y-value",
                              old_y_value,
                              (GDestroyNotify) g_free);
    }

  if (x_value != y_value)
    {
      GtkWidget *chainbutton;

      chainbutton = g_object_get_data (G_OBJECT (sizeentry), "chainbutton");

      if (chainbutton &&
          gimp_chain_button_get_active (GIMP_CHAIN_BUTTON (chainbutton)))
        {
          if (x_value != *old_x_value)
            {
              *old_x_value = x_value;
              gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (sizeentry), 1,
                                          x_value);
              return;
            }

          if (y_value != *old_y_value)
            {
              *old_y_value = y_value;
              gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (sizeentry), 0,
                                          y_value);
              return;
            }
        }
    }

  if (*old_x_value == x_value && *old_y_value == y_value)
    return;

  *old_x_value = x_value;
  *old_y_value = y_value;

  if (G_IS_PARAM_SPEC_INT (x_param_spec) &&
      G_IS_PARAM_SPEC_INT (y_param_spec))
    {
      g_object_set (config,
                    x_param_spec->name,    (gint) x_value,
                    y_param_spec->name,    (gint) y_value,

                    unit_param_spec ?
                    unit_param_spec->name : NULL, unit_value,

                    NULL);
    }
  else if (G_IS_PARAM_SPEC_DOUBLE (x_param_spec) &&
           G_IS_PARAM_SPEC_DOUBLE (y_param_spec))
    {
      g_object_set (config,
                    x_param_spec->name,    x_value,
                    y_param_spec->name,    y_value,

                    unit_param_spec ?
                    unit_param_spec->name : NULL, unit_value,

                    NULL);
    }
}

static void
gimp_prop_coordinates_notify_x (GObject       *config,
                                GParamSpec    *param_spec,
                                GimpSizeEntry *sizeentry)
{
  gdouble value;

  if (G_IS_PARAM_SPEC_INT (param_spec))
    {
      gint int_value;

      g_object_get (config,
                    param_spec->name, &int_value,
                    NULL);

      value = int_value;
    }
  else
    {
      g_object_get (config,
                    param_spec->name, &value,
                    NULL);
    }

  if (value != gimp_size_entry_get_refval (sizeentry, 0))
    {
      g_signal_handlers_block_by_func (sizeentry,
                                       gimp_prop_coordinates_callback,
                                       config);

      gimp_size_entry_set_refval (sizeentry, 0, value);

      g_signal_handlers_unblock_by_func (sizeentry,
                                         gimp_prop_coordinates_callback,
                                         config);
    }
}

static void
gimp_prop_coordinates_notify_y (GObject       *config,
                                GParamSpec    *param_spec,
                                GimpSizeEntry *sizeentry)
{
  gdouble value;

  if (G_IS_PARAM_SPEC_INT (param_spec))
    {
      gint int_value;

      g_object_get (config,
                    param_spec->name, &int_value,
                    NULL);

      value = int_value;
    }
  else
    {
      g_object_get (config,
                    param_spec->name, &value,
                    NULL);
    }

  if (value != gimp_size_entry_get_refval (sizeentry, 1))
    {
      g_signal_handlers_block_by_func (sizeentry,
                                       gimp_prop_coordinates_callback,
                                       config);

      gimp_size_entry_set_refval (sizeentry, 1, value);

      g_signal_handlers_unblock_by_func (sizeentry,
                                         gimp_prop_coordinates_callback,
                                         config);
    }
}

static void
gimp_prop_coordinates_notify_unit (GObject       *config,
                                   GParamSpec    *param_spec,
                                   GimpSizeEntry *sizeentry)
{
  GimpUnit value;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  if (value != gimp_size_entry_get_unit (sizeentry))
    {
      g_signal_handlers_block_by_func (sizeentry,
                                       gimp_prop_coordinates_callback,
                                       config);

      gimp_size_entry_set_unit (sizeentry, value);

      g_signal_handlers_unblock_by_func (sizeentry,
                                         gimp_prop_coordinates_callback,
                                         config);
    }
}


/******************/
/*  color button  */
/******************/

static void   gimp_prop_color_button_callback (GtkWidget  *widget,
                                               GObject    *config);
static void   gimp_prop_color_button_notify   (GObject    *config,
                                               GParamSpec *param_spec,
                                               GtkWidget  *button);

GtkWidget *
gimp_prop_color_button_new (GObject           *config,
                            const gchar       *property_name,
                            const gchar       *title,
                            gint               width,
                            gint               height,
                            GimpColorAreaType  type)
{
  GParamSpec *param_spec;
  GtkWidget  *button;
  GimpRGB    *value;

  param_spec = check_param_spec (config, property_name,
                                 GIMP_TYPE_PARAM_COLOR, G_STRLOC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  button = gimp_color_panel_new (title, value, type, width, height);

  g_free (value);

  set_param_spec (G_OBJECT (button), param_spec);

  g_signal_connect (button, "color_changed",
		    G_CALLBACK (gimp_prop_color_button_callback),
		    config);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_color_button_notify),
                  button);

  return button;
}

static void
gimp_prop_color_button_callback (GtkWidget *button,
                                 GObject   *config)
{
  GParamSpec *param_spec;
  GimpRGB     value;

  param_spec = get_param_spec (G_OBJECT (button));
  if (! param_spec)
    return;

  gimp_color_button_get_color (GIMP_COLOR_BUTTON (button), &value);

  g_signal_handlers_block_by_func (config,
                                   gimp_prop_color_button_notify,
                                   button);

  g_object_set (config,
                param_spec->name, &value,
                NULL);

  g_signal_handlers_unblock_by_func (config,
                                     gimp_prop_color_button_notify,
                                     button);
}

static void
gimp_prop_color_button_notify (GObject    *config,
                               GParamSpec *param_spec,
                               GtkWidget  *button)
{
  GimpRGB *value;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  g_signal_handlers_block_by_func (button,
                                   gimp_prop_color_button_callback,
                                   config);

  gimp_color_button_set_color (GIMP_COLOR_BUTTON (button), value);

  g_free (value);

  g_signal_handlers_unblock_by_func (button,
                                     gimp_prop_color_button_callback,
                                     config);
}


/*******************************/
/*  private utility functions  */
/*******************************/

static GQuark param_spec_quark = 0;

static void
set_param_spec (GObject     *object,
                GParamSpec  *value)
{
  if (! param_spec_quark)
    param_spec_quark = g_quark_from_static_string ("gimp-config-param-spec");

  g_object_set_qdata (object, param_spec_quark, value);
}

static GParamSpec *
get_param_spec (GObject *object)
{
  if (! param_spec_quark)
    param_spec_quark = g_quark_from_static_string ("gimp-config-param-spec");

  return g_object_get_qdata (object, param_spec_quark);
}

static GParamSpec *
find_param_spec (GObject     *object,
                 const gchar *property_name,
                 const gchar *strloc)
{
  GParamSpec *param_spec;

  param_spec = g_object_class_find_property (G_OBJECT_GET_CLASS (object),
                                             property_name);

  if (! param_spec)
    g_warning ("%s: %s has no property named '%s'",
               strloc,
               g_type_name (G_TYPE_FROM_INSTANCE (object)),
               property_name);

  return param_spec;
}

static GParamSpec *
check_param_spec (GObject     *object,
                  const gchar *property_name,
                  GType        type,
                  const gchar *strloc)
{
  GParamSpec *param_spec;

  param_spec = find_param_spec (object, property_name, strloc);

  if (param_spec && ! g_type_is_a (G_TYPE_FROM_INSTANCE (param_spec), type))
    {
      g_warning ("%s: property '%s' of %s is not a %s",
                 strloc,
                 param_spec->name,
                 g_type_name (param_spec->owner_type),
                 g_type_name (type));
      return NULL;
    }

  return param_spec;
}

static void
connect_notify (GObject     *config,
                const gchar *property_name,
                GCallback    callback,
                gpointer     callback_data)
{
  gchar *notify_name;

  notify_name = g_strconcat ("notify::", property_name, NULL);

  g_signal_connect_object (config, notify_name, callback, callback_data, 0);

  g_free (notify_name);
}
