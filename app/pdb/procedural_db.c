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

#include <stdarg.h>
#include <string.h>
#include <sys/types.h>

#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"

#include "pdb-types.h"

#include "core/gimp.h"

#include "plug-in/plug-in.h"

#include "procedural_db.h"

#include "libgimp/gimpintl.h"


typedef struct _PDBData PDBData;

struct _PDBData
{
  gchar  *identifier;
  gint32  bytes;
  guint8 *data;
};


void
procedural_db_init (Gimp *gimp)
{
  g_return_if_fail (GIMP_IS_GIMP (gimp));

  gimp->procedural_ht           = g_hash_table_new (g_str_hash, g_str_equal);
  gimp->procedural_db_data_list = NULL;
}

static void
procedural_db_free_entry (gpointer key,
			  gpointer value,
			  gpointer user_data)
{
  if (value)
    g_list_free (value);
}

void
procedural_db_free (Gimp *gimp)
{
  g_return_if_fail (GIMP_IS_GIMP (gimp));

  if (gimp->procedural_ht)
    {
      g_hash_table_foreach (gimp->procedural_ht, 
                            procedural_db_free_entry, NULL);
      g_hash_table_destroy (gimp->procedural_ht);
  
      gimp->procedural_ht = NULL;
    }

  if (gimp->procedural_db_data_list)
    {
      PDBData *data;
      GList   *list;

      for (list = gimp->procedural_db_data_list; list; list = g_list_next (list))
        {
          data = (PDBData *) list->data;

          g_free (data->identifier);
          g_free (data->data);
          g_free (data);
        }

      g_list_free (gimp->procedural_db_data_list);
      gimp->procedural_db_data_list = NULL;
    }
}

void
procedural_db_register (Gimp       *gimp,
			ProcRecord *procedure)
{
  GList *list;

  g_return_if_fail (GIMP_IS_GIMP (gimp));
  g_return_if_fail (procedure != NULL);

  list = g_hash_table_lookup (gimp->procedural_ht, (gpointer) procedure->name);
  list = g_list_prepend (list, (gpointer) procedure);

  g_hash_table_insert (gimp->procedural_ht,
		       (gpointer) procedure->name,
		       (gpointer) list);
}

void
procedural_db_unregister (Gimp        *gimp,
			  const gchar *name)
{
  GList *list;

  g_return_if_fail (GIMP_IS_GIMP (gimp));
  g_return_if_fail (name != NULL);

  list = g_hash_table_lookup (gimp->procedural_ht, (gpointer) name);

  if (list)
    {
      list = g_list_remove (list, list->data);

      if (list)
	g_hash_table_insert (gimp->procedural_ht,
			     (gpointer) name,
			     (gpointer) list);
      else 
	g_hash_table_remove (gimp->procedural_ht,
			     (gpointer) name);
    }
}

ProcRecord *
procedural_db_lookup (Gimp        *gimp,
		      const gchar *name)
{
  GList *list;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  list = g_hash_table_lookup (gimp->procedural_ht, (gpointer) name);

  if (list)
    return (ProcRecord *) list->data;
  else
    return NULL;
}

