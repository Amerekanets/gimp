/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * module_db.c (C) 1999 Austin Donnelly <austin@gimp.org>
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

#include <glib.h>

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <time.h>

#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "core/core-types.h"

#include "core/gimplist.h"

#include "appenv.h"
#include "module_db.h"
#include "gimprc.h"
#include "datafiles.h"

#include "libgimp/gimpmodule.h"

#include "libgimp/gimpintl.h"


typedef enum
{
  ST_MODULE_ERROR,         /* missing module_load function or other error    */
  ST_LOADED_OK,            /* happy and running (normal state of affairs)    */
  ST_LOAD_FAILED,          /* module_load returned GIMP_MODULE_UNLOAD        */
  ST_UNLOAD_REQUESTED,     /* sent unload request, waiting for callback      */
  ST_UNLOADED_OK           /* callback arrived, module not in memory anymore */
} module_state;

static const gchar * const statename[] =
{
  N_("Module error"),
  N_("Loaded OK"),
  N_("Load failed"),
  N_("Unload requested"),
  N_("Unloaded OK")
};

#ifdef __EMX__
extern void gimp_color_selector_register   ();
extern void gimp_color_selector_unregister ();
extern void dialog_register                ();
extern void dialog_unregister              ();

static struct main_funcs_struc
{
  gchar *name;
  void (*func) ();
} 

gimp_main_funcs[] =
{
  { "gimp_color_selector_register",   gimp_color_selector_register },
  { "gimp_color_selector_unregister", gimp_color_selector_unregister },
  { "dialog_register",                dialog_register },
  { "dialog_unregister",              dialog_unregister },
  { NULL, NULL }
};
#endif


/* one of these objects is kept per-module */
typedef struct
{
  GtkObject       parent_instance;

  gchar          *fullpath;     /* path to the module                        */
  module_state    state;        /* what's happened to the module             */
  gboolean        ondisk;       /* TRUE if file still exists                 */
  gboolean        load_inhibit; /* user requests not to load at boot time    */
  gint            refs;         /* how many time we're running in the module */

  /* stuff from now on may be NULL depending on the state the module is in   */
  GimpModuleInfo *info;         /* returned values from module_init          */
  GModule        *module;       /* handle on the module                      */
  gchar          *last_module_error;

  GimpModuleInitFunc   init;
  GimpModuleUnloadFunc unload;
} ModuleInfo;


static guint module_info_get_type (void);

#define MODULE_INFO_TYPE    (module_info_get_type ())
#define MODULE_INFO(obj)    (GTK_CHECK_CAST (obj, MODULE_INFO_TYPE, ModuleInfo))
#define IS_MODULE_INFO(obj) (GTK_CHECK_TYPE (obj, MODULE_INFO_TYPE))


#define NUM_INFO_LINES 7

typedef struct
{
  GtkWidget  *table;
  GtkWidget  *label[NUM_INFO_LINES];
  GtkWidget  *button_label;
  ModuleInfo *last_update;
  GtkWidget  *button;
  GtkWidget  *list;
  GtkWidget  *load_inhibit_check;
} BrowserState;

/* global set of module_info pointers */
static GimpContainer *modules;
static GQuark         modules_handler_id;

/* If the inhibit state of any modules changes, we might need to
 * re-write the modulerc.
 */
static gboolean need_to_rewrite_modulerc = FALSE;


/* debug control: */

/*#define DUMP_DB*/
/*#define DEBUG*/

#ifdef DEBUG
#undef DUMP_DB
#define DUMP_DB
#define TRC(x) printf x
#else
#define TRC(x)
#endif


/* prototypes */
static void         module_initialize      (const gchar   *filename,
					    gpointer       loader_data);
static void         mod_load               (ModuleInfo    *mod,
					    gboolean       verbose);
static void         mod_unload             (ModuleInfo    *mod,
					    gboolean       verbose);
static gboolean     mod_idle_unref         (ModuleInfo    *mod);
static ModuleInfo * module_find_by_path    (const gchar   *fullpath);

#ifdef DUMP_DB
static void   print_module_info            (gpointer       data,
					    gpointer       user_data);
#endif

static void   browser_popdown_callback     (GtkWidget     *widget,
					    gpointer       data);
static void   browser_destroy_callback     (GtkWidget     *widget,
					    gpointer       data);
