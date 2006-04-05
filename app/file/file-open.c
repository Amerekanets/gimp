/* The GIMP -- an image manipulation program
 * Copyright (C) 1995, 1996, 1997 Spencer Kimball and Peter Mattis
 * Copyright (C) 1997 Josh MacDonald
 *
 * file-open.c
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib-object.h>
#include <glib/gstdio.h>

#ifdef G_OS_WIN32
#include <io.h>
#define R_OK 4
#endif

#include "core/core-types.h"

#include "config/gimpcoreconfig.h"

#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpdocumentlist.h"
#include "core/gimpimage.h"
#include "core/gimpimage-merge.h"
#include "core/gimpimage-undo.h"
#include "core/gimpimagefile.h"
#include "core/gimplayer.h"
#include "core/gimpparamspecs.h"
#include "core/gimpprogress.h"

#include "pdb/gimp-pdb.h"
#include "pdb/gimpprocedure.h"

#include "plug-in/plug-in.h"
#include "plug-in/plug-in-proc-def.h"

#include "file-open.h"
#include "file-utils.h"
#include "gimprecentlist.h"

#include "gimp-intl.h"


static void  file_open_sanitize_image (GimpImage *image);


/*  public functions  */

GimpImage *
file_open_image (Gimp                *gimp,
                 GimpContext         *context,
                 GimpProgress        *progress,
		 const gchar         *uri,
		 const gchar         *entered_filename,
		 GimpPlugInProcedure *file_proc,
		 GimpRunMode          run_mode,
		 GimpPDBStatusType   *status,
                 const gchar        **mime_type,
                 GError             **error)
{
  GValueArray *return_vals;
  gchar       *filename;
  GimpImage   *image = NULL;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);
  g_return_val_if_fail (GIMP_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (progress == NULL || GIMP_IS_PROGRESS (progress), NULL);
  g_return_val_if_fail (status != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  *status = GIMP_PDB_EXECUTION_ERROR;

  if (! file_proc)
    file_proc = file_utils_find_proc (gimp->load_procs, uri);

  if (! file_proc)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   _("Unknown file type"));
      return NULL;
    }

  filename = file_utils_filename_from_uri (uri);

  if (filename)
    {
      /* check if we are opening a file */
      if (g_file_test (filename, G_FILE_TEST_EXISTS))
        {
          if (! g_file_test (filename, G_FILE_TEST_IS_REGULAR))
            {
              g_free (filename);
              g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                           _("Not a regular file"));
              return NULL;
            }

          if (g_access (filename, R_OK) != 0)
            {
              g_free (filename);
              g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_ACCES,
                           g_strerror (errno));
              return NULL;
            }
        }
    }
  else
    {
      filename = g_strdup (uri);
    }

  return_vals = gimp_pdb_run_proc (gimp, context, progress,
                                   GIMP_PROCEDURE (file_proc)->name,
                                   GIMP_TYPE_INT32, run_mode,
                                   G_TYPE_STRING,   filename,
                                   G_TYPE_STRING,   entered_filename,
                                   G_TYPE_NONE);

  g_free (filename);

  *status = g_value_get_enum (&return_vals->values[0]);

  if (*status == GIMP_PDB_SUCCESS)
    {
      image = gimp_value_get_image (&return_vals->values[1], gimp);

      if (image)
        {
          file_open_sanitize_image (image);

          if (mime_type)
            *mime_type = file_proc->mime_type;
        }
      else
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                       _("Plug-In returned SUCCESS but did not "
                         "return an image"));
          *status = GIMP_PDB_EXECUTION_ERROR;
        }
    }
  else if (*status != GIMP_PDB_CANCEL)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   _("Plug-In could not open image"));
    }

  g_value_array_free (return_vals);

  return image;
}

/*  Attempts to load a thumbnail by using a registered thumbnail loader.  */
GimpImage *
file_open_thumbnail (Gimp          *gimp,
                     GimpContext   *context,
                     GimpProgress  *progress,
                     const gchar   *uri,
                     gint           size,
                     const gchar  **mime_type,
                     gint          *image_width,
                     gint          *image_height)
{
  GimpPlugInProcedure *file_proc;
  GimpProcedure       *procedure;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);
  g_return_val_if_fail (GIMP_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (progress == NULL || GIMP_IS_PROGRESS (progress), NULL);
  g_return_val_if_fail (mime_type != NULL, NULL);
  g_return_val_if_fail (image_width != NULL, NULL);
  g_return_val_if_fail (image_height != NULL, NULL);

  *image_width  = 0;
  *image_height = 0;

  file_proc = file_utils_find_proc (gimp->load_procs, uri);

  if (! file_proc || ! file_proc->thumb_loader)
    return NULL;

  procedure = gimp_pdb_lookup (gimp, file_proc->thumb_loader);

  if (procedure && procedure->num_args >= 2 && procedure->num_values >= 1)
    {
      GimpPDBStatusType  status;
      GValueArray       *return_vals;
      gchar             *filename;
      GimpImage         *image = NULL;

      filename = file_utils_filename_from_uri (uri);

      if (! filename)
        filename = g_strdup (uri);

      return_vals = gimp_pdb_run_proc (gimp, context, progress,
                                       procedure->name,
                                       G_TYPE_STRING,   filename,
                                       GIMP_TYPE_INT32, size,
                                       G_TYPE_NONE);

      g_free (filename);

      status = g_value_get_enum (&return_vals->values[0]);

      if (status == GIMP_PDB_SUCCESS)
        {
          image = gimp_value_get_image (&return_vals->values[1], gimp);

          if (return_vals->n_values >= 3)
            {
              *image_width  = MAX (0, g_value_get_int (&return_vals->values[2]));
              *image_height = MAX (0, g_value_get_int (&return_vals->values[3]));
            }

          if (image)
            {
              file_open_sanitize_image (image);

              *mime_type = file_proc->mime_type;

              g_printerr ("opened thumbnail at %d x %d\n",
                          image->width, image->height);
            }
        }

      g_value_array_free (return_vals);

      return image;
    }

  return NULL;
}