Argument *
procedural_db_execute (Gimp        *gimp,
		       const gchar *name,
		       Argument    *args)
{
  ProcRecord *procedure;
  Argument   *return_args;
  GList      *list;
  gint        i;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  return_args = NULL;

  list = g_hash_table_lookup (gimp->procedural_ht, (gpointer) name);

  if (list == NULL)
    {
      g_message (_("PDB calling error %s not found"), name);
      
      return_args = g_new (Argument, 1);
      return_args->arg_type      = GIMP_PDB_STATUS;
      return_args->value.pdb_int = GIMP_PDB_CALLING_ERROR;

      return return_args;
    }
  
  while (list)
    {
      if ((procedure = (ProcRecord *) list->data) == NULL)
	{
	  g_message (_("PDB calling error %s not found"), name);

	  return_args = g_new (Argument, 1);
	  return_args->arg_type      = GIMP_PDB_STATUS;
	  return_args->value.pdb_int = GIMP_PDB_CALLING_ERROR;

	  return return_args;
	}
      list = list->next;

      /*  check the arguments  */
      for (i = 0; i < procedure->num_args; i++)
	{
	  if (args[i].arg_type != procedure->args[i].arg_type)
	    {
	      return_args = g_new (Argument, 1);
	      return_args->arg_type      = GIMP_PDB_STATUS;
	      return_args->value.pdb_int = GIMP_PDB_CALLING_ERROR;

	      g_message (_("PDB calling error %s"), procedure->name);

	      return return_args;
	    }
	}

      /*  call the procedure  */
      switch (procedure->proc_type)
	{
	case GIMP_INTERNAL:
	  return_args = 
            (* procedure->exec_method.internal.marshal_func) (gimp, args);
	  break;

	case GIMP_PLUGIN:
	case GIMP_EXTENSION:
	case GIMP_TEMPORARY:
	  return_args = plug_in_run (gimp,
                                     procedure, 
                                     args, procedure->num_args, 
                                     TRUE, FALSE, -1);
	  break;

	default:
	  return_args = g_new (Argument, 1);
	  return_args->arg_type      = GIMP_PDB_STATUS;
	  return_args->value.pdb_int = GIMP_PDB_EXECUTION_ERROR;
	  break;
	}

      if ((return_args[0].value.pdb_int != GIMP_PDB_SUCCESS) &&
	  (procedure->num_values > 0))
	memset (&return_args[1], 0, sizeof (Argument) * procedure->num_values);

      /*  Check if the return value is a PDB_PASS_THROUGH, 
          in which case run the next procedure in the list  */

      if (return_args[0].value.pdb_int != GIMP_PDB_PASS_THROUGH)
	break;
      else if (list)  /*  Pass through, 
                          destroy return values and run another procedure  */
	procedural_db_destroy_args (return_args, procedure->num_values);
    }

  return return_args;
}

Argument *
procedural_db_run_proc (Gimp        *gimp,
			const gchar *name,
			gint        *nreturn_vals,
			...)
{
  ProcRecord *proc;
  Argument   *params;
  Argument   *return_vals;
  va_list     args;
  gint        i;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  if ((proc = procedural_db_lookup (gimp, name)) == NULL)
    {
      return_vals = g_new (Argument, 1);
      return_vals->arg_type      = GIMP_PDB_STATUS;
      return_vals->value.pdb_int = GIMP_PDB_CALLING_ERROR;

      return return_vals;
    }

  /*  allocate the parameter array  */
  params = g_new (Argument, proc->num_args);

  va_start (args, nreturn_vals);

  for (i = 0; i < proc->num_args; i++)
    {
      if (proc->args[i].arg_type != 
          (params[i].arg_type = va_arg (args, GimpPDBArgType)))
	{
	  g_message (_("Incorrect arguments passed to procedural_db_run_proc:\n"
		       "Argument %d to '%s' should be a %s, but got passed a %s"),
		     i+1, proc->name,
		     pdb_type_name (proc->args[i].arg_type),
		     pdb_type_name (params[i].arg_type));
	  g_free (params);
	  return NULL;
	}

      switch (proc->args[i].arg_type)
	{
	case GIMP_PDB_INT32:
	case GIMP_PDB_INT16:
	case GIMP_PDB_INT8:
        case GIMP_PDB_DISPLAY:
	  params[i].value.pdb_int = (gint32) va_arg (args, gint);
	  break;

        case GIMP_PDB_FLOAT:
          params[i].value.pdb_float = (gdouble) va_arg (args, gdouble);
          break;

        case GIMP_PDB_STRING:
        case GIMP_PDB_INT32ARRAY:
        case GIMP_PDB_INT16ARRAY:
        case GIMP_PDB_INT8ARRAY:
        case GIMP_PDB_FLOATARRAY:
        case GIMP_PDB_STRINGARRAY:
          params[i].value.pdb_pointer = va_arg (args, gpointer);
          break;

        case GIMP_PDB_COLOR:
	  params[i].value.pdb_color = va_arg (args, GimpRGB);
          break;

        case GIMP_PDB_REGION:
          break;

        case GIMP_PDB_IMAGE:
        case GIMP_PDB_LAYER:
        case GIMP_PDB_CHANNEL:
        case GIMP_PDB_DRAWABLE:
        case GIMP_PDB_SELECTION:
        case GIMP_PDB_BOUNDARY:
        case GIMP_PDB_PATH:
	  params[i].value.pdb_int = (gint32) va_arg (args, gint);
	  break;

        case GIMP_PDB_PARASITE:
          params[i].value.pdb_pointer = va_arg (args, gpointer);
          break;

        case GIMP_PDB_STATUS:
	  params[i].value.pdb_int = (gint32) va_arg (args, gint);
	  break;

	case GIMP_PDB_END:
	  break;
	}
    }

  va_end (args);

  *nreturn_vals = proc->num_values;

  return_vals = procedural_db_execute (gimp, name, params);

  g_free (params);

  return return_vals;
}

