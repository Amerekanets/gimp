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

#include <string.h>

#include <glib-object.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpbase/gimpprotocol.h"
#include "libgimpbase/gimpwire.h"

#include "plug-in-types.h"

#include "base/tile.h"
#include "base/tile-manager.h"

#include "core/gimp.h"
#include "core/gimpdrawable.h"

#include "plug-in.h"
#include "plug-ins.h"
#include "plug-in-def.h"
#include "plug-in-params.h"
#include "plug-in-proc.h"
#include "plug-in-shm.h"


typedef struct _PlugInBlocked PlugInBlocked;

struct _PlugInBlocked
{
  PlugIn *plug_in;
  gchar  *proc_name;
};


/*  local function prototypes  */

static void plug_in_handle_quit             (PlugIn          *plug_in);
static void plug_in_handle_tile_req         (PlugIn          *plug_in,
                                             GPTileReq       *tile_req);
static void plug_in_handle_proc_run         (PlugIn          *plug_in,
                                             GPProcRun       *proc_run);
static void plug_in_handle_proc_return_priv (PlugIn          *plug_in,
                                             GPProcReturn    *proc_return);
static void plug_in_handle_proc_return      (PlugIn          *plug_in,
                                             GPProcReturn    *proc_return);
#ifdef ENABLE_TEMP_RETURN
static void plug_in_handle_temp_proc_return (PlugIn          *plug_in,
                                             GPProcReturn    *proc_return);
#endif
static void plug_in_handle_proc_install     (PlugIn          *plug_in,
                                             GPProcInstall   *proc_install);
static void plug_in_handle_proc_uninstall   (PlugIn          *plug_in,
                                             GPProcUninstall *proc_uninstall);
static void plug_in_handle_extension_ack    (PlugIn          *plug_in);
static void plug_in_handle_has_init         (PlugIn          *plug_in);


/*  private variables  */

static GSList *blocked_plug_ins = NULL;


/*  public functions  */

void
plug_in_handle_message (PlugIn      *plug_in,
                        WireMessage *msg)
{
  switch (msg->type)
    {
    case GP_QUIT:
      plug_in_handle_quit (plug_in);
      break;

    case GP_CONFIG:
      g_warning ("plug_in_handle_message: "
		 "received a config message (should not happen)");
      plug_in_close (plug_in, TRUE);
      break;

    case GP_TILE_REQ:
      plug_in_handle_tile_req (plug_in, msg->data);
      break;

    case GP_TILE_ACK:
      g_warning ("plug_in_handle_message: "
		 "received a tile ack message (should not happen)");
      plug_in_close (plug_in, TRUE);
      break;

    case GP_TILE_DATA:
      g_warning ("plug_in_handle_message: "
		 "received a tile data message (should not happen)");
      plug_in_close (plug_in, TRUE);
      break;

    case GP_PROC_RUN:
      plug_in_handle_proc_run (plug_in, msg->data);
      break;

    case GP_PROC_RETURN:
      plug_in_handle_proc_return (plug_in, msg->data);
      break;

    case GP_TEMP_PROC_RUN:
      g_warning ("plug_in_handle_message: "
		 "received a temp proc run message (should not happen)");
      plug_in_close (plug_in, TRUE);
      break;

    case GP_TEMP_PROC_RETURN:
#ifdef ENABLE_TEMP_RETURN
      plug_in_handle_temp_proc_return (plug_in, msg->data);
#else
      g_warning ("plug_in_handle_message: "
		 "received a temp proc return message (should not happen)");
      plug_in_close (plug_in, TRUE);
#endif
      break;

    case GP_PROC_INSTALL:
      plug_in_handle_proc_install (plug_in, msg->data);
      break;

    case GP_PROC_UNINSTALL:
      plug_in_handle_proc_uninstall (plug_in, msg->data);
      break;

    case GP_EXTENSION_ACK:
      plug_in_handle_extension_ack (plug_in);
      break;

    case GP_HAS_INIT:
      plug_in_handle_has_init (plug_in);
      break;
    }
}


/*  private functions  */

static void
plug_in_handle_quit (PlugIn *plug_in)
{
  plug_in_close (plug_in, FALSE);
}