static void   browser_info_update          (ModuleInfo    *mod,
					    BrowserState  *st);
static void   browser_info_add             (GimpContainer *container,
					    ModuleInfo    *mod,
					    BrowserState  *st);
static void   browser_info_remove          (GimpContainer *container,
					    ModuleInfo    *mod,
					    BrowserState  *st);
static void   browser_info_init            (BrowserState  *st,
					    GtkWidget     *table);
static void   browser_select_callback      (GtkWidget     *widget,
					    GtkWidget     *child);
static void   browser_load_unload_callback (GtkWidget     *widget,
					    gpointer       data);
static void   browser_refresh_callback     (GtkWidget     *widget,
					    gpointer       data);
static void   make_list_item               (gpointer       data,
					    gpointer       user_data);

static void   gimp_module_ref              (ModuleInfo   *mod);
static void   gimp_module_unref            (ModuleInfo   *mod);



/**************************************************************/
/* Exported functions */

void
module_db_init (void)
{
  gchar *filename;

  /* load the modulerc file */
  filename = gimp_personal_rc_file ("modulerc");
  parse_gimprc_file (filename);
  g_free (filename);

  /* Load and initialize gimp modules */

  modules = gimp_list_new (MODULE_INFO_TYPE, GIMP_CONTAINER_POLICY_WEAK);

  if (g_module_supported ())
    gimp_datafiles_read_directories (module_path, 0 /* no flags */,
				     module_initialize, NULL);
#ifdef DUMP_DB
  gimp_container_foreach (modules, print_module_info, NULL);
#endif
}

static void
free_a_single_module (gpointer data, 
		      gpointer user_data)
{
  ModuleInfo *mod = data;

  if (mod->module && mod->unload && mod->state == ST_LOADED_OK)
    {
      mod_unload (mod, FALSE);
    }
}

static void
add_to_inhibit_string (gpointer data, 
		       gpointer user_data)
{
  ModuleInfo *mod = data;
  GString    *str = user_data;

  if (mod->load_inhibit)
    {
      str = g_string_append_c (str, G_SEARCHPATH_SEPARATOR);
      str = g_string_append (str, mod->fullpath);
    }
}


static gboolean
module_db_write_modulerc (void)
{
  GString  *str;
  gchar    *p;
  gchar    *filename;
  FILE     *fp;
  gboolean  saved = FALSE;

  str = g_string_new (NULL);
  gimp_container_foreach (modules, add_to_inhibit_string, str);
  if (str->len > 0)
    p = str->str + 1;
  else
    p = "";

  filename = gimp_personal_rc_file ("modulerc");
  fp = fopen (filename, "wt");
  g_free (filename);
  if (fp)
    {
      fprintf (fp, "(module-load-inhibit \"%s\")\n", p);
      fclose (fp);
      saved = TRUE;
    }

  g_string_free (str, TRUE);
  return (saved);
}


void
module_db_free (void)
{
  if (need_to_rewrite_modulerc)
    {
      if (module_db_write_modulerc ())
	{
	  need_to_rewrite_modulerc = FALSE;
	}
    }
  gimp_container_foreach (modules, free_a_single_module, NULL);
}


