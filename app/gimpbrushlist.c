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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

#include <gtk/gtk.h>

#include "libgimpmath/gimpmath.h"

#include "apptypes.h"

#include "appenv.h"
#include "gimpbrushgenerated.h"
#include "gimpbrushpipe.h"
#include "brush_header.h"
#include "brush_select.h"
#include "datafiles.h"
#include "gimpcontext.h"
#include "gimprc.h"
#include "gimplist.h"
#include "gimpbrush.h"
#include "gimpbrushlist.h"

#include "libgimp/gimpenv.h"

#include "libgimp/gimpintl.h"


/*  global variables  */
GimpBrushList *brush_list = NULL;


/*  local function prototypes  */
static void   brushes_brush_load (const gchar   *filename);

static gint   brush_compare_func (gconstpointer  first,
				  gconstpointer  second);


/*  class functions  */
static GimpListClass* parent_class = NULL;


static void
gimp_brush_list_add_func (GimpList *list,
			  gpointer  val)
{
  list->list = g_slist_insert_sorted (list->list, val, brush_compare_func);
  GIMP_BRUSH_LIST (list)->num_brushes++;
}

static void
gimp_brush_list_remove_func (GimpList *list,
			     gpointer  val)
{
  list->list = g_slist_remove (list->list, val);

  GIMP_BRUSH_LIST (list)->num_brushes--;
}

static void
gimp_brush_list_class_init (GimpBrushListClass *klass)
{
  GimpListClass *gimp_list_class;
  
  gimp_list_class = (GimpListClass *) klass;

  parent_class = gtk_type_class (GIMP_TYPE_LIST);

  gimp_list_class->add    = gimp_brush_list_add_func;
  gimp_list_class->remove = gimp_brush_list_remove_func;
}

void
gimp_brush_list_init (GimpBrushList *list)
{
  list->num_brushes = 0;
}

GtkType
gimp_brush_list_get_type (void)
{
  static GtkType type = 0;

  if (!type)
    {
      GtkTypeInfo info =
      {
	"GimpBrushList",
	sizeof (GimpBrushList),
	sizeof (GimpBrushListClass),
	(GtkClassInitFunc) gimp_brush_list_class_init,
	(GtkObjectInitFunc) gimp_brush_list_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL
      };

      type = gtk_type_unique (GIMP_TYPE_LIST, &info);
    }

  return type;
}

GimpBrushList *
gimp_brush_list_new (void)
{
  GimpBrushList *list;

  list = GIMP_BRUSH_LIST (gtk_type_new (GIMP_TYPE_BRUSH_LIST));

  GIMP_LIST (list)->type = GIMP_TYPE_BRUSH;
  GIMP_LIST (list)->weak = FALSE;

  return list;
}

/*  function declarations  */
void
brushes_init (gboolean no_data)
{
  if (brush_list)
    brushes_free ();
  else
    brush_list = gimp_brush_list_new ();

  if (brush_path != NULL && !no_data)
    {
      brush_select_freeze_all ();

      datafiles_read_directories (brush_path, brushes_brush_load, 0);
      datafiles_read_directories (brush_vbr_path, brushes_brush_load, 0);

      brush_select_thaw_all ();
    }

  gimp_context_refresh_brushes ();
}

GimpBrush *
brushes_get_standard_brush (void)
{
  static GimpBrush *standard_brush = NULL;

  if (! standard_brush)
    {
      standard_brush =
	GIMP_BRUSH (gimp_brush_generated_new (5.0, 0.5, 0.0, 1.0));

      gimp_object_set_name (GIMP_OBJECT (standard_brush), "Standard");

      /*  set ref_count to 2 --> never swap the standard brush  */
      gtk_object_ref (GTK_OBJECT (standard_brush));
      gtk_object_ref (GTK_OBJECT (standard_brush));
      gtk_object_sink (GTK_OBJECT (standard_brush));
    }

  return standard_brush;
}