static void
plug_in_handle_tile_req (PlugIn    *plug_in,
                         GPTileReq *tile_req)
{
  GPTileData   tile_data;
  GPTileData  *tile_info;
  WireMessage  msg;
  TileManager *tm;
  Tile        *tile;
  gint         shm_ID;

  shm_ID = plug_in_shm_get_ID (plug_in->gimp);

  if (tile_req->drawable_ID == -1)
    {
      /*  this branch communicates with libgimp/gimptile.c:gimp_tile_put()  */

      tile_data.drawable_ID = -1;
      tile_data.tile_num    = 0;
      tile_data.shadow      = 0;
      tile_data.bpp         = 0;
      tile_data.width       = 0;
      tile_data.height      = 0;
      tile_data.use_shm     = (shm_ID == -1) ? FALSE : TRUE;
      tile_data.data        = NULL;

      if (! gp_tile_data_write (plug_in->my_write, &tile_data, plug_in))
	{
	  g_warning ("plug_in_handle_tile_req: ERROR");
	  plug_in_close (plug_in, TRUE);
	  return;
	}

      if (! wire_read_msg (plug_in->my_read, &msg, plug_in))
	{
	  g_warning ("plug_in_handle_tile_req: ERROR");
	  plug_in_close (plug_in, TRUE);
	  return;
	}

      if (msg.type != GP_TILE_DATA)
	{
	  g_warning ("expected tile data and received: %d", msg.type);
	  plug_in_close (plug_in, TRUE);
	  return;
	}

      tile_info = msg.data;

      if (tile_info->shadow)
	tm = gimp_drawable_shadow ((GimpDrawable *)
                                   gimp_item_get_by_ID (plug_in->gimp,
                                                        tile_info->drawable_ID));
      else
	tm = gimp_drawable_data ((GimpDrawable *)
                                 gimp_item_get_by_ID (plug_in->gimp,
                                                      tile_info->drawable_ID));

      if (!tm)
	{
	  g_warning ("plug-in requested invalid drawable (killing)");
	  plug_in_close (plug_in, TRUE);
	  return;
	}

      tile = tile_manager_get (tm, tile_info->tile_num, TRUE, TRUE);
      if (!tile)
	{
	  g_warning ("plug-in requested invalid tile (killing)");
	  plug_in_close (plug_in, TRUE);
	  return;
	}

      if (tile_data.use_shm)
	memcpy (tile_data_pointer (tile, 0, 0),
                plug_in_shm_get_addr (plug_in->gimp),
                tile_size (tile));
      else
	memcpy (tile_data_pointer (tile, 0, 0),
                tile_info->data,
                tile_size (tile));

      tile_release (tile, TRUE);

      wire_destroy (&msg);
      if (! gp_tile_ack_write (plug_in->my_write, plug_in))
	{
	  g_warning ("plug_in_handle_tile_req: ERROR");
	  plug_in_close (plug_in, TRUE);
	  return;
	}
    }
  else
    {
      /*  this branch communicates with libgimp/gimptile.c:gimp_tile_get()  */

      if (tile_req->shadow)
	tm = gimp_drawable_shadow ((GimpDrawable *)
                                   gimp_item_get_by_ID (plug_in->gimp,
                                                        tile_req->drawable_ID));
      else
	tm = gimp_drawable_data ((GimpDrawable *)
                                 gimp_item_get_by_ID (plug_in->gimp,
                                                      tile_req->drawable_ID));

      if (! tm)
	{
	  g_warning ("plug-in requested invalid drawable (killing)");
	  plug_in_close (plug_in, TRUE);
	  return;
	}

      tile = tile_manager_get (tm, tile_req->tile_num, TRUE, FALSE);
      if (! tile)
	{
	  g_warning ("plug-in requested invalid tile (killing)");
	  plug_in_close (plug_in, TRUE);
	  return;
	}

      tile_data.drawable_ID = tile_req->drawable_ID;
      tile_data.tile_num    = tile_req->tile_num;
      tile_data.shadow      = tile_req->shadow;
      tile_data.bpp         = tile_bpp (tile);
      tile_data.width       = tile_ewidth (tile);
      tile_data.height      = tile_eheight (tile);
      tile_data.use_shm     = (shm_ID == -1) ? FALSE : TRUE;

      if (tile_data.use_shm)
	memcpy (plug_in_shm_get_addr (plug_in->gimp),
                tile_data_pointer (tile, 0, 0),
                tile_size (tile));
      else
	tile_data.data = tile_data_pointer (tile, 0, 0);

      if (! gp_tile_data_write (plug_in->my_write, &tile_data, plug_in))
	{
	  g_message ("plug_in_handle_tile_req: ERROR");
	  plug_in_close (plug_in, TRUE);
	  return;
	}

      tile_release (tile, FALSE);

      if (! wire_read_msg (plug_in->my_read, &msg, plug_in))
	{
	  g_message ("plug_in_handle_tile_req: ERROR");
	  plug_in_close (plug_in, TRUE);
	  return;
	}

      if (msg.type != GP_TILE_ACK)
	{
	  g_warning ("expected tile ack and received: %d", msg.type);
	  plug_in_close (plug_in, TRUE);
	  return;
	}

      wire_destroy (&msg);
    }
}