GtkWidget *
module_db_browser_new (void)
{
  GtkWidget *shell;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *listbox;
  GtkWidget *button;
  BrowserState *st;

  shell = gimp_dialog_new (_("Module DB"), "module_db_dialog",
			   gimp_standard_help_func,
			   "dialogs/module_browser.html",
			   GTK_WIN_POS_NONE,
			   FALSE, TRUE, FALSE,

			   _("OK"), browser_popdown_callback,
			   NULL, NULL, NULL, TRUE, TRUE,

			   NULL);

  vbox = gtk_vbox_new (FALSE, 5);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (shell)->vbox), vbox);
  gtk_widget_show (vbox);

  listbox = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (listbox),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (vbox), listbox, TRUE, TRUE, 0);
  gtk_widget_set_usize (listbox, 125, 100);
  gtk_widget_show (listbox);

  st = g_new0 (BrowserState, 1);

  st->list = gtk_list_new ();
  gtk_list_set_selection_mode (GTK_LIST (st->list), GTK_SELECTION_BROWSE);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (listbox),
					 st->list);

  gimp_container_foreach (modules, make_list_item, st);

  gtk_widget_show (st->list);

  st->table = gtk_table_new (5, NUM_INFO_LINES + 1, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (st->table), 4);  
  gtk_box_pack_start (GTK_BOX (vbox), st->table, FALSE, FALSE, 0);
  gtk_widget_show (st->table);

  hbox = gtk_hbutton_box_new ();
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_SPREAD);

  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 5);

  button = gtk_button_new_with_label (_("Refresh"));
  gtk_widget_show (button);
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      browser_refresh_callback, st);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

  st->button = gtk_button_new_with_label ("");
  st->button_label = GTK_BIN (st->button)->child;
  gtk_box_pack_start (GTK_BOX (hbox), st->button, TRUE, TRUE, 0);
  gtk_widget_show (st->button);
  gtk_signal_connect (GTK_OBJECT (st->button), "clicked",
		      browser_load_unload_callback, st);

  browser_info_init (st, st->table);
  browser_info_update (st->last_update, st);

  gtk_object_set_user_data (GTK_OBJECT (st->list), st);

  gtk_signal_connect (GTK_OBJECT (st->list), "select_child",
		      browser_select_callback, NULL);

  /* hook the GimpContainer signals so we can refresh the display
   * appropriately.
   */
  modules_handler_id =
    gimp_container_add_handler (modules, "modified", browser_info_update, st);

  gtk_signal_connect (GTK_OBJECT (modules), "add", 
		      browser_info_add, st);
  gtk_signal_connect (GTK_OBJECT (modules), "remove", 
		      browser_info_remove, st);

  gtk_signal_connect (GTK_OBJECT (shell), "destroy",
		      browser_destroy_callback, st);

  return shell;
}


/**************************/
/* ModuleInfo object glue */


enum
{
  MODIFIED,
  LAST_SIGNAL
};

typedef struct _ModuleInfoClass ModuleInfoClass;

struct _ModuleInfoClass
{
  GimpObjectClass  parent_class;

  void (* modified) (ModuleInfo *module_info);
};


static guint module_info_signals[LAST_SIGNAL];

static GimpObjectClass *parent_class = NULL;

static void
module_info_destroy (GtkObject *object)
{
  ModuleInfo *mod = MODULE_INFO (object);

  /* if this trips, then we're onto some serious lossage in a moment */
  g_return_if_fail (mod->refs == 0);

  if (mod->last_module_error)
    g_free (mod->last_module_error);
  g_free (mod->fullpath);

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
module_info_class_init (ModuleInfoClass *klass)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass *) klass;

  parent_class = gtk_type_class (GIMP_TYPE_OBJECT);

  module_info_signals[MODIFIED] =
    gtk_signal_new ("modified",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (ModuleInfoClass,
				       modified),
                    gtk_signal_default_marshaller,
                    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (object_class, module_info_signals, LAST_SIGNAL);

  object_class->destroy = module_info_destroy;

  klass->modified = NULL;
}

static void
module_info_init (ModuleInfo *mod)
{
  /* don't need to do anything */
}