GimpImage *
file_open_with_display (Gimp               *gimp,
                        GimpContext        *context,
                        GimpProgress       *progress,
                        const gchar        *uri,
                        GimpPDBStatusType  *status,
                        GError            **error)
{
  return file_open_with_proc_and_display (gimp, context, progress,
                                          uri, uri, NULL, status, error);
}

GimpImage *
file_open_with_proc_and_display (Gimp                *gimp,
                                 GimpContext         *context,
                                 GimpProgress        *progress,
                                 const gchar         *uri,
                                 const gchar         *entered_filename,
                                 GimpPlugInProcedure *file_proc,
                                 GimpPDBStatusType   *status,
                                 GError             **error)
{
  GimpImage   *image;
  const gchar *mime_type = NULL;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);
  g_return_val_if_fail (GIMP_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (progress == NULL || GIMP_IS_PROGRESS (progress), NULL);
  g_return_val_if_fail (status != NULL, NULL);

  image = file_open_image (gimp, context, progress,
                           uri,
                           entered_filename,
                           file_proc,
                           GIMP_RUN_INTERACTIVE,
                           status,
                           &mime_type,
                           error);

  if (image)
    {
      GimpDocumentList *documents = GIMP_DOCUMENT_LIST (gimp->documents);
      GimpImagefile    *imagefile;

      gimp_create_display (image->gimp, image, GIMP_UNIT_PIXEL, 1.0);

      imagefile = gimp_document_list_add_uri (documents, uri, mime_type);

      /*  can only create a thumbnail if the passed uri and the
       *  resulting image's uri match.
       */
      if (strcmp (uri, gimp_image_get_uri (image)) == 0)
        {
          /*  no need to save a thumbnail if there's a good one already  */
          if (! gimp_imagefile_check_thumbnail (imagefile))
            {
              gimp_imagefile_save_thumbnail (imagefile, mime_type, image);
            }
        }

      if (gimp->config->save_document_history)
        gimp_recent_list_add_uri (uri, mime_type);

      /*  the display owns the image now  */
      g_object_unref (image);
    }

  return image;
}

GimpLayer *
file_open_layer (Gimp                *gimp,
                 GimpContext         *context,
                 GimpProgress        *progress,
                 GimpImage           *dest_image,
                 const gchar         *uri,
                 GimpRunMode          run_mode,
                 GimpPlugInProcedure *file_proc,
                 GimpPDBStatusType   *status,
                 GError             **error)
{
  GimpLayer   *new_layer = NULL;
  GimpImage   *new_image;
  const gchar *mime_type = NULL;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);
  g_return_val_if_fail (GIMP_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (progress == NULL || GIMP_IS_PROGRESS (progress), NULL);
  g_return_val_if_fail (GIMP_IS_IMAGE (dest_image), NULL);
  g_return_val_if_fail (uri != NULL, NULL);
  g_return_val_if_fail (status != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  new_image = file_open_image (gimp, context, progress,
                               uri, uri,
                               file_proc,
                               run_mode,
                               status, &mime_type, error);

  if (new_image)
    {
      GList     *list;
      GimpLayer *layer     = NULL;
      gint       n_visible = 0;

      gimp_image_undo_disable (new_image);

      for (list = GIMP_LIST (new_image->layers)->list;
           list;
           list = g_list_next (list))
        {
          if (gimp_item_get_visible (list->data))
            {
              n_visible++;

              if (! layer)
                layer = list->data;
            }
        }

      if (n_visible > 1)
        layer = gimp_image_merge_visible_layers (new_image, context,
                                                 GIMP_CLIP_TO_IMAGE);

      if (layer)
        {
          GimpItem *item = gimp_item_convert (GIMP_ITEM (layer), dest_image,
                                              G_TYPE_FROM_INSTANCE (layer),
                                              TRUE);

          if (item)
            {
              gchar *basename = file_utils_uri_display_basename (uri);

              new_layer = GIMP_LAYER (item);

              gimp_object_set_name (GIMP_OBJECT (new_layer), basename);
              g_free (basename);

              gimp_document_list_add_uri (GIMP_DOCUMENT_LIST (gimp->documents),
                                          uri, mime_type);

              if (gimp->config->save_document_history)
                gimp_recent_list_add_uri (uri, mime_type);
            }
        }
      else
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                       _("Image doesn't contain any visible layers"));
          *status = GIMP_PDB_EXECUTION_ERROR;
       }

      g_object_unref (new_image);
    }

  return new_layer;
}


/*  private functions  */

static void
file_open_sanitize_image (GimpImage *image)
{
  /* clear all undo steps */
  gimp_image_undo_free (image);

  /* make sure that undo is enabled */
  while (image->undo_freeze_count)
    gimp_image_undo_thaw (image);

  /* set the image to clean  */
  gimp_image_clean_all (image);

  gimp_image_invalidate_layer_previews (image);
  gimp_image_invalidate_channel_previews (image);
  gimp_viewable_invalidate_preview (GIMP_VIEWABLE (image));
}