static void
plug_in_handle_proc_run (PlugIn    *plug_in,
                         GPProcRun *proc_run)
{
  ProcRecord *proc_rec;
  Argument   *args;
  Argument   *return_vals;

  args = plug_in_params_to_args (proc_run->params, proc_run->nparams, FALSE);
  proc_rec = procedural_db_lookup (plug_in->gimp, proc_run->name);

  plug_in_push (plug_in->gimp, plug_in);

  /*  Execute the procedure even if procedural_db_lookup() returned NULL,
   *  procedural_db_execute() will return appropriate error return_vals.
   */
  return_vals = procedural_db_execute (plug_in->gimp, proc_run->name, args);

  plug_in_pop (plug_in->gimp);

  if (return_vals)
    {
      GPProcReturn proc_return;

      proc_return.name = proc_run->name;

      if (proc_rec)
	{
	  proc_return.nparams = proc_rec->num_values + 1;
	  proc_return.params  = plug_in_args_to_params (return_vals,
                                                        proc_return.nparams,
                                                        FALSE);
	}
      else
	{
	  proc_return.nparams = 1;
	  proc_return.params  = plug_in_args_to_params (return_vals, 1, FALSE);
	}

      if (! gp_proc_return_write (plug_in->my_write, &proc_return, plug_in))
	{
	  g_warning ("plug_in_handle_proc_run: ERROR");
	  plug_in_close (plug_in, TRUE);
	  return;
	}

      plug_in_args_destroy (args, proc_run->nparams, FALSE);

      if (proc_rec)
        plug_in_args_destroy (return_vals, proc_rec->num_values + 1, TRUE);
      else
        plug_in_args_destroy (return_vals, 1, TRUE);

      plug_in_params_destroy (proc_return.params, proc_return.nparams, FALSE);
    }
  else
    {
      PlugInBlocked *blocked;

      blocked = g_new0 (PlugInBlocked, 1);

      blocked->plug_in   = plug_in;
      blocked->proc_name = g_strdup (proc_run->name);

      blocked_plug_ins = g_slist_prepend (blocked_plug_ins, blocked);
    }
}

static void
plug_in_handle_proc_return_priv (PlugIn       *plug_in,
                                 GPProcReturn *proc_return)
{
  if (plug_in->recurse)
    {
      plug_in->return_vals = plug_in_params_to_args (proc_return->params,
                                                     proc_return->nparams,
                                                     TRUE);
      plug_in->n_return_vals = proc_return->nparams;
    }
  else
    {
      GSList *list;

      for (list = blocked_plug_ins; list; list = g_slist_next (list))
	{
          PlugInBlocked *blocked;

	  blocked = (PlugInBlocked *) list->data;

	  if (blocked->proc_name && proc_return->name && 
	      strcmp (blocked->proc_name, proc_return->name) == 0)
	    {
	      if (! gp_proc_return_write (blocked->plug_in->my_write,
                                          proc_return,
                                          blocked->plug_in))
		{
		  g_message ("plug_in_handle_proc_run: ERROR");
		  plug_in_close (blocked->plug_in, TRUE);
		  return;
		}

	      blocked_plug_ins = g_slist_remove (blocked_plug_ins, blocked);
	      g_free (blocked->proc_name);
	      g_free (blocked);
	      break;
	    }
	}
    }
}