static guint
module_info_get_type (void)
{
  static guint module_info_type = 0;

  if (!module_info_type)
    {
      static const GtkTypeInfo module_info_info =
      {
	"ModuleInfo",
	sizeof (ModuleInfo),
	sizeof (ModuleInfoClass),
	(GtkClassInitFunc) module_info_class_init,
	(GtkObjectInitFunc) module_info_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      module_info_type = gtk_type_unique (GIMP_TYPE_OBJECT, &module_info_info);
    }

  return module_info_type;
}

/* exported API: */

static void
module_info_modified (ModuleInfo *mod)
{
  gtk_signal_emit (GTK_OBJECT (mod), module_info_signals[MODIFIED]);
}

static ModuleInfo *
module_info_new (void)
{
  return MODULE_INFO (gtk_type_new (module_info_get_type ()));
}

static void
module_info_free (ModuleInfo *mod)
{
  gtk_object_unref (GTK_OBJECT (mod));
}


/**************************************************************/
/* helper functions */


/* name must be of the form lib*.so (Unix) or *.dll (Win32) */
static gboolean
valid_module_name (const gchar *filename)
{
  const gchar *basename;
  gint         len;

  basename = g_basename (filename);

  len = strlen (basename);

#if !defined(G_OS_WIN32) && !defined(G_WITH_CYGWIN) && !defined(__EMX__)
  if (len < 3 + 1 + 3)
    return FALSE;

  if (strncmp (basename, "lib", 3))
    return FALSE;

  if (strcmp (basename + len - 3, ".so"))
    return FALSE;
#else
  if (len < 1 + 4)
      return FALSE;

  if (g_strcasecmp (basename + len - 4, ".dll"))
    return FALSE;
#endif

  return TRUE;
}


static gboolean
module_inhibited (const gchar *fullpath, 
		  const gchar *inhibit_list)
{
  gchar       *p;
  gint         pathlen;
  const gchar *start;
  const gchar *end;

  /* common case optimisation: the list is empty */
  if (!inhibit_list || *inhibit_list == '\000')
    return FALSE;

  p = strstr (inhibit_list, fullpath);
  if (!p)
    return FALSE;

  /* we have a substring, but check for colons either side */
  start = p;
  while (start != inhibit_list && *start != G_SEARCHPATH_SEPARATOR)
      start--;
  if (*start == G_SEARCHPATH_SEPARATOR)
      start++;

  end = strchr (p, G_SEARCHPATH_SEPARATOR);
  if (!end)
    end = inhibit_list + strlen (inhibit_list);

  pathlen = strlen (fullpath);
  if ((end - start) == pathlen)
    return TRUE;
  else
    return FALSE;
}


static void
module_initialize (const gchar *filename,
		   gpointer     loader_data)
{
  ModuleInfo *mod;

  if (!valid_module_name (filename))
    return;

  /* don't load if we already know about it */
  if (module_find_by_path (filename))
    return;

  mod = module_info_new ();

  mod->fullpath          = g_strdup (filename);
  mod->ondisk            = TRUE;
  mod->state             = ST_MODULE_ERROR;

  mod->info              = NULL;
  mod->module            = NULL;
  mod->last_module_error = NULL;
  mod->init              = NULL;
  mod->unload            = NULL;

  /* Count of times main gimp is within the module.  Normally, this
   * will be 1, and we assume that the module won't call its
   * unload callback until it is satisfied that it's not in use any
   * more.  refs can be 2 temporarily while we're running the module's
   * unload function, to stop the module attempting to unload
   * itself.
   */
  mod->refs              = 0;

  mod->load_inhibit = module_inhibited (mod->fullpath, module_db_load_inhibit);
  if (!mod->load_inhibit)
    {
      if (be_verbose)
	g_print (_("load module: \"%s\"\n"), filename);

      mod_load (mod, TRUE);
    }
  else
    {
      if (be_verbose)
	g_print (_("skipping module: \"%s\"\n"), filename);

      mod->state = ST_UNLOADED_OK;
    }

  gimp_container_add (modules, GIMP_OBJECT (mod));
}

static void
mod_load (ModuleInfo *mod, 
	  gboolean    verbose)
{
  gpointer symbol;

  g_return_if_fail (mod->module == NULL);

  mod->module = g_module_open (mod->fullpath, G_MODULE_BIND_LAZY);
  if (!mod->module)
    {
      mod->state = ST_MODULE_ERROR;

      if (mod->last_module_error)
	g_free (mod->last_module_error);
      mod->last_module_error = g_strdup (g_module_error ());

      if (verbose)
	g_warning (_("module load error: %s: %s"),
		   mod->fullpath, mod->last_module_error);
      return;
    }

#ifdef __EMX__
  if (g_module_symbol (mod->module, "gimp_main_funcs", &symbol))
    {
      *(struct main_funcs_struc **) symbol = gimp_main_funcs;
    }
#endif
  /* find the module_init symbol */
  if (!g_module_symbol (mod->module, "module_init", &symbol))
    {
      mod->state = ST_MODULE_ERROR;

      if (mod->last_module_error)
	g_free (mod->last_module_error);
      mod->last_module_error = g_strdup ("missing module_init() symbol");

      if (verbose)
	g_warning ("%s: module_init() symbol not found", mod->fullpath);

      g_module_close (mod->module);
      mod->module = NULL;
      mod->info   = NULL;
      return;
    }

  /* run module's initialisation */
  mod->init = symbol;
  mod->info = NULL;
  gimp_module_ref (mod); /* loaded modules are assumed to have a ref of 1 */
  if (mod->init (&mod->info) == GIMP_MODULE_UNLOAD)
    {
      mod->state = ST_LOAD_FAILED;
      gimp_module_unref (mod);
      mod->info = NULL;
      return;
    }

  /* module is now happy */
  mod->state = ST_LOADED_OK;
  TRC (("loaded module %s, state at %p\n", mod->fullpath, mod));

  /* do we have an unload function? */
  if (g_module_symbol (mod->module, "module_unload", &symbol))
    mod->unload = symbol;
  else
    mod->unload = NULL;
}


static void
mod_unload_completed_callback (gpointer data)
{
  ModuleInfo *mod = data;

  g_return_if_fail (mod->state == ST_UNLOAD_REQUESTED);

  /* lose the ref we gave this module when we loaded it,
   * since the module's now happy to be unloaded. */
  gimp_module_unref (mod);
  mod->info = NULL;

  mod->state = ST_UNLOADED_OK;

  TRC (("module unload completed callback for %p\n", mod));

  module_info_modified (mod);
}

static void
mod_unload (ModuleInfo *mod, 
	    gboolean    verbose)
{
  g_return_if_fail (mod->module != NULL);
  g_return_if_fail (mod->unload != NULL);

  if (mod->state == ST_UNLOAD_REQUESTED)
    return;

  mod->state = ST_UNLOAD_REQUESTED;

  TRC (("module unload requested for %p\n", mod));

  /* Send the unload request.  Need to ref the module so we don't
   * accidentally unload it while this call is in progress (eg if the
   * callback is called before the unload function returns). */
  gimp_module_ref (mod);
  mod->unload (mod->info->shutdown_data, mod_unload_completed_callback, mod);

  gtk_idle_add ((GtkFunction) mod_idle_unref, (gpointer) mod);
}

static gboolean
mod_idle_unref (ModuleInfo *mod)
{
  gimp_module_unref (mod);

  return FALSE;
}

#ifdef DUMP_DB
static void
print_module_info (gpointer data, 
		   gpointer user_data)
{
  ModuleInfo *i = data;

  g_print ("\n%s: %s\n",
	   i->fullpath, statename[i->state]);
  g_print ("  module:%p  lasterr:%s  init:%p  unload:%p\n",
	   i->module, i->last_module_error? i->last_module_error : "NONE",
	   i->init, i->unload);
  if (i->info)
    {
      g_print ("  shutdown_data: %p\n"
	       "  purpose:   %s\n"
	       "  author:    %s\n"
	       "  version:   %s\n"
	       "  copyright: %s\n"
	       "  date:      %s\n",
	       i->info->shutdown_data,
	       i->info->purpose, i->info->author, i->info->version,
	       i->info->copyright, i->info->date);
    }
}
#endif



/**************************************************************/
/* UI functions */

static void
browser_popdown_callback (GtkWidget *widget,
			  gpointer   data)
{
  gtk_widget_destroy (GTK_WIDGET (data));
}

static void
browser_destroy_callback (GtkWidget *widget,
			  gpointer   data)
{
  gtk_signal_disconnect_by_data (GTK_OBJECT (modules), data);
  gimp_container_remove_handler (modules, modules_handler_id);
  g_free (data);
}

static void
browser_load_inhibit_callback (GtkWidget *widget,
			       gpointer   data)
{
  BrowserState *st = data;
  gboolean new_value;

  g_return_if_fail (st->last_update != NULL);

  new_value = ! GTK_TOGGLE_BUTTON (widget)->active;

  if (new_value == st->last_update->load_inhibit)
    return;

  st->last_update->load_inhibit = new_value;
  module_info_modified (st->last_update);

  need_to_rewrite_modulerc = TRUE;
}

static void
browser_info_update (ModuleInfo   *mod, 
		     BrowserState *st)
{
  gint i;
  const gchar *text[NUM_INFO_LINES - 1];
  gchar *status;

  /* only update the info if we're actually showing it */
  if (mod != st->last_update)
    return;

  if (!mod)
    {
      for (i=0; i < NUM_INFO_LINES; i++)
	gtk_label_set_text (GTK_LABEL (st->label[i]), "");
      gtk_label_set_text (GTK_LABEL(st->button_label), _("<No modules>"));
      gtk_widget_set_sensitive (GTK_WIDGET (st->button), FALSE);
      gtk_widget_set_sensitive (GTK_WIDGET (st->load_inhibit_check), FALSE);
      return;
    }

  if (mod->info)
    {
      text[0] = mod->info->purpose;
      text[1] = mod->info->author;
      text[2] = mod->info->version;
      text[3] = mod->info->copyright;
      text[4] = mod->info->date;
      text[5] = mod->ondisk? _("on disk") : _("only in memory");
    }
  else
    {
      text[0] = "--";
      text[1] = "--";
      text[2] = "--";
      text[3] = "--";
      text[4] = "--";
      text[5] = mod->ondisk? _("on disk") : _("nowhere (click 'refresh')");
    }

  if (mod->state == ST_MODULE_ERROR && mod->last_module_error)
    status = g_strdup_printf ("%s (%s)", gettext (statename[mod->state]),
			      mod->last_module_error);
  else
    {
      status = g_strdup (gettext (statename[mod->state]));
    }

  for (i=0; i < NUM_INFO_LINES - 1; i++)
    {
      gtk_label_set_text (GTK_LABEL (st->label[i]), gettext (text[i]));
    }

  gtk_label_set_text (GTK_LABEL (st->label[NUM_INFO_LINES-1]), status);

  g_free (status);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (st->load_inhibit_check),
				!mod->load_inhibit);
  gtk_widget_set_sensitive (GTK_WIDGET (st->load_inhibit_check), TRUE);

  /* work out what the button should do (if anything) */
  switch (mod->state)
    {
    case ST_MODULE_ERROR:
    case ST_LOAD_FAILED:
    case ST_UNLOADED_OK:
      gtk_label_set_text (GTK_LABEL(st->button_label), _("Load"));
      gtk_widget_set_sensitive (GTK_WIDGET (st->button), mod->ondisk);
      break;

    case ST_UNLOAD_REQUESTED:
      gtk_widget_set_sensitive (GTK_WIDGET (st->button), FALSE);
      break;

    case ST_LOADED_OK:
      gtk_label_set_text (GTK_LABEL(st->button_label), _("Unload"));
      gtk_widget_set_sensitive (GTK_WIDGET (st->button),
				mod->unload? TRUE : FALSE);
      break;    
    }
}