static void
brushes_brush_load (const gchar *filename)
{
  GimpBrush *brush;

  if (strcmp (&filename[strlen (filename) - 4], ".gbr") == 0 ||
      strcmp (&filename[strlen (filename) - 4], ".gpb") == 0)
    {
      brush = gimp_brush_load (filename);

      if (brush != NULL)
	gimp_brush_list_add (brush_list, brush);
      else
	g_message (_("Warning: Failed to load brush\n\"%s\""), filename);
    }
  else if (strcmp (&filename[strlen(filename) - 4], ".vbr") == 0)
    {
      brush = gimp_brush_generated_load (filename);

      if (brush != NULL)
	gimp_brush_list_add (brush_list, GIMP_BRUSH (brush));
      else
	g_message (_("Warning: Failed to load brush\n\"%s\""), filename);
    }
  else if (strcmp (&filename[strlen (filename) - 4], ".gih") == 0)
    {
      brush = gimp_brush_pipe_load (filename);

      if (brush != NULL)
	gimp_brush_list_add (brush_list, GIMP_BRUSH (brush));
      else
	g_message (_("Warning: Failed to load brush pipe\n\"%s\""), filename);
    }
}

static gint
brush_compare_func (gconstpointer first,
		    gconstpointer second)
{
  return strcmp (((const GimpObject *) first)->name, 
		 ((const GimpObject *) second)->name);
}

void
brushes_free (void)
{
  GList *vbr_path;
  gchar *vbr_dir;

  if (!brush_list)
    return;

  vbr_path = gimp_path_parse (brush_vbr_path, 16, TRUE, NULL);
  vbr_dir  = gimp_path_get_user_writable_dir (vbr_path);
  gimp_path_free (vbr_path);

  brush_select_freeze_all ();

  while (GIMP_LIST (brush_list)->list)
    {
      GimpBrush *brush = GIMP_BRUSH (GIMP_LIST (brush_list)->list->data);

      if (GIMP_IS_BRUSH_GENERATED (brush) && vbr_dir)
	{
	  gchar *filename = NULL;
	  if (!brush->filename)
	    {
	      FILE *tmp_fp;
	      gint  unum = 0;

	      if (vbr_dir)
	        {
		  gchar *safe_name;
		  gint   i;

		  /* make sure we don't create a naughty filename */
		  safe_name = g_strdup (GIMP_OBJECT (brush)->name);
		  if (safe_name[0] == '.')
		    safe_name[0] = '_';
		  for (i = 0; safe_name[i]; i++)
		    if (safe_name[i] == G_DIR_SEPARATOR || isspace(safe_name[i]))
		      safe_name[i] = '_';

		  filename = g_strdup_printf ("%s%s.vbr",
					      vbr_dir,
					      safe_name);
		  while ((tmp_fp = fopen (filename, "r")))
		    { /* make sure we don't overite an existing brush */
		      fclose (tmp_fp);
		      g_free (filename);
		      filename = g_strdup_printf ("%s%s_%d.vbr", 
						  vbr_dir, safe_name, unum);
		      unum++;
		    }
		  g_free (safe_name);
		}
	    }
	  else
	    {
	      filename = g_strdup (brush->filename);
	      if (strlen(filename) < 4 || strcmp (&filename[strlen (filename) - 4],
						  ".vbr"))
	        { /* we only want to save .vbr files, so set filename to null
		     if this isn't a .vbr file */
		  g_free (filename);
		  filename = NULL;
		}
	    }
	  /*  we are (finaly) ready to try to save the generated brush file  */
	  if (filename)
	    {
	      gimp_brush_generated_save (GIMP_BRUSH_GENERATED (brush),
					 filename);
	      g_free (filename);
	    }
	}

      gimp_brush_list_remove (brush_list, brush);
    }

  brush_select_thaw_all ();

  g_free (vbr_dir);
}

gint
gimp_brush_list_get_brush_index (GimpBrushList *brush_list,
				 GimpBrush     *brush)
{
  /* FIXME: make a gimp_list function that does this? */
  return g_slist_index (GIMP_LIST (brush_list)->list, brush);
}

GimpBrush *
gimp_brush_list_get_brush_by_index (GimpBrushList *brush_list,
				    gint           index)
{
  GimpBrush *brush = NULL;
  GSList    *list;

  /* FIXME: make a gimp_list function that does this? */
  list = g_slist_nth (GIMP_LIST (brush_list)->list, index);
  if (list)
    brush = (GimpBrush *) list->data;

  return brush;
}