static void
plug_in_handle_proc_return (PlugIn       *plug_in,
                            GPProcReturn *proc_return)
{
  plug_in_handle_proc_return_priv (plug_in, proc_return);

  if (plug_in->recurse)
    plug_in_main_loop_quit (plug_in);

  plug_in_close (plug_in, FALSE);
}

#ifdef ENABLE_TEMP_RETURN
static void
plug_in_handle_temp_proc_return (PlugIn       *plug_in,
                                 GPProcReturn *proc_return)
{
  if (plug_in->in_temp_proc)
    {
      plug_in_handle_proc_return_priv (plug_in, proc_return);

      plug_in_main_loop_quit (plug_in);
    }
  else
    {
      g_warning ("plug_in_handle_temp_proc_return: "
                 "received a temp_proc_return mesage while not running "
                 "a temp proc (should not happen)");
      plug_in_close (plug_in, TRUE);
    }
}
#endif

static void
plug_in_handle_proc_install (PlugIn        *plug_in,
                             GPProcInstall *proc_install)
{
  PlugInDef     *plug_in_def = NULL;
  PlugInProcDef *proc_def    = NULL;
  ProcRecord    *proc        = NULL;
  GSList        *tmp         = NULL;
  gchar         *prog        = NULL;
  gboolean       valid_utf8  = FALSE;
  gint           i;

  /*  Argument checking
   *   --only sanity check arguments when the procedure requests a menu path
   */

  if (proc_install->menu_path)
    {
      if (strncmp (proc_install->menu_path, "<Toolbox>", 9) == 0)
	{
	  if ((proc_install->nparams < 1) ||
	      (proc_install->params[0].type != GIMP_PDB_INT32))
	    {
	      g_message ("Plug-In \"%s\"\n(%s)\n"
			 "attempted to install <Toolbox> procedure \"%s\"\n"
			 "which does not take the standard <Toolbox> Plug-In "
                         "args.\n"
                         "(INT32)",
			 plug_in->name,
			 plug_in->prog,
			 proc_install->name);
	      return;
	    }
	}
      else if (strncmp (proc_install->menu_path, "<Image>", 7) == 0)
	{
	  if ((proc_install->nparams < 3) ||
	      (proc_install->params[0].type != GIMP_PDB_INT32) ||
	      (proc_install->params[1].type != GIMP_PDB_IMAGE) ||
	      (proc_install->params[2].type != GIMP_PDB_DRAWABLE))
	    {
	      g_message ("Plug-In \"%s\"\n(%s)\n"
			 "attempted to install <Image> procedure \"%s\"\n"
			 "which does not take the standard <Image> Plug-In "
                         "args.\n"
                         "(INT32, IMAGE, DRAWABLE)",
			 plug_in->name,
			 plug_in->prog,
			 proc_install->name);
	      return;
	    }
	}
      else if (strncmp (proc_install->menu_path, "<Load>", 6) == 0)
	{
	  if ((proc_install->nparams < 3) ||
	      (proc_install->params[0].type != GIMP_PDB_INT32) ||
	      (proc_install->params[1].type != GIMP_PDB_STRING) ||
	      (proc_install->params[2].type != GIMP_PDB_STRING))
	    {
	      g_message ("Plug-In \"%s\"\n(%s)\n"
			 "attempted to install <Load> procedure \"%s\"\n"
			 "which does not take the standard <Load> Plug-In "
                         "args.\n"
                         "(INT32, STRING, STRING)",
			 plug_in->name,
			 plug_in->prog,
			 proc_install->name);
	      return;
	    }
	}
      else if (strncmp (proc_install->menu_path, "<Save>", 6) == 0)
	{
	  if ((proc_install->nparams < 5) ||
	      (proc_install->params[0].type != GIMP_PDB_INT32)    ||
	      (proc_install->params[1].type != GIMP_PDB_IMAGE)    ||
	      (proc_install->params[2].type != GIMP_PDB_DRAWABLE) ||
	      (proc_install->params[3].type != GIMP_PDB_STRING)   ||
	      (proc_install->params[4].type != GIMP_PDB_STRING))
	    {
	      g_message ("Plug-In \"%s\"\n(%s)\n"
			 "attempted to install <Save> procedure \"%s\"\n"
			 "which does not take the standard <Save> Plug-In "
                         "args.\n"
                         "(INT32, IMAGE, DRAWABLE, STRING, STRING)",
			 plug_in->name,
			 plug_in->prog,
			 proc_install->name);
	      return;
	    }
	}
      else
	{
	  g_message ("Plug-In \"%s\"\n(%s)\n"
		     "attempted to install procedure \"%s\"\n"
		     "in an invalid menu location.\n"
		     "Use either \"<Toolbox>\", \"<Image>\", "
		     "\"<Load>\", or \"<Save>\".",
		     plug_in->name,
		     plug_in->prog,
		     proc_install->name);
	  return;
	}
    }

  /*  Sanity check for array arguments  */

  for (i = 1; i < proc_install->nparams; i++) 
    {
      if ((proc_install->params[i].type == GIMP_PDB_INT32ARRAY ||
	   proc_install->params[i].type == GIMP_PDB_INT8ARRAY  ||
	   proc_install->params[i].type == GIMP_PDB_FLOATARRAY ||
	   proc_install->params[i].type == GIMP_PDB_STRINGARRAY)
          &&
	  proc_install->params[i-1].type != GIMP_PDB_INT32)
	{
	  g_message ("Plug-In \"%s\"\n(%s)\n"
		     "attempted to install procedure \"%s\"\n"
		     "which fails to comply with the array parameter\n"
		     "passing standard.  Argument %d is noncompliant.", 
		     plug_in->name,
		     plug_in->prog,
		     proc_install->name, i);
	  return;
	}
    }

  /*  Sanity check strings for UTF-8 validity  */

  if ((proc_install->menu_path == NULL || 
       g_utf8_validate (proc_install->menu_path, -1, NULL)) &&
      (g_utf8_validate (proc_install->name, -1, NULL))      &&
      (proc_install->blurb == NULL || 
       g_utf8_validate (proc_install->blurb, -1, NULL))     &&
      (proc_install->help == NULL || 
       g_utf8_validate (proc_install->help, -1, NULL))      &&
      (proc_install->author == NULL ||
       g_utf8_validate (proc_install->author, -1, NULL))    &&
      (proc_install->copyright == NULL || 
       g_utf8_validate (proc_install->copyright, -1, NULL)) &&
      (proc_install->date == NULL ||
       g_utf8_validate (proc_install->date, -1, NULL)))
    {
      valid_utf8 = TRUE;

      for (i = 0; i < proc_install->nparams && valid_utf8; i++)
        {
          if (! (g_utf8_validate (proc_install->params[i].name, -1, NULL) &&
                 (proc_install->params[i].description == NULL || 
                  g_utf8_validate (proc_install->params[i].description, -1, NULL))))
            valid_utf8 = FALSE;
        }

      for (i = 0; i < proc_install->nreturn_vals && valid_utf8; i++)
        {
          if (! (g_utf8_validate (proc_install->return_vals[i].name, -1, NULL) &&
                 (proc_install->return_vals[i].description == NULL ||
                  g_utf8_validate (proc_install->return_vals[i].description, -1, NULL))))
            valid_utf8 = FALSE;
        }
    }
  
  if (! valid_utf8)
    {
      g_message ("Plug-In \"%s\"\n(%s)\n"
                 "attempted to install a procedure with invalid UTF-8 strings.", 
                 plug_in->name,
                 plug_in->prog);
      return;
    }

  /*  Initialization  */

  switch (proc_install->type)
    {
    case GIMP_PLUGIN:
    case GIMP_EXTENSION:
      plug_in_def = plug_in->plug_in_def;
      prog        = plug_in_def->prog;

      tmp = plug_in_def->proc_defs;
      break;

    case GIMP_TEMPORARY:
      plug_in_def = NULL;
      prog        = "none";

      tmp = plug_in->temp_proc_defs;
      break;
    }

  while (tmp)
    {
      proc_def = tmp->data;
      tmp = tmp->next;

      if (strcmp (proc_def->db_info.name, proc_install->name) == 0)
	{
          switch (proc_install->type)
            {
            case GIMP_PLUGIN:
            case GIMP_EXTENSION:
              plug_in_def->proc_defs = g_slist_remove (plug_in_def->proc_defs,
                                                       proc_def);
              plug_in_proc_def_free (proc_def);
              break;

            case GIMP_TEMPORARY:
              plug_in->temp_proc_defs = g_slist_remove (plug_in->temp_proc_defs,
                                                        proc_def);
              plug_ins_temp_proc_def_remove (plug_in->gimp, proc_def);
              break;
            }

	  break;
	}
    }

  proc_def = plug_in_proc_def_new ();

  proc_def->prog            = g_strdup (prog);
  proc_def->menu_path       = g_strdup (proc_install->menu_path);
  proc_def->accelerator     = NULL;
  proc_def->extensions      = NULL;
  proc_def->prefixes        = NULL;
  proc_def->magics          = NULL;
  proc_def->image_types     = g_strdup (proc_install->image_types);
  proc_def->image_types_val = plug_ins_image_types_parse (proc_def->image_types);
  /* Install temp one use todays time */
  proc_def->mtime           = time (NULL);

  /*  The procedural database procedure  */

  proc = &proc_def->db_info;

  proc->name      = g_strdup (proc_install->name);
  proc->blurb     = g_strdup (proc_install->blurb);
  proc->help      = g_strdup (proc_install->help);
  proc->author    = g_strdup (proc_install->author);
  proc->copyright = g_strdup (proc_install->copyright);
  proc->date      = g_strdup (proc_install->date);
  proc->proc_type = proc_install->type;

  proc->num_args   = proc_install->nparams;
  proc->num_values = proc_install->nreturn_vals;

  proc->args   = g_new0 (ProcArg, proc->num_args);
  proc->values = g_new0 (ProcArg, proc->num_values);

  for (i = 0; i < proc->num_args; i++)
    {
      proc->args[i].arg_type    = proc_install->params[i].type;
      proc->args[i].name        = g_strdup (proc_install->params[i].name);
      proc->args[i].description = g_strdup (proc_install->params[i].description);
    }

  for (i = 0; i < proc->num_values; i++)
    {
      proc->values[i].arg_type    = proc_install->return_vals[i].type;
      proc->values[i].name        = g_strdup (proc_install->return_vals[i].name);
      proc->values[i].description = g_strdup (proc_install->return_vals[i].description);
    }

  switch (proc_install->type)
    {
    case GIMP_PLUGIN:
    case GIMP_EXTENSION:
      plug_in_def->proc_defs = g_slist_prepend (plug_in_def->proc_defs,
                                                proc_def);
      break;

    case GIMP_TEMPORARY:
      plug_in->temp_proc_defs = g_slist_prepend (plug_in->temp_proc_defs,
                                                 proc_def);

      proc->exec_method.temporary.plug_in = plug_in;

      plug_ins_temp_proc_def_add (plug_in->gimp, proc_def,
                                  plug_ins_locale_domain (plug_in->gimp,
                                                          plug_in->prog,
                                                          NULL),
                                  plug_ins_help_path (plug_in->gimp,
                                                      plug_in->prog));
      break;
    }
}