static void
browser_info_init (BrowserState *st, 
		   GtkWidget    *table)
{
  GtkWidget *label;
  gint i;

  gchar *text[] =
  {
    N_("Purpose:"),
    N_("Author:"),
    N_("Version:"),
    N_("Copyright:"),
    N_("Date:"),
    N_("Location:"),
    N_("State:")
  };

  for (i=0; i < sizeof(text) / sizeof(char *); i++)
    {
      label = gtk_label_new (gettext (text[i]));
      gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
      gtk_table_attach (GTK_TABLE (table), label, 0, 1, i, i+1,
			GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 2);
      gtk_widget_show (label);

      st->label[i] = gtk_label_new ("");
      gtk_misc_set_alignment (GTK_MISC (st->label[i]), 0.0, 0.5);
      gtk_table_attach (GTK_TABLE (st->table), st->label[i], 1, 2, i, i+1,
			GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 2);
      gtk_widget_show (st->label[i]);
    }

  st->load_inhibit_check =
    gtk_check_button_new_with_label (_("Autoload during startup"));
  gtk_widget_show (st->load_inhibit_check);
  gtk_table_attach (GTK_TABLE (table), st->load_inhibit_check,
		    0, 2, i, i+1,
		    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 2);
  gtk_signal_connect (GTK_OBJECT (st->load_inhibit_check), "toggled",
		      browser_load_inhibit_callback, st);
}