Argument *
procedural_db_return_args (ProcRecord *procedure,
			   gboolean    success)
{
  Argument *return_args;
  gint      i;

  g_return_val_if_fail (procedure != NULL, NULL);

  return_args = g_new (Argument, procedure->num_values + 1);

  if (success)
    {
      return_args[0].arg_type      = GIMP_PDB_STATUS;
      return_args[0].value.pdb_int = GIMP_PDB_SUCCESS;
    }
  else
    {
      return_args[0].arg_type      = GIMP_PDB_STATUS;
      return_args[0].value.pdb_int = GIMP_PDB_EXECUTION_ERROR;
    }

  /*  Set the arg types for the return values  */
  for (i = 0; i < procedure->num_values; i++)
    return_args[i+1].arg_type = procedure->values[i].arg_type;

  return return_args;
}

void
procedural_db_destroy_args (Argument *args,
			    gint      nargs)
{
  gint i;

  if (! args)
    return;

  for (i = 0; i < nargs; i++)
    {
      switch (args[i].arg_type)
	{
	case GIMP_PDB_INT32:
	case GIMP_PDB_INT16:
	case GIMP_PDB_INT8:
	case GIMP_PDB_FLOAT:
	  break;

	case GIMP_PDB_STRING:
	case GIMP_PDB_INT32ARRAY:
	case GIMP_PDB_INT16ARRAY:
	case GIMP_PDB_INT8ARRAY:
	case GIMP_PDB_FLOATARRAY:
	  g_free (args[i].value.pdb_pointer);
	  break;

	case GIMP_PDB_STRINGARRAY:
          {
            gchar **stringarray;
            gint    count;
            gint    j;

            count       = args[i - 1].value.pdb_int;
            stringarray = args[i].value.pdb_pointer;

            for (j = 0; j < count; j++)
              g_free (stringarray[j]);

            g_free (args[i].value.pdb_pointer);
          }
	  break;

	case GIMP_PDB_COLOR:
	case GIMP_PDB_REGION:
	case GIMP_PDB_DISPLAY:
	case GIMP_PDB_IMAGE:
	case GIMP_PDB_LAYER:
	case GIMP_PDB_CHANNEL:
	case GIMP_PDB_DRAWABLE:
	case GIMP_PDB_SELECTION:
	case GIMP_PDB_BOUNDARY:
	case GIMP_PDB_PATH:
          break;

	case GIMP_PDB_PARASITE:
          gimp_parasite_free ((GimpParasite *) (args[i].value.pdb_pointer));
          break;

	case GIMP_PDB_STATUS:
	case GIMP_PDB_END:
	  break;
	}
    }

  g_free (args);
}

void
procedural_db_set_data (Gimp        *gimp,
                        const gchar  *identifier,
                        gint32        bytes,
                        const guint8 *data)
{
  GList   *list;
  PDBData *pdb_data;

  g_return_if_fail (GIMP_IS_GIMP (gimp));
  g_return_if_fail (identifier != NULL);
  g_return_if_fail (bytes > 0);
  g_return_if_fail (data != NULL);

  for (list = gimp->procedural_db_data_list; list; list = g_list_next (list))
    {
      pdb_data = (PDBData *) list->data;

      if (! strcmp (pdb_data->identifier, identifier))
        break;
    }

  /* If there isn't already data with the specified identifier, create one */
  if (list == NULL)
    {
      pdb_data = g_new0 (PDBData, 1);
      pdb_data->identifier = g_strdup (identifier);

      gimp->procedural_db_data_list =
        g_list_prepend (gimp->procedural_db_data_list, pdb_data);
    }
  else
    {
      g_free (pdb_data->data);
    }

  pdb_data->bytes = bytes;
  pdb_data->data  = g_memdup (data, bytes);
}

const guint8 *
procedural_db_get_data (Gimp        *gimp,
                        const gchar *identifier,
                        gint32      *bytes)
{
  GList   *list;
  PDBData *pdb_data;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);
  g_return_val_if_fail (identifier != NULL, NULL);
  g_return_val_if_fail (bytes != NULL, NULL);

  *bytes = 0;

  for (list = gimp->procedural_db_data_list; list; list = g_list_next (list))
    {
      pdb_data = (PDBData *) list->data;

      if (! strcmp (pdb_data->identifier, identifier))
        {
          *bytes = pdb_data->bytes;
          return pdb_data->data;
        }
    }

  return NULL;
}