static void
gimp_brush_list_uniquefy_brush_name (GimpBrushList *brush_list,
				     GimpBrush     *brush)
{
  GSList    *list;
  GSList    *list2;
  GSList    *base_list;
  GimpBrush *brush2;
  gint       unique_ext = 0;
  gchar     *new_name   = NULL;
  gchar     *ext;

  g_return_if_fail (GIMP_IS_BRUSH_LIST (brush_list));
  g_return_if_fail (GIMP_IS_BRUSH (brush));

  base_list = GIMP_LIST (brush_list)->list;

  for (list = base_list; list; list = g_slist_next (list))
    {
      brush2 = GIMP_BRUSH (list->data);

      if (brush != brush2 &&
	  strcmp (gimp_object_get_name (GIMP_OBJECT (brush)),
		  gimp_object_get_name (GIMP_OBJECT (brush2))) == 0)
	{
          ext = strrchr (GIMP_OBJECT (brush)->name, '#');

          if (ext)
            {
              gchar *ext_str;

              unique_ext = atoi (ext + 1);

              ext_str = g_strdup_printf ("%d", unique_ext);

              /*  check if the extension really is of the form "#<n>"  */
              if (! strcmp (ext_str, ext + 1))
                {
                  *ext = '\0';
                }
              else
                {
                  unique_ext = 0;
                }

              g_free (ext_str);
            }
          else
            {
              unique_ext = 0;
            }

          do
            {
              unique_ext++;

              g_free (new_name);

              new_name = g_strdup_printf ("%s#%d",
                                          GIMP_OBJECT (brush)->name,
                                          unique_ext);

              for (list2 = base_list; list2; list2 = g_slist_next (list2))
                {
                  brush2 = GIMP_BRUSH (list2->data);

                  if (brush == brush2)
                    continue;

                  if (! strcmp (GIMP_OBJECT (brush2)->name, new_name))
                    {
                      break;
                    }
                }
            }
          while (list2);

	  gimp_object_set_name (GIMP_OBJECT (brush), new_name);
	  g_free (new_name);

	  if (gimp_list_have (GIMP_LIST (brush_list), brush))
	    {
	      gtk_object_ref (GTK_OBJECT (brush));
	      gimp_brush_list_remove (brush_list, brush);
	      gimp_brush_list_add (brush_list, brush);
	      gtk_object_unref (GTK_OBJECT (brush));
	    }

	  break;
	}
    }
}

static void
brush_renamed (GimpBrush     *brush,
	       GimpBrushList *brush_list)
{
  gimp_brush_list_uniquefy_brush_name (brush_list, brush);
}

void
gimp_brush_list_add (GimpBrushList *brush_list,
		     GimpBrush     *brush)
{
  gimp_brush_list_uniquefy_brush_name (brush_list, brush);
  gimp_list_add (GIMP_LIST (brush_list), brush);
  gtk_signal_connect (GTK_OBJECT (brush), "name_changed",
		      GTK_SIGNAL_FUNC (brush_renamed),
		      brush_list);
}

void
gimp_brush_list_remove (GimpBrushList *brush_list,
			GimpBrush     *brush)
{
  gtk_signal_disconnect_by_data (GTK_OBJECT (brush), brush_list);

  gimp_list_remove (GIMP_LIST (brush_list), brush);
}

gint
gimp_brush_list_length (GimpBrushList *brush_list)
{
  g_return_val_if_fail (GIMP_IS_BRUSH_LIST (brush_list), 0);

  return brush_list->num_brushes;
}

GimpBrush *
gimp_brush_list_get_brush (GimpBrushList *blist,
			   const gchar   *name)
{
  GimpObject *object;
  GSList     *list;

  if (blist == NULL || name == NULL)
    return NULL;

  for (list = GIMP_LIST (blist)->list; list; list = g_slist_next (list))
    {
      object = (GimpObject *) list->data;

      if (!strcmp (object->name, name))
	return GIMP_BRUSH (object);
    }

  return NULL;
}