static void
browser_select_callback (GtkWidget *widget, 
			 GtkWidget *child)
{
  ModuleInfo *i;
  BrowserState *st;

  i = gtk_object_get_user_data (GTK_OBJECT (child));
  st = gtk_object_get_user_data (GTK_OBJECT (widget));

  if (st->last_update == i)
    return;

  st->last_update = i;

  browser_info_update (st->last_update, st);
}


static void
browser_load_unload_callback (GtkWidget *widget, 
			      gpointer   data)
{
  BrowserState *st = data;

  if (st->last_update->state == ST_LOADED_OK)
    mod_unload (st->last_update, FALSE);
  else
    mod_load (st->last_update, FALSE);

  module_info_modified (st->last_update);
}


static void
make_list_item (gpointer data, 
		gpointer user_data)
{
  ModuleInfo   *info = data;
  BrowserState *st = user_data;
  GtkWidget    *list_item;

  if (!st->last_update)
    st->last_update = info;

  list_item = gtk_list_item_new_with_label (info->fullpath);

  gtk_widget_show (list_item);
  gtk_object_set_user_data (GTK_OBJECT (list_item), info);

  gtk_container_add (GTK_CONTAINER (st->list), list_item);
}


static void
browser_info_add (GimpContainer *container,
		  ModuleInfo    *mod, 
		  BrowserState  *st)
{
  make_list_item (mod, st);
}