static void
plug_in_handle_proc_uninstall (PlugIn          *plug_in,
                               GPProcUninstall *proc_uninstall)
{
  GSList *tmp;

  for (tmp = plug_in->temp_proc_defs; tmp; tmp = g_slist_next (tmp))
    {
      PlugInProcDef *proc_def;

      proc_def = tmp->data;

      if (! strcmp (proc_def->db_info.name, proc_uninstall->name))
	{
	  plug_in->temp_proc_defs = g_slist_remove (plug_in->temp_proc_defs,
                                                    proc_def);
	  plug_ins_temp_proc_def_remove (plug_in->gimp, proc_def);
	  break;
	}
    }
}

static void
plug_in_handle_extension_ack (PlugIn *plug_in)
{
  if (plug_in->starting_ext)
    {
      plug_in_main_loop_quit (plug_in);
    }
  else
    {
      g_warning ("plug_in_handle_extension_ack: "
		 "received an extension_ack message while not starting "
                 "an extension (should not happen)");
      plug_in_close (plug_in, TRUE);
    }
}

static void
plug_in_handle_has_init (PlugIn *plug_in)
{
  if (plug_in->query)
    {
      plug_in_def_set_has_init (plug_in->plug_in_def, TRUE);
    }
  else
    {
      g_warning ("plug_in_handle_has_init: "
		 "received a has_init message while not in query() "
                 "(should not happen)");
      plug_in_close (plug_in, TRUE);
    }
}