static void
browser_info_remove (GimpContainer *container,
		     ModuleInfo    *mod, 
		     BrowserState  *st)
{
  GList      *dlist;
  GList      *free_list;
  GtkWidget  *list_item;
  ModuleInfo *i;

  dlist = gtk_container_children (GTK_CONTAINER (st->list));
  free_list = dlist;

  while (dlist)
  {
    list_item = dlist->data;

    i = gtk_object_get_user_data (GTK_OBJECT (list_item));
    g_return_if_fail (i != NULL);

    if (i == mod)
    {
      gtk_container_remove (GTK_CONTAINER (st->list), list_item);
      g_list_free(free_list);
      return;
    }

    dlist = dlist->next;
  }

  g_warning ("tried to remove module that wasn't in brower's list");
  g_list_free(free_list);
}



static void
module_db_module_ondisk (gpointer data, 
			 gpointer user_data)
{
  ModuleInfo *mod = data;
  struct stat statbuf;
  gint ret;
  gint old_ondisk = mod->ondisk;
  GSList **kill_list = user_data;

  ret = stat (mod->fullpath, &statbuf);
  if (ret != 0)
    mod->ondisk = FALSE;
  else
    mod->ondisk = TRUE;

  /* if it's not on the disk, and it isn't in memory, mark it to be
   * removed later. */
  if (!mod->ondisk && !mod->module)
    {
      *kill_list = g_slist_append (*kill_list, mod);
      mod = NULL;
    }

  if (mod && mod->ondisk != old_ondisk)
    module_info_modified (mod);
}


static void
module_db_module_remove (gpointer data, 
			 gpointer user_data)
{
  ModuleInfo *mod = data;

  gimp_container_remove (modules, GIMP_OBJECT (mod));

  module_info_free (mod);
}



typedef struct
{
  const gchar *search_key;
  ModuleInfo  *found;
} find_by_path_closure;

static void
module_db_path_cmp (gpointer data, 
		    gpointer user_data)
{
  ModuleInfo *mod = data;
  find_by_path_closure *cl = user_data;

  if (!strcmp (mod->fullpath, cl->search_key))
    cl->found = mod;
}

static ModuleInfo *
module_find_by_path (const char *fullpath)
{
  find_by_path_closure cl;

  cl.found = NULL;
  cl.search_key = fullpath;

  gimp_container_foreach (modules, module_db_path_cmp, &cl);

  return cl.found;
}



static void
browser_refresh_callback (GtkWidget *widget, 
			  gpointer   data)
{
  GSList *kill_list = NULL;

  /* remove modules we don't have on disk anymore */
  gimp_container_foreach (modules, module_db_module_ondisk, &kill_list);
  g_slist_foreach (kill_list, module_db_module_remove, NULL);
  g_slist_free (kill_list);
  kill_list = NULL;

  /* walk filesystem and add new things we find */
  gimp_datafiles_read_directories (module_path, 0 /* no flags */,
				   module_initialize, NULL);
}


static void
gimp_module_ref (ModuleInfo *mod)
{
  g_return_if_fail (mod->refs >= 0);
  g_return_if_fail (mod->module != NULL);
  mod->refs++;
}

static void
gimp_module_unref (ModuleInfo *mod)
{
  g_return_if_fail (mod->refs > 0);
  g_return_if_fail (mod->module != NULL);

  mod->refs--;

  if (mod->refs == 0)
    {
      TRC (("module %p refs hit 0, g_module_closing it\n", mod));
      g_module_close (mod->module);
      mod->module = NULL;
    }
}
