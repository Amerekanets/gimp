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

#include <gtk/gtk.h>

#include "libgimpcolor/gimpcolor.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpbase/gimpbase.h"

#include "core-types.h"

#include "base/pixel-region.h"
#include "base/temp-buf.h"
#include "base/tile-manager.h"
#include "base/tile.h"

#include "paint-funcs/paint-funcs.h"

/* FIXME: remove the Path <-> BezierSelect dependency */
#include "tools/tools-types.h"

#include "gimp.h"
#include "gimpcontext.h"
#include "gimpimage.h"
#include "gimpimage-colorhash.h"
#include "gimpimage-mask.h"
#include "gimpimage-undo.h"
#include "gimplayer.h"
#include "gimplayermask.h"
#include "gimplist.h"
#include "gimpmarshal.h"
#include "gimpparasite.h"
#include "gimpundostack.h"

#include "app_procs.h"
#include "drawable.h"
#include "floating_sel.h"
#include "gdisplay.h"
#include "gimprc.h"
#include "parasitelist.h"
#include "path.h"
#include "undo.h"

#include "libgimp/gimpintl.h"


#ifdef DEBUG
#define TRC(x) printf x
#else
#define TRC(x)
#endif


/*  Local function declarations  */
static void     gimp_image_class_init            (GimpImageClass *klass);
static void     gimp_image_init                  (GimpImage      *gimage);
static void     gimp_image_destroy               (GtkObject      *object);
static void     gimp_image_name_changed          (GimpObject     *object);
static void     gimp_image_invalidate_preview    (GimpViewable   *viewable);
static void     gimp_image_size_changed          (GimpViewable   *viewable);
static void     gimp_image_real_colormap_changed (GimpImage      *gimage,
						  gint            ncol);
static TempBuf *gimp_image_get_preview           (GimpViewable   *gimage,
						  gint            width,
						  gint            height);
static TempBuf *gimp_image_get_new_preview       (GimpViewable   *viewable,
						  gint            width, 
						  gint            height);
static void     gimp_image_free_projection       (GimpImage      *gimage);
static void     gimp_image_allocate_shadow       (GimpImage      *gimage,
						  gint            width,
						  gint            height,
						  gint            bpp);
static void     gimp_image_allocate_projection   (GimpImage      *gimage);
static void     gimp_image_construct_layers      (GimpImage      *gimage,
						  gint            x,
						  gint            y,
						  gint            w,
						  gint            h);
static void     gimp_image_construct_channels    (GimpImage      *gimage,
						  gint            x,
						  gint            y,
						  gint            w,
						  gint            h);
static void     gimp_image_initialize_projection (GimpImage      *gimage,
						  gint            x,
						  gint            y,
						  gint            w,
						  gint            h);
static void     gimp_image_get_active_channels   (GimpImage      *gimage,
						  GimpDrawable   *drawable,
						  gint           *active);

/*  projection functions  */
static void     project_intensity                (GimpImage      *gimage,
						  GimpLayer      *layer,
						  PixelRegion    *src,
						  PixelRegion    *dest,
						  PixelRegion    *mask);
static void     project_intensity_alpha          (GimpImage      *gimage,
						  GimpLayer      *layer,
						  PixelRegion    *src,
						  PixelRegion    *dest,
						  PixelRegion    *mask);
static void     project_indexed                  (GimpImage      *gimage,
						  GimpLayer      *layer,
						  PixelRegion    *src,
						  PixelRegion    *dest);
static void     project_indexed_alpha            (GimpImage      *gimage, 
						  GimpLayer      *layer,
						  PixelRegion    *src, 
						  PixelRegion    *dest,
						  PixelRegion    *mask);
static void     project_channel                  (GimpImage      *gimage,
						  GimpChannel    *channel,
						  PixelRegion    *src,
						  PixelRegion    *src2);

/*
 *  Global variables
 */
gint valid_combinations[][MAX_CHANNELS + 1] =
{
  /* RGB GIMAGE */
  { -1, -1, -1, COMBINE_INTEN_INTEN, COMBINE_INTEN_INTEN_A },
  /* RGBA GIMAGE */
  { -1, -1, -1, COMBINE_INTEN_A_INTEN, COMBINE_INTEN_A_INTEN_A },
  /* GRAY GIMAGE */
  { -1, COMBINE_INTEN_INTEN, COMBINE_INTEN_INTEN_A, -1, -1 },
  /* GRAYA GIMAGE */
  { -1, COMBINE_INTEN_A_INTEN, COMBINE_INTEN_A_INTEN_A, -1, -1 },
  /* INDEXED GIMAGE */
  { -1, COMBINE_INDEXED_INDEXED, COMBINE_INDEXED_INDEXED_A, -1, -1 },
  /* INDEXEDA GIMAGE */
  { -1, -1, COMBINE_INDEXED_A_INDEXED_A, -1, -1 },
};


/*
 *  Static variables
 */

enum
{
  MODE_CHANGED,
  ALPHA_CHANGED,
  FLOATING_SELECTION_CHANGED,
  ACTIVE_LAYER_CHANGED,
  ACTIVE_CHANNEL_CHANGED,
  COMPONENT_VISIBILITY_CHANGED,
  COMPONENT_ACTIVE_CHANGED,
  MASK_CHANGED,

  CLEAN,
  DIRTY,
  REPAINT,
  COLORMAP_CHANGED,
  UNDO_EVENT,
  LAST_SIGNAL
};

static guint gimp_image_signals[LAST_SIGNAL] = { 0 };

static GimpViewableClass *parent_class = NULL;

static gint        global_image_ID  = 1;
static GHashTable *gimp_image_table = NULL;

static guint32     next_guide_id    = 1;


GtkType 
gimp_image_get_type (void) 
{
  static GtkType image_type = 0;

  if (! image_type)
    {
      GtkTypeInfo image_info =
      {
        "GimpImage",
        sizeof (GimpImage),
        sizeof (GimpImageClass),
        (GtkClassInitFunc) gimp_image_class_init,
        (GtkObjectInitFunc) gimp_image_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      image_type = gtk_type_unique (GIMP_TYPE_VIEWABLE, &image_info);
    }

  return image_type;
}

static void
gimp_image_class_init (GimpImageClass *klass)
{
  GtkObjectClass    *object_class;
  GimpObjectClass   *gimp_object_class;
  GimpViewableClass *viewable_class;

  object_class      = (GtkObjectClass *) klass;
  gimp_object_class = (GimpObjectClass *) klass;
  viewable_class    = (GimpViewableClass *) klass;

  parent_class = gtk_type_class (GIMP_TYPE_VIEWABLE);

  gimp_image_signals[MODE_CHANGED] =
    gtk_signal_new ("mode_changed",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GimpImageClass,
				       mode_changed),
                    gtk_signal_default_marshaller,
                    GTK_TYPE_NONE, 0);

  gimp_image_signals[ALPHA_CHANGED] =
    gtk_signal_new ("alpha_changed",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GimpImageClass,
				       alpha_changed),
                    gtk_signal_default_marshaller,
                    GTK_TYPE_NONE, 0);

  gimp_image_signals[FLOATING_SELECTION_CHANGED] =
    gtk_signal_new ("floating_selection_changed",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GimpImageClass,
				       floating_selection_changed),
                    gtk_signal_default_marshaller,
                    GTK_TYPE_NONE, 0);

  gimp_image_signals[ACTIVE_LAYER_CHANGED] =
    gtk_signal_new ("active_layer_changed",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GimpImageClass,
				       active_layer_changed),
                    gtk_signal_default_marshaller,
                    GTK_TYPE_NONE, 0);

  gimp_image_signals[ACTIVE_CHANNEL_CHANGED] =
    gtk_signal_new ("active_channel_changed",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GimpImageClass,
				       active_channel_changed),
                    gtk_signal_default_marshaller,
                    GTK_TYPE_NONE, 0);

  gimp_image_signals[COMPONENT_VISIBILITY_CHANGED] =
    gtk_signal_new ("component_visibility_changed",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GimpImageClass,
				       component_visibility_changed),
                    gtk_marshal_NONE__INT,
                    GTK_TYPE_NONE, 1,
		    GTK_TYPE_INT);

  gimp_image_signals[COMPONENT_ACTIVE_CHANGED] =
    gtk_signal_new ("component_active_changed",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GimpImageClass,
				       component_active_changed),
                    gtk_marshal_NONE__INT,
                    GTK_TYPE_NONE, 1,
		    GTK_TYPE_INT);

  gimp_image_signals[MASK_CHANGED] =
    gtk_signal_new ("mask_changed",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GimpImageClass,
				       mask_changed),
                    gtk_signal_default_marshaller,
                    GTK_TYPE_NONE, 0);

  gimp_image_signals[CLEAN] =
    gtk_signal_new ("clean",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GimpImageClass,
				       clean),
                    gtk_signal_default_marshaller,
                    GTK_TYPE_NONE, 0);

  gimp_image_signals[DIRTY] =
    gtk_signal_new ("dirty",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GimpImageClass,
				       dirty),
                    gtk_signal_default_marshaller,
                    GTK_TYPE_NONE, 0);

  gimp_image_signals[REPAINT] =
    gtk_signal_new ("repaint",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GimpImageClass,
				       repaint),
                    gimp_marshal_NONE__INT_INT_INT_INT,
                    GTK_TYPE_NONE, 4,
		    GTK_TYPE_INT,
		    GTK_TYPE_INT,
		    GTK_TYPE_INT,
		    GTK_TYPE_INT);

  gimp_image_signals[COLORMAP_CHANGED] =
    gtk_signal_new ("colormap_changed",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GimpImageClass,
				       colormap_changed),
                    gtk_marshal_NONE__INT,
                    GTK_TYPE_NONE, 1,
		    GTK_TYPE_INT);

  gimp_image_signals[UNDO_EVENT] = 
    gtk_signal_new ("undo_event",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GimpImageClass,
				       undo_event),
                    gtk_marshal_NONE__INT,
                    GTK_TYPE_NONE, 1,
		    GTK_TYPE_INT);

  gtk_object_class_add_signals (object_class, gimp_image_signals, LAST_SIGNAL);

  object_class->destroy               = gimp_image_destroy;

  gimp_object_class->name_changed     = gimp_image_name_changed;

  viewable_class->invalidate_preview  = gimp_image_invalidate_preview;
  viewable_class->size_changed        = gimp_image_size_changed;
  viewable_class->get_preview         = gimp_image_get_preview;
  viewable_class->get_new_preview     = gimp_image_get_new_preview;

  klass->mode_changed                 = NULL;
  klass->alpha_changed                = NULL;
  klass->floating_selection_changed   = NULL;
  klass->active_layer_changed         = NULL;
  klass->active_channel_changed       = NULL;
  klass->component_visibility_changed = NULL;
  klass->component_active_changed     = NULL;
  klass->mask_changed                 = NULL;

  klass->clean                        = NULL;
  klass->dirty                        = NULL;
  klass->repaint                      = NULL;
  klass->colormap_changed             = gimp_image_real_colormap_changed;
  klass->undo_event                   = NULL;
  klass->undo                         = gimp_image_undo;
  klass->redo                         = gimp_image_redo;

  gimp_image_color_hash_init ();
}


/* static functions */

static void 
gimp_image_init (GimpImage *gimage)
{
  gimage->ID                    = global_image_ID++;

  gimage->save_proc             = NULL;

  gimage->width                 = 0;
  gimage->height                = 0;
  gimage->xresolution           = gimprc.default_xresolution;
  gimage->yresolution           = gimprc.default_yresolution;
  gimage->unit                  = gimprc.default_units;
  gimage->base_type             = RGB;

  gimage->cmap                  = NULL;
  gimage->num_cols              = 0;

  gimage->dirty                 = 1;
  gimage->undo_on               = TRUE;

  gimage->instance_count        = 0;
  gimage->disp_count            = 0;

  gimage->tattoo_state          = 0;

  gimage->shadow                = NULL;

  gimage->construct_flag        = -1;
  gimage->proj_type             = RGBA_GIMAGE;
  gimage->projection            = NULL;

  gimage->guides                = NULL;

  gimage->layers                = gimp_list_new (GIMP_TYPE_LAYER, 
						 GIMP_CONTAINER_POLICY_STRONG);
  gimage->channels              = gimp_list_new (GIMP_TYPE_CHANNEL, 
						 GIMP_CONTAINER_POLICY_STRONG);
  gimage->layer_stack           = NULL;

  gimage->active_layer          = NULL;
  gimage->active_channel        = NULL;
  gimage->floating_sel          = NULL;
  gimage->selection_mask        = NULL;

  gimage->parasites             = gimp_parasite_list_new ();

  gimage->paths                 = NULL;

  gimage->qmask_state           = FALSE;
  gimage->qmask_color.r         = 1.0;
  gimage->qmask_color.g         = 0.0;
  gimage->qmask_color.b         = 0.0;
  gimage->qmask_color.a         = 0.5;

  gimage->undo_stack            = NULL;
  gimage->redo_stack            = NULL;
  gimage->undo_bytes            = 0;
  gimage->undo_levels           = 0;
  gimage->group_count           = 0;
  gimage->pushing_undo_group    = UNDO_NULL;
  gimage->undo_history          = NULL;

  gimage->new_undo_stack        = gimp_undo_stack_new (gimage);
  gimage->new_redo_stack        = gimp_undo_stack_new (gimage);

  gimage->comp_preview          = NULL;
  gimage->comp_preview_valid    = FALSE;

  if (gimp_image_table == NULL)
    gimp_image_table = g_hash_table_new (g_direct_hash, NULL);

  g_hash_table_insert (gimp_image_table,
		       GINT_TO_POINTER (gimage->ID),
		       (gpointer) gimage);
}

static void
gimp_image_destroy (GtkObject *object)
{
  GimpImage *gimage;

  gimage = GIMP_IMAGE (object);

  g_hash_table_remove (gimp_image_table, GINT_TO_POINTER (gimage->ID));

  gimp_image_free_projection (gimage);
  gimp_image_free_shadow (gimage);
  
  if (gimage->cmap)
    g_free (gimage->cmap);
  
  gtk_object_unref (GTK_OBJECT (gimage->layers));
  gtk_object_unref (GTK_OBJECT (gimage->channels));
  g_slist_free (gimage->layer_stack);

  gtk_object_unref (GTK_OBJECT (gimage->selection_mask));

  if (gimage->comp_preview)
    temp_buf_free (gimage->comp_preview);

  if (gimage->parasites)
    gtk_object_unref (GTK_OBJECT (gimage->parasites));

  g_list_foreach (gimage->guides, (GFunc) g_free, NULL);
  g_list_free (gimage->guides);

  gtk_object_unref (GTK_OBJECT (gimage->new_undo_stack));
  gtk_object_unref (GTK_OBJECT (gimage->new_redo_stack));
}

static void
gimp_image_name_changed (GimpObject *object)
{
  GimpImage   *gimage;
  const gchar *name;

  if (GIMP_OBJECT_CLASS (parent_class)->name_changed)
    GIMP_OBJECT_CLASS (parent_class)->name_changed (object);

  gimage = GIMP_IMAGE (object);
  name   = gimp_object_get_name (object);

  if (! (name && strlen (name)))
    {
      g_free (object->name);
      object->name = NULL;
    }
}

static void
gimp_image_invalidate_preview (GimpViewable *viewable)
{
  GimpImage *gimage;

  if (GIMP_VIEWABLE_CLASS (parent_class)->invalidate_preview)
    GIMP_VIEWABLE_CLASS (parent_class)->invalidate_preview (viewable);

  gimage = GIMP_IMAGE (viewable);

  gimage->comp_preview_valid = FALSE;
}

static void
gimp_image_size_changed (GimpViewable *viewable)
{
  GimpImage *gimage;

  if (GIMP_VIEWABLE_CLASS (parent_class)->size_changed)
    GIMP_VIEWABLE_CLASS (parent_class)->size_changed (viewable);

  gimage = GIMP_IMAGE (viewable);

  gimp_image_invalidate_layer_previews (gimage);
  gimp_image_invalidate_channel_previews (gimage);
}

static void 
gimp_image_real_colormap_changed (GimpImage *gimage,
				  gint       ncol)
{
  if (gimp_image_base_type (gimage) == INDEXED)
    gimp_image_color_hash_invalidate (gimage, ncol);
}

static void
gimp_image_allocate_projection (GimpImage *gimage)
{
  if (gimage->projection)
    gimp_image_free_projection (gimage);

  /*  Find the number of bytes required for the projection.
   *  This includes the intensity channels and an alpha channel
   *  if one doesn't exist.
   */
  switch (gimp_image_base_type (gimage))
    {
    case RGB:
    case INDEXED:
      gimage->proj_bytes = 4;
      gimage->proj_type = RGBA_GIMAGE;
      break;
    case GRAY:
      gimage->proj_bytes = 2;
      gimage->proj_type = GRAYA_GIMAGE;
      break;
    default:
      g_assert_not_reached ();
    }

  /*  allocate the new projection  */
  gimage->projection = tile_manager_new (gimage->width, gimage->height,
					 gimage->proj_bytes);
  tile_manager_set_user_data (gimage->projection, (void *) gimage);
  tile_manager_set_validate_proc (gimage->projection, gimp_image_validate);
}

static void
gimp_image_free_projection (GimpImage *gimage)
{
  if (gimage->projection)
    tile_manager_destroy (gimage->projection);

  gimage->projection = NULL;
}

static void
gimp_image_allocate_shadow (GimpImage *gimage, 
			    gint       width, 
			    gint       height, 
			    gint       bpp)
{
  /*  allocate the new projection  */
  gimage->shadow = tile_manager_new (width, height, bpp);
}


/* function definitions */

GimpImage *
gimp_image_new (Gimp              *gimp,
		gint               width,
		gint               height,
		GimpImageBaseType  base_type)
{
  GimpImage *gimage;
  gint       i;

  g_return_val_if_fail (gimp != NULL, NULL);
  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);

  gimage = GIMP_IMAGE (gtk_type_new (GIMP_TYPE_IMAGE));

  gimage->gimp      = gimp;
  gimage->width     = width;
  gimage->height    = height;
  gimage->base_type = base_type;

  switch (base_type)
    {
    case RGB:
    case GRAY:
      break;
    case INDEXED:
      /* always allocate 256 colors for the colormap */
      gimage->num_cols = 0;
      gimage->cmap     = (guchar *) g_malloc0 (COLORMAP_SIZE);
      break;
    default:
      break;
    }

  /*  set all color channels visible and active  */
  for (i = 0; i < MAX_CHANNELS; i++)
    {
      gimage->visible[i] = TRUE;
      gimage->active[i]  = TRUE;
    }

  /* create the selection mask */
  gimage->selection_mask = gimp_channel_new_mask (gimage,
						  gimage->width,
						  gimage->height);


  return gimage;
}

gint
gimp_image_get_ID (GimpImage *gimage)
{
  g_return_val_if_fail (gimage != NULL, -1);
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), -1);

  return gimage->ID;
}

GimpImage *
gimp_image_get_by_ID (gint image_id)
{
  if (gimp_image_table == NULL)
    return NULL;

  return (GimpImage *) g_hash_table_lookup (gimp_image_table, 
					    GINT_TO_POINTER (image_id));
}

void
gimp_image_set_filename (GimpImage   *gimage,
			 const gchar *filename)
{
  gimp_object_set_name (GIMP_OBJECT (gimage), filename);
}

void
gimp_image_set_resolution (GimpImage *gimage,
			   gdouble    xresolution,
			   gdouble    yresolution)
{
  /* nothing to do if setting res to the same as before */
  if ((ABS (gimage->xresolution - xresolution) < 1e-5) &&
      (ABS (gimage->yresolution - yresolution) < 1e-5))
      return;

  /* don't allow to set the resolution out of bounds */
  if (xresolution < GIMP_MIN_RESOLUTION || xresolution > GIMP_MAX_RESOLUTION ||
      yresolution < GIMP_MIN_RESOLUTION || yresolution > GIMP_MAX_RESOLUTION)
    return;

  undo_push_resolution (gimage);

  gimage->xresolution = xresolution;
  gimage->yresolution = yresolution;

  /* really just want to recalc size and repaint */
  gdisplays_shrink_wrap (gimage);
}

void
gimp_image_get_resolution (const GimpImage *gimage,
			   gdouble         *xresolution,
			   gdouble         *yresolution)
{
  g_return_if_fail (xresolution && yresolution);

  *xresolution = gimage->xresolution;
  *yresolution = gimage->yresolution;
}

void
gimp_image_set_unit (GimpImage *gimage,
		     GimpUnit   unit)
{
  undo_push_resolution (gimage);

  gimage->unit = unit;
}

GimpUnit
gimp_image_get_unit (const GimpImage *gimage)
{
  return gimage->unit;
}

void
gimp_image_set_save_proc (GimpImage     *gimage, 
			  PlugInProcDef *proc)
{
  gimage->save_proc = proc;
}

PlugInProcDef *
gimp_image_get_save_proc (const GimpImage *gimage)
{
  return gimage->save_proc;
}

gint
gimp_image_get_width (const GimpImage *gimage)
{
  return gimage->width;
}

gint
gimp_image_get_height (const GimpImage *gimage)
{
  return gimage->height;
}

void
gimp_image_resize (GimpImage *gimage, 
		   gint       new_width, 
		   gint       new_height,
		   gint       offset_x, 
		   gint       offset_y)
{
  GimpChannel *channel;
  GimpLayer   *layer;
  GimpLayer   *floating_layer;
  GList       *list;
  GList       *guide_list;

  gimp_set_busy ();

  g_assert (new_width > 0 && new_height > 0);

  /*  Get the floating layer if one exists  */
  floating_layer = gimp_image_floating_sel (gimage);

  undo_push_group_start (gimage, IMAGE_RESIZE_UNDO);

  /*  Relax the floating selection  */
  if (floating_layer)
    floating_sel_relax (floating_layer, TRUE);

  /*  Push the image size to the stack  */
  undo_push_gimage_mod (gimage);

  /*  Set the new width and height  */
  gimage->width  = new_width;
  gimage->height = new_height;

  /*  Resize all channels  */
  for (list = GIMP_LIST (gimage->channels)->list; 
       list; 
       list = g_list_next (list))
    {
      channel = (GimpChannel *) list->data;

      gimp_channel_resize (channel, new_width, new_height, offset_x, offset_y);
    }

  /*  Reposition or remove any guides  */
  guide_list = gimage->guides;
  while (guide_list)
    {
      GimpGuide *guide;

      guide = (GimpGuide *) guide_list->data;
      guide_list = g_list_next (guide_list);

      switch (guide->orientation)
	{
	case ORIENTATION_HORIZONTAL:
	  undo_push_guide (gimage, guide);
	  guide->position += offset_y;
	  if (guide->position < 0 || guide->position > new_height)
	    gimp_image_delete_guide (gimage, guide);
	  break;

	case ORIENTATION_VERTICAL:
	  undo_push_guide (gimage, guide);
	  guide->position += offset_x;
	  if (guide->position < 0 || guide->position > new_width)
	    gimp_image_delete_guide (gimage, guide);
	  break;

	default:
	  g_error ("Unknown guide orientation\n");
	}
    }

  /*  Don't forget the selection mask!  */
  gimp_channel_resize (gimage->selection_mask,
		       new_width, new_height, offset_x, offset_y);
  gimage_mask_invalidate (gimage);

  /*  Reposition all layers  */
  for (list = GIMP_LIST (gimage->layers)->list; 
       list; 
       list = g_list_next (list))
    {
      layer = (GimpLayer *) list->data;

      gimp_layer_translate (layer, offset_x, offset_y);
    }

  /*  Make sure the projection matches the gimage size  */
  gimp_image_projection_realloc (gimage);

  /*  Rigor the floating selection  */
  if (floating_layer)
    floating_sel_rigor (floating_layer, TRUE);

  undo_push_group_end (gimage);

  gimp_viewable_size_changed (GIMP_VIEWABLE (gimage));

  gimp_unset_busy ();
}

void
gimp_image_scale (GimpImage *gimage, 
		  gint       new_width, 
		  gint       new_height)
{
  GimpChannel *channel;
  GimpLayer   *layer;
  GimpLayer   *floating_layer;
  GList       *list;
  GSList      *remove = NULL;
  GSList      *slist;
  GimpGuide   *guide;
  gint         old_width;
  gint         old_height;
  gdouble      img_scale_w = 1.0;
  gdouble      img_scale_h = 1.0;

  if ((new_width == 0) || (new_height == 0))
    {
      g_message ("%s(): Scaling to zero width or height has been rejected.",
		 G_GNUC_FUNCTION);
      return;
    }

  gimp_set_busy ();

  /*  Get the floating layer if one exists  */
  floating_layer = gimp_image_floating_sel (gimage);

  undo_push_group_start (gimage, IMAGE_SCALE_UNDO);

  /*  Relax the floating selection  */
  if (floating_layer)
    floating_sel_relax (floating_layer, TRUE);

  /*  Push the image size to the stack  */
  undo_push_gimage_mod (gimage);

  /*  Set the new width and height  */

  old_width      = gimage->width;
  old_height     = gimage->height;
  gimage->width  = new_width;
  gimage->height = new_height;
  img_scale_w    = (gdouble) new_width  / (gdouble) old_width;
  img_scale_h    = (gdouble) new_height / (gdouble) old_height;
 
  /*  Scale all channels  */
  for (list = GIMP_LIST (gimage->channels)->list; 
       list; 
       list = g_list_next (list))
    {
      channel = (GimpChannel *) list->data;

      gimp_channel_scale (channel, new_width, new_height);
    }

  /*  Don't forget the selection mask!  */
  /*  if (channel_is_empty(gimage->selection_mask))
        gimp_channel_resize(gimage->selection_mask, new_width, new_height, 0, 0)
      else
  */

  gimp_channel_scale (gimage->selection_mask, new_width, new_height);
  gimage_mask_invalidate (gimage);

  /*  Scale all layers  */
  for (list = GIMP_LIST (gimage->layers)->list; 
       list; 
       list = g_list_next (list))
    {
      layer = (GimpLayer *) list->data;

      if (! gimp_layer_scale_by_factors (layer, img_scale_w, img_scale_h))
	{
	  /* Since 0 < img_scale_w, img_scale_h, failure due to one or more
	   * vanishing scaled layer dimensions. Implicit delete implemented
	   * here. Upstream warning implemented in resize_check_layer_scaling()
	   * [resize.c line 1295], which offers the user the chance to bail out.
	   */
          remove = g_slist_append (remove, layer);
        }
    }

  /* We defer removing layers lost to scaling until now so as not to mix
   * the operations of iterating over and removal from gimage->layers.
   */  
  for (slist = remove; slist; slist = g_slist_next (slist))
    {
      layer = slist->data;
      gimp_image_remove_layer (gimage, layer);
    }
  g_slist_free (remove);

  /*  Scale any Guides  */
  for (list = gimage->guides; list; list = g_list_next (list))
    {
      guide = (GimpGuide *) list->data;

      switch (guide->orientation)
	{
	case ORIENTATION_HORIZONTAL:
	  undo_push_guide (gimage, guide);
	  guide->position = (guide->position * new_height) / old_height;
	  break;
	case ORIENTATION_VERTICAL:
	  undo_push_guide (gimage, guide);
	  guide->position = (guide->position * new_width) / old_width;
	  break;
	default:
	  g_error("Unknown guide orientation II.\n");
	}
    }

  /*  Make sure the projection matches the gimage size  */
  gimp_image_projection_realloc (gimage);

  /*  Rigor the floating selection  */
  if (floating_layer)
    floating_sel_rigor (floating_layer, TRUE);

  undo_push_group_end (gimage);

  gimp_viewable_size_changed (GIMP_VIEWABLE (gimage));

  gimp_unset_busy ();
}

/**
 * gimp_image_check_scaling:
 * @gimage:     A #GimpImage.
 * @new_width:  The new width.
 * @new_height: The new height.
 * 
 * Inventory the layer list in gimage and return #TRUE if, after
 * scaling, they all retain positive x and y pixel dimensions.
 * 
 * Return value: #TRUE if scaling the image will shrink none of it's
 *               layers completely away.
 **/
gboolean 
gimp_image_check_scaling (const GimpImage *gimage,
			  gint             new_width,
			  gint             new_height)
{
  GList *list;

  g_return_val_if_fail (gimage != NULL, FALSE);
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);

  for (list = GIMP_LIST (gimage->layers)->list;
       list;
       list = g_list_next (list))
    {
      GimpLayer *layer;

      layer = (GimpLayer *) list->data;

      if (! gimp_layer_check_scaling (layer, new_width, new_height))
	return FALSE;
    }

  return TRUE;
}

TileManager *
gimp_image_shadow (GimpImage *gimage, 
		   gint       width, 
		   gint       height, 
		   gint       bpp)
{
  if (gimage->shadow &&
      ((width != tile_manager_width (gimage->shadow)) ||
       (height != tile_manager_height (gimage->shadow)) ||
       (bpp != tile_manager_bpp (gimage->shadow))))
    gimp_image_free_shadow (gimage);
  else if (gimage->shadow)
    return gimage->shadow;

  gimp_image_allocate_shadow (gimage, width, height, bpp);

  return gimage->shadow;
}

void
gimp_image_free_shadow (GimpImage *gimage)
{
  /*  Free the shadow buffer from the specified gimage if it exists  */
  if (gimage->shadow)
    tile_manager_destroy (gimage->shadow);

  gimage->shadow = NULL;
}

void
gimp_image_apply_image (GimpImage	 *gimage,
			GimpDrawable	 *drawable,
			PixelRegion	 *src2PR,
			gboolean          undo,
			gint              opacity,
			LayerModeEffects  mode,
			/*  alternative to using drawable tiles as src1: */
			TileManager	 *src1_tiles,
			gint              x,
			gint              y)
{
  GimpChannel *mask;
  gint         x1, y1, x2, y2;
  gint         offset_x, offset_y;
  PixelRegion  src1PR, destPR, maskPR;
  gint         operation;
  gint         active [MAX_CHANNELS];

  /*  get the selection mask if one exists  */
  mask = (gimage_mask_is_empty (gimage)) ? NULL : gimp_image_get_mask (gimage);

  /*  configure the active channel array  */
  gimp_image_get_active_channels (gimage, drawable, active);

  /*  determine what sort of operation is being attempted and
   *  if it's actually legal...
   */
  operation = valid_combinations[gimp_drawable_type (drawable)][src2PR->bytes];
  if (operation == -1)
    {
      g_message ("%s(): illegal parameters", G_GNUC_FUNCTION);
      return;
    }

  /*  get the layer offsets  */
  gimp_drawable_offsets (drawable, &offset_x, &offset_y);

  /*  make sure the image application coordinates are within gimage bounds  */
  x1 = CLAMP (x, 0, gimp_drawable_width  (drawable));
  y1 = CLAMP (y, 0, gimp_drawable_height (drawable));
  x2 = CLAMP (x + src2PR->w, 0, gimp_drawable_width  (drawable));
  y2 = CLAMP (y + src2PR->h, 0, gimp_drawable_height (drawable));

  if (mask)
    {
      /*  make sure coordinates are in mask bounds ...
       *  we need to add the layer offset to transform coords
       *  into the mask coordinate system
       */
      x1 = CLAMP (x1, -offset_x, gimp_drawable_width (GIMP_DRAWABLE (mask))-offset_x);
      y1 = CLAMP (y1, -offset_y, gimp_drawable_height(GIMP_DRAWABLE (mask))-offset_y);
      x2 = CLAMP (x2, -offset_x, gimp_drawable_width (GIMP_DRAWABLE (mask))-offset_x);
      y2 = CLAMP (y2, -offset_y, gimp_drawable_height(GIMP_DRAWABLE (mask))-offset_y);
    }

  /*  If the calling procedure specified an undo step...  */
  if (undo)
    undo_push_image (gimp_drawable_gimage (drawable), drawable, x1, y1, x2, y2);

  /* configure the pixel regions
   *  If an alternative to using the drawable's data as src1 was provided...
   */
  if (src1_tiles)
    pixel_region_init (&src1PR, src1_tiles, 
		       x1, y1, (x2 - x1), (y2 - y1), FALSE);
  else
    pixel_region_init (&src1PR, gimp_drawable_data (drawable), 
		       x1, y1, (x2 - x1), (y2 - y1), FALSE);
  pixel_region_init (&destPR, gimp_drawable_data (drawable), 
		     x1, y1, (x2 - x1), (y2 - y1), TRUE);
  pixel_region_resize (src2PR, 
		       src2PR->x + (x1 - x), src2PR->y + (y1 - y), 
		       (x2 - x1), (y2 - y1));

  if (mask)
    {
      gint mx, my;

      /*  configure the mask pixel region
       *  don't use x1 and y1 because they are in layer
       *  coordinate system.  Need mask coordinate system
       */
      mx = x1 + offset_x;
      my = y1 + offset_y;

      pixel_region_init (&maskPR, 
			 gimp_drawable_data (GIMP_DRAWABLE (mask)), 
			 mx, my, 
			 (x2 - x1), (y2 - y1), 
			 FALSE);
      combine_regions (&src1PR, src2PR, &destPR, &maskPR, NULL,
		       opacity, mode, active, operation);
    }
  else
    {
      combine_regions (&src1PR, src2PR, &destPR, NULL, NULL,
		       opacity, mode, active, operation);
    }
}

/* Similar to gimp_image_apply_image but works in "replace" mode (i.e.
   transparent pixels in src2 make the result transparent rather
   than opaque.

   Takes an additional mask pixel region as well.
*/
void
gimp_image_replace_image (GimpImage    *gimage, 
			  GimpDrawable *drawable, 
			  PixelRegion  *src2PR,
			  gboolean      undo, 
			  gint          opacity,
			  PixelRegion  *maskPR,
			  gint          x, 
			  gint          y)
{
  GimpChannel *mask;
  gint         x1, y1, x2, y2;
  gint         offset_x, offset_y;
  PixelRegion  src1PR, destPR;
  PixelRegion  mask2PR, tempPR;
  guchar      *temp_data;
  gint         operation;
  gint         active [MAX_CHANNELS];

  /*  get the selection mask if one exists  */
  mask = (gimage_mask_is_empty (gimage)) ? NULL : gimp_image_get_mask (gimage);

  /*  configure the active channel array  */
  gimp_image_get_active_channels (gimage, drawable, active);

  /*  determine what sort of operation is being attempted and
   *  if it's actually legal...
   */
  operation = valid_combinations [gimp_drawable_type (drawable)][src2PR->bytes];
  if (operation == -1)
    {
      g_message ("%s(): got illegal parameters", G_GNUC_FUNCTION);
      return;
    }

  /*  get the layer offsets  */
  gimp_drawable_offsets (drawable, &offset_x, &offset_y);

  /*  make sure the image application coordinates are within gimage bounds  */
  x1 = CLAMP (x, 0, gimp_drawable_width (drawable));
  y1 = CLAMP (y, 0, gimp_drawable_height (drawable));
  x2 = CLAMP (x + src2PR->w, 0, gimp_drawable_width (drawable));
  y2 = CLAMP (y + src2PR->h, 0, gimp_drawable_height (drawable));

  if (mask)
    {
      /*  make sure coordinates are in mask bounds ...
       *  we need to add the layer offset to transform coords
       *  into the mask coordinate system
       */
      x1 = CLAMP (x1, -offset_x, gimp_drawable_width (GIMP_DRAWABLE (mask))-offset_x);
      y1 = CLAMP (y1, -offset_y, gimp_drawable_height(GIMP_DRAWABLE (mask))-offset_y);
      x2 = CLAMP (x2, -offset_x, gimp_drawable_width (GIMP_DRAWABLE (mask))-offset_x);
      y2 = CLAMP (y2, -offset_y, gimp_drawable_height(GIMP_DRAWABLE (mask))-offset_y);
    }

  /*  If the calling procedure specified an undo step...  */
  if (undo)
    drawable_apply_image (drawable, x1, y1, x2, y2, NULL, FALSE);

  /* configure the pixel regions
   *  If an alternative to using the drawable's data as src1 was provided...
   */
  pixel_region_init (&src1PR, gimp_drawable_data (drawable),
		     x1, y1, (x2 - x1), (y2 - y1), FALSE);
  pixel_region_init (&destPR, gimp_drawable_data (drawable),
		     x1, y1, (x2 - x1), (y2 - y1), TRUE);
  pixel_region_resize (src2PR,
		       src2PR->x + (x1 - x), src2PR->y + (y1 - y),
		       (x2 - x1), (y2 - y1));

  if (mask)
    {
      int mx, my;

      /*  configure the mask pixel region
       *  don't use x1 and y1 because they are in layer
       *  coordinate system.  Need mask coordinate system
       */
      mx = x1 + offset_x;
      my = y1 + offset_y;

      pixel_region_init (&mask2PR, 
			 gimp_drawable_data (GIMP_DRAWABLE (mask)), 
			 mx, my, 
			 (x2 - x1), (y2 - y1), 
			 FALSE);

      tempPR.bytes = 1;
      tempPR.x = 0;
      tempPR.y = 0;
      tempPR.w = x2 - x1;
      tempPR.h = y2 - y1;
      tempPR.rowstride = tempPR.w * tempPR.bytes;
      temp_data = g_malloc (tempPR.h * tempPR.rowstride);
      tempPR.data = temp_data;

      copy_region (&mask2PR, &tempPR);

      /* apparently, region operations can mutate some PR data. */
      tempPR.x = 0;
      tempPR.y = 0;
      tempPR.w = x2 - x1;
      tempPR.h = y2 - y1;
      tempPR.data = temp_data;

      apply_mask_to_region (&tempPR, maskPR, OPAQUE_OPACITY);

      tempPR.x = 0;
      tempPR.y = 0;
      tempPR.w = x2 - x1;
      tempPR.h = y2 - y1;
      tempPR.data = temp_data;

      combine_regions_replace (&src1PR, src2PR, &destPR, &tempPR, NULL,
		       opacity, active, operation);

      g_free (temp_data);
    }
  else
    {
      combine_regions_replace (&src1PR, src2PR, &destPR, maskPR, NULL,
			       opacity, active, operation);
    }
}

/* Get rid of these! A "foreground" is an UI concept.. */

void
gimp_image_get_foreground (const GimpImage    *gimage, 
			   const GimpDrawable *drawable, 
			   guchar             *fg)
{
  GimpRGB  color;
  guchar   pfg[3];

  gimp_context_get_foreground (NULL, &color);

  gimp_rgb_get_uchar (&color, &pfg[0], &pfg[1], &pfg[2]);

  gimp_image_transform_color (gimage, drawable, pfg, fg, RGB);
}

void
gimp_image_get_background (const GimpImage    *gimage, 
			   const GimpDrawable *drawable, 
			   guchar             *bg)
{
  GimpRGB  color;
  guchar   pbg[3];

  /*  Get the palette color  */
  gimp_context_get_background (NULL, &color);

  gimp_rgb_get_uchar (&color, &pbg[0], &pbg[1], &pbg[2]);

  gimp_image_transform_color (gimage, drawable, pbg, bg, RGB);
}

guchar *
gimp_image_get_color_at (GimpImage *gimage, 
			 gint       x, 
			 gint       y)
{
  Tile   *tile;
  guchar *src;
  guchar *dest;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  if (x < 0 || y < 0 || x >= gimage->width || y >= gimage->height)
    return NULL;
  
  dest = g_new (guchar, 5);
  tile = tile_manager_get_tile (gimp_image_composite (gimage), x, y,
				TRUE, FALSE);
  src = tile_data_pointer (tile, x % TILE_WIDTH, y % TILE_HEIGHT);
  gimp_image_get_color (gimage, gimp_image_composite_type (gimage), dest, src);

  if (GIMP_IMAGE_TYPE_HAS_ALPHA (gimp_image_composite_type (gimage)))
    dest[3] = src[gimp_image_composite_bytes (gimage) - 1];
  else
    dest[3] = 255;

  dest[4] = 0;
  tile_release (tile, FALSE);

  return dest;
}

void
gimp_image_get_color (const GimpImage *gimage, 
		      GimpImageType    d_type,
		      guchar          *rgb, 
		      guchar          *src)
{
  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  switch (d_type)
    {
    case RGB_GIMAGE: case RGBA_GIMAGE:
      map_to_color (0, NULL, src, rgb);
      break;
    case GRAY_GIMAGE: case GRAYA_GIMAGE:
      map_to_color (1, NULL, src, rgb);
      break;
    case INDEXED_GIMAGE: case INDEXEDA_GIMAGE:
      map_to_color (2, gimage->cmap, src, rgb);
      break;
    }
}

void
gimp_image_transform_color (const GimpImage    *gimage, 
			    const GimpDrawable *drawable,
			    guchar             *src, 
			    guchar             *dest, 
			    GimpImageBaseType   type)
{
  GimpImageType d_type;

  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  d_type = (drawable != NULL) ? gimp_drawable_type (drawable) :
    gimp_image_base_type_with_alpha (gimage);

  switch (type)
    {
    case RGB:
      switch (d_type)
	{
	case RGB_GIMAGE: case RGBA_GIMAGE:
	  /*  Straight copy  */
	  *dest++ = *src++;
	  *dest++ = *src++;
	  *dest++ = *src++;
	  break;
	case GRAY_GIMAGE: case GRAYA_GIMAGE:
	  /*  NTSC conversion  */
	  *dest = INTENSITY (src[RED_PIX],
			     src[GREEN_PIX],
			     src[BLUE_PIX]);
	  break;
	case INDEXED_GIMAGE: case INDEXEDA_GIMAGE:
	  /*  Least squares method  */
	  *dest = gimp_image_color_hash_rgb_to_indexed (gimage,
							src[RED_PIX],
							src[GREEN_PIX],
							src[BLUE_PIX]);
	  break;
	}
      break;
    case GRAY:
      switch (d_type)
	{
	case RGB_GIMAGE: case RGBA_GIMAGE:
	  /*  Gray to RG&B */
	  *dest++ = *src;
	  *dest++ = *src;
	  *dest++ = *src;
	  break;
	case GRAY_GIMAGE: case GRAYA_GIMAGE:
	  /*  Straight copy  */
	  *dest = *src;
	  break;
	case INDEXED_GIMAGE: case INDEXEDA_GIMAGE:
	  /*  Least squares method  */
	  *dest = gimp_image_color_hash_rgb_to_indexed (gimage,
							src[GRAY_PIX],
							src[GRAY_PIX],
							src[GRAY_PIX]);
	  break;
	}
      break;
    default:
      break;
    }
}

GimpGuide *
gimp_image_add_hguide (GimpImage *gimage)
{
  GimpGuide *guide;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  guide = g_new (GimpGuide, 1);

  guide->ref_count   = 0;
  guide->position    = -1;
  guide->guide_ID    = next_guide_id++;
  guide->orientation = ORIENTATION_HORIZONTAL;

  gimage->guides = g_list_prepend (gimage->guides, guide);

  return guide;
}

GimpGuide *
gimp_image_add_vguide (GimpImage *gimage)
{
  GimpGuide *guide;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  guide = g_new (GimpGuide, 1);

  guide->ref_count   = 0;
  guide->position    = -1;
  guide->guide_ID    = next_guide_id++;
  guide->orientation = ORIENTATION_VERTICAL;

  gimage->guides = g_list_prepend (gimage->guides, guide);

  return guide;
}

void
gimp_image_add_guide (GimpImage *gimage,
		      GimpGuide *guide)
{
  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  gimage->guides = g_list_prepend (gimage->guides, guide);
}

void
gimp_image_remove_guide (GimpImage *gimage,
			 GimpGuide *guide)
{
  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  gimage->guides = g_list_remove (gimage->guides, guide);
}

void
gimp_image_delete_guide (GimpImage *gimage,
			 GimpGuide *guide) 
{
  guide->position = -1;

  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  if (guide->ref_count <= 0)
    {
      gimage->guides = g_list_remove (gimage->guides, guide);
      g_free (guide);
    }
}


GimpParasite *
gimp_image_parasite_find (const GimpImage *gimage, 
			  const gchar     *name)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  return gimp_parasite_list_find (gimage->parasites, name);
}

static void
list_func (gchar          *key, 
	   GimpParasite   *p, 
	   gchar        ***cur)
{
  *(*cur)++ = (gchar *) g_strdup (key);
}

gchar **
gimp_image_parasite_list (const GimpImage *gimage, 
			  gint            *count)
{
  gchar **list;
  gchar **cur;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  *count = gimp_parasite_list_length (gimage->parasites);
  cur = list = g_new (gchar*, *count);

  gimp_parasite_list_foreach (gimage->parasites, (GHFunc) list_func, &cur);
  
  return list;
}

void
gimp_image_parasite_attach (GimpImage    *gimage, 
			    GimpParasite *parasite)
{
  g_return_if_fail (GIMP_IS_IMAGE (gimage) && parasite != NULL);

  /* only set the dirty bit manually if we can be saved and the new
     parasite differs from the current one and we aren't undoable */
  if (gimp_parasite_is_undoable (parasite))
    undo_push_image_parasite (gimage, parasite);

  /*  We used to push an cantundo on te stack here. This made the undo stack
      unusable (NULL on the stack) and prevented people from undoing after a 
      save (since most save plug-ins attach an undoable comment parasite).
      Now we simply attach the parasite without pushing an undo. That way it's
      undoable but does not block the undo system.   --Sven
   */

  gimp_parasite_list_add (gimage->parasites, parasite);

  if (gimp_parasite_has_flag (parasite, GIMP_PARASITE_ATTACH_PARENT))
    {
      gimp_parasite_shift_parent (parasite);
      gimp_parasite_attach (gimage->gimp, parasite);
    }
}

void
gimp_image_parasite_detach (GimpImage   *gimage, 
			    const gchar *parasite)
{
  GimpParasite *p;

  g_return_if_fail (GIMP_IS_IMAGE (gimage) && parasite != NULL);

  if (!(p = gimp_parasite_list_find (gimage->parasites, parasite)))
    return;

  if (gimp_parasite_is_undoable (p))
    undo_push_image_parasite_remove (gimage, gimp_parasite_name (p));

  gimp_parasite_list_remove (gimage->parasites, parasite);
}

Tattoo
gimp_image_get_new_tattoo (GimpImage *image)
{
  image->tattoo_state++;

  if (image->tattoo_state <= 0)
    g_warning ("Tattoo state has become corrupt (2.1 billion operation limit exceded)");

  return image->tattoo_state;
}

Tattoo
gimp_image_get_tattoo_state (GimpImage *image)
{
  return (image->tattoo_state);
}

gboolean
gimp_image_set_tattoo_state (GimpImage *gimage, 
			     Tattoo     val)
{
  GList       *list;
  gboolean     retval = TRUE;
  GimpChannel *channel;
  Tattoo       maxval = 0;
  Path        *pptr   = NULL;
  PathList    *plist;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);

  for (list = GIMP_LIST (gimage->layers)->list; 
       list; 
       list = g_list_next (list))
    {
      Tattoo ltattoo;
      
      ltattoo = gimp_drawable_get_tattoo (GIMP_DRAWABLE (list->data));
      if (ltattoo > maxval)
	maxval = ltattoo;
      if (gimp_image_get_channel_by_tattoo (gimage, ltattoo) != NULL)
	{
	  retval = FALSE; /* Oopps duplicated tattoo in channel */
	}

      /* Now check path an't got this tattoo */
      if (path_get_path_by_tattoo (gimage, ltattoo) != NULL)
	{
	  retval = FALSE; /* Oopps duplicated tattoo in layer */
	}
    }

  /* Now check that the paths channel tattoos don't overlap */
  for (list = GIMP_LIST (gimage->channels)->list; 
       list; 
       list = g_list_next (list))
    {
      Tattoo ctattoo;
      channel = (GimpChannel *) list->data;
      
      ctattoo = gimp_drawable_get_tattoo (GIMP_DRAWABLE (channel));
      if (ctattoo > maxval)
	maxval = ctattoo;
      /* Now check path an't got this tattoo */
      if (path_get_path_by_tattoo (gimage, ctattoo) != NULL)
	{
	  retval = FALSE; /* Oopps duplicated tattoo in layer */
	}
    }

  /* Find the max tatto value in the paths */
  plist = gimage->paths;
      
  if (plist && plist->bz_paths)
    {
      Tattoo  ptattoo;
      GSList *pl;

      for (pl = plist->bz_paths; pl; pl = g_slist_next (pl))
	{
	  pptr = pl->data;

	  ptattoo = path_get_tattoo (pptr);
	  
	  if (ptattoo > maxval)
	    maxval = ptattoo;
	}
    }

  if (val < maxval)
    retval = FALSE;
  /* Must check the state is valid */
  if (retval == TRUE)
    gimage->tattoo_state = val;

  return retval;
}

void
gimp_image_set_paths (GimpImage *gimage,
		      PathList  *paths)
{
  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  gimage->paths = paths;
}

PathList *
gimp_image_get_paths (const GimpImage *gimage)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  return gimage->paths;
}

void
gimp_image_colormap_changed (GimpImage *gimage, 
			     gint       col)
{
  g_return_if_fail (GIMP_IS_IMAGE (gimage));
  g_return_if_fail (col < gimage->num_cols);

  gtk_signal_emit (GTK_OBJECT (gimage),
		   gimp_image_signals[COLORMAP_CHANGED],
		   col);
}

void
gimp_image_mode_changed (GimpImage *gimage)
{
  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  gtk_signal_emit (GTK_OBJECT (gimage), gimp_image_signals[MODE_CHANGED]);
}

void
gimp_image_mask_changed (GimpImage *gimage)
{
  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  gtk_signal_emit (GTK_OBJECT (gimage), gimp_image_signals[MASK_CHANGED]);
}

void
gimp_image_alpha_changed (GimpImage *gimage)
{
  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  gtk_signal_emit (GTK_OBJECT (gimage), gimp_image_signals[ALPHA_CHANGED]);
}

void
gimp_image_floating_selection_changed (GimpImage *gimage)
{
  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  gtk_signal_emit (GTK_OBJECT (gimage),
		   gimp_image_signals[FLOATING_SELECTION_CHANGED]);
}

/************************************************************/
/*  Projection functions                                    */
/************************************************************/

static void
project_intensity (GimpImage   *gimage, 
		   GimpLayer   *layer,
		   PixelRegion *src, 
		   PixelRegion *dest, 
		   PixelRegion *mask)
{
  if (! gimage->construct_flag)
    initial_region (src, dest, mask, NULL, layer->opacity,
		    layer->mode, gimage->visible, INITIAL_INTENSITY);
  else
    combine_regions (dest, src, dest, mask, NULL, layer->opacity,
		     layer->mode, gimage->visible, COMBINE_INTEN_A_INTEN);
}

static void
project_intensity_alpha (GimpImage   *gimage, 
			 GimpLayer   *layer,
			 PixelRegion *src,
			 PixelRegion *dest,
			 PixelRegion *mask)
{
  if (! gimage->construct_flag)
    initial_region (src, dest, mask, NULL, layer->opacity,
		    layer->mode, gimage->visible, INITIAL_INTENSITY_ALPHA);
  else
    combine_regions (dest, src, dest, mask, NULL, layer->opacity,
		     layer->mode, gimage->visible, COMBINE_INTEN_A_INTEN_A);
}

static void
project_indexed (GimpImage   *gimage, 
		 GimpLayer   *layer,
		 PixelRegion *src, 
		 PixelRegion *dest)
{
  if (! gimage->construct_flag)
    initial_region (src, dest, NULL, gimage->cmap, layer->opacity,
		    layer->mode, gimage->visible, INITIAL_INDEXED);
  else
    g_message ("%s(): unable to project indexed image.", G_GNUC_FUNCTION);
}

static void
project_indexed_alpha (GimpImage   *gimage, 
		       GimpLayer   *layer,
		       PixelRegion *src, 
		       PixelRegion *dest,
		       PixelRegion *mask)
{
  if (! gimage->construct_flag)
    initial_region (src, dest, mask, gimage->cmap, layer->opacity,
		    layer->mode, gimage->visible, INITIAL_INDEXED_ALPHA);
  else
    combine_regions (dest, src, dest, mask, gimage->cmap, layer->opacity,
		     layer->mode, gimage->visible, COMBINE_INTEN_A_INDEXED_A);
}

static void
project_channel (GimpImage   *gimage, 
		 GimpChannel *channel,
		 PixelRegion *src, 
		 PixelRegion *src2)
{
  guchar  col[3];
  guchar  opacity;
  gint    type;

  gimp_rgba_get_uchar (&channel->color,
		       &col[0], &col[1], &col[2], &opacity);

  if (! gimage->construct_flag)
    {
      type = (channel->show_masked) ?
	INITIAL_CHANNEL_MASK : INITIAL_CHANNEL_SELECTION;

      initial_region (src2, src, NULL, col, opacity,
		      NORMAL_MODE, NULL, type);
    }
  else
    {
      type = (channel->show_masked) ?
	COMBINE_INTEN_A_CHANNEL_MASK : COMBINE_INTEN_A_CHANNEL_SELECTION;

      combine_regions (src, src2, src, NULL, col, opacity,
		       NORMAL_MODE, NULL, type);
    }
}

/************************************************************/
/*  Layer/Channel functions                                 */
/************************************************************/

static void
gimp_image_construct_layers (GimpImage *gimage, 
			     gint       x, 
			     gint       y, 
			     gint       w, 
			     gint       h)
{
  GimpLayer   *layer;
  gint         x1, y1, x2, y2;
  PixelRegion  src1PR, src2PR, maskPR;
  PixelRegion * mask;
  GList       *list;
  GSList      *reverse_list = NULL;
  gint         off_x;
  gint         off_y;

  /*  composite the floating selection if it exists  */
  if ((layer = gimp_image_floating_sel (gimage)))
    floating_sel_composite (layer, x, y, w, h, FALSE);

  /* Note added by Raph Levien, 27 Jan 1998

     This looks it was intended as an optimization, but it seems to
     have correctness problems. In particular, if all channels are
     turned off, the screen simply does not update the projected
     image. It should be black. Turning off this optimization seems to
     restore correct behavior. At some future point, it may be
     desirable to turn the optimization back on.

     */
#if 0
  /*  If all channels are not visible, simply return  */
  switch (gimp_image_base_type (gimage))
    {
    case RGB:
      if (! gimp_image_get_component_visible (gimage, RED_CHANNEL) &&
	  ! gimp_image_get_component_visible (gimage, GREEN_CHANNEL) &&
	  ! gimp_image_get_component_visible (gimage, BLUE_CHANNEL))
	return;
      break;
    case GRAY:
      if (! gimp_image_get_component_visible (gimage, GRAY_CHANNEL))
	return;
      break;
    case INDEXED:
      if (! gimp_image_get_component_visible (gimage, INDEXED_CHANNEL))
	return;
      break;
    }
#endif

  for (list = GIMP_LIST (gimage->layers)->list; 
       list; 
       list = g_list_next (list))
    {
      layer = (GimpLayer *) list->data;

      /*  only add layers that are visible and not floating selections 
	  to the list  */
      if (! gimp_layer_is_floating_sel (layer) && 
	  gimp_drawable_get_visible (GIMP_DRAWABLE (layer)))
	{
	  reverse_list = g_slist_prepend (reverse_list, layer);
	}
    }

  while (reverse_list)
    {
      layer = (GimpLayer *) reverse_list->data;
      gimp_drawable_offsets (GIMP_DRAWABLE (layer), &off_x, &off_y);

      x1 = CLAMP (off_x, x, x + w);
      y1 = CLAMP (off_y, y, y + h);
      x2 = CLAMP (off_x + gimp_drawable_width (GIMP_DRAWABLE (layer)), x, x + w);
      y2 = CLAMP (off_y + gimp_drawable_height (GIMP_DRAWABLE (layer)), y, y + h);

      /* configure the pixel regions  */
      pixel_region_init (&src1PR, gimp_image_projection (gimage), 
			 x1, y1, (x2 - x1), (y2 - y1), 
			 TRUE);

      /*  If we're showing the layer mask instead of the layer...  */
      if (layer->mask && layer->mask->show_mask)
	{
	  pixel_region_init (&src2PR, 
			     gimp_drawable_data (GIMP_DRAWABLE (layer->mask)),
			     (x1 - off_x), (y1 - off_y),
			     (x2 - x1), (y2 - y1), FALSE);

	  copy_gray_to_region (&src2PR, &src1PR);
	}
      /*  Otherwise, normal  */
      else
	{
	  pixel_region_init (&src2PR, 
			     gimp_drawable_data (GIMP_DRAWABLE (layer)),
			     (x1 - off_x), (y1 - off_y),
			     (x2 - x1), (y2 - y1), FALSE);

	  if (layer->mask && layer->mask->apply_mask)
	    {
	      pixel_region_init (&maskPR, 
				 gimp_drawable_data (GIMP_DRAWABLE (layer->mask)),
				 (x1 - off_x), (y1 - off_y),
				 (x2 - x1), (y2 - y1), FALSE);
	      mask = &maskPR;
	    }
	  else
	    mask = NULL;

	  /*  Based on the type of the layer, project the layer onto the
	   *  projection image...
	   */
	  switch (gimp_drawable_type (GIMP_DRAWABLE (layer)))
	    {
	    case RGB_GIMAGE: case GRAY_GIMAGE:
	      /* no mask possible */
	      project_intensity (gimage, layer, &src2PR, &src1PR, mask);
	      break;

	    case RGBA_GIMAGE: case GRAYA_GIMAGE:
	      project_intensity_alpha (gimage, layer, &src2PR, &src1PR, mask);
	      break;

	    case INDEXED_GIMAGE:
	      /* no mask possible */
	      project_indexed (gimage, layer, &src2PR, &src1PR);
	      break;

	    case INDEXEDA_GIMAGE:
	      project_indexed_alpha (gimage, layer, &src2PR, &src1PR, mask);
	      break;

	    default:
	      break;
	    }
	}
      gimage->construct_flag = 1;  /*  something was projected  */

      reverse_list = g_slist_next (reverse_list);
    }

  g_slist_free (reverse_list);
}

static void
gimp_image_construct_channels (GimpImage *gimage, 
			       gint       x, 
			       gint       y, 
			       gint       w, 
			       gint       h)
{
  GimpChannel *channel;
  PixelRegion  src1PR;
  PixelRegion  src2PR;
  GList       *list;
  GSList      *reverse_list = NULL;

  /*  reverse the channel list  */
  for (list = GIMP_LIST (gimage->channels)->list; 
       list; 
       list = g_list_next (list))
    reverse_list = g_slist_prepend (reverse_list, list->data);

  while (reverse_list)
    {
      channel = (GimpChannel *) reverse_list->data;

      if (gimp_drawable_get_visible (GIMP_DRAWABLE (channel)))
	{
	  /* configure the pixel regions  */
	  pixel_region_init (&src1PR,
			     gimp_image_projection (gimage), 
			     x, y, w, h, 
			     TRUE);
	  pixel_region_init (&src2PR,
			     gimp_drawable_data (GIMP_DRAWABLE (channel)), 
			     x, y, w, h, 
			     FALSE);

	  project_channel (gimage, channel, &src1PR, &src2PR);

	  gimage->construct_flag = 1;
	}

      reverse_list = g_slist_next (reverse_list);
    }

  g_slist_free (reverse_list);
}

static void
gimp_image_initialize_projection (GimpImage *gimage, 
				  gint       x, 
				  gint       y, 
				  gint       w, 
				  gint       h)
{

  GList       *list;
  GimpLayer   *layer;
  gint         coverage = 0;
  PixelRegion  PR;
  guchar       clear[4] = { 0, 0, 0, 0 };

  /*  this function determines whether a visible layer
   *  provides complete coverage over the image.  If not,
   *  the projection is initialized to transparent
   */
  
  for (list = GIMP_LIST (gimage->layers)->list; 
       list; 
       list = g_list_next (list))
    {
      gint off_x, off_y;

      layer = (GimpLayer *) list->data;
      gimp_drawable_offsets (GIMP_DRAWABLE (layer), &off_x, &off_y);

      if (gimp_drawable_get_visible (GIMP_DRAWABLE (layer)) &&
	  ! gimp_layer_has_alpha (layer) &&
	  (off_x <= x) &&
	  (off_y <= y) &&
	  (off_x + gimp_drawable_width (GIMP_DRAWABLE (layer)) >= x + w) &&
	  (off_y + gimp_drawable_height (GIMP_DRAWABLE (layer)) >= y + h))
	{
	  coverage = 1;
	  break;
	}
    }

  if (!coverage)
    {
      pixel_region_init (&PR, gimp_image_projection (gimage), 
			 x, y, w, h, TRUE);
      color_region (&PR, clear);
    }
}

static void
gimp_image_get_active_channels (GimpImage    *gimage, 
				GimpDrawable *drawable, 
				gint         *active)
{
  GimpLayer *layer;
  gint       i;

  /*  first, blindly copy the gimage active channels  */
  for (i = 0; i < MAX_CHANNELS; i++)
    active[i] = gimage->active[i];

  /*  If the drawable is a channel (a saved selection, etc.)
   *  make sure that the alpha channel is not valid
   */
  if (GIMP_IS_CHANNEL (drawable))
    active[ALPHA_G_PIX] = 0;  /*  no alpha values in channels  */
  else
    {
      /*  otherwise, check whether preserve transparency is
       *  enabled in the layer and if the layer has alpha
       */
      if (GIMP_IS_LAYER (drawable))
	{
	  layer = GIMP_LAYER (drawable);
	  if (gimp_layer_has_alpha (layer) && layer->preserve_trans)
	    active[gimp_drawable_bytes (drawable) - 1] = 0;
	}
    }
}

void
gimp_image_construct (GimpImage *gimage, 
		      gint       x,
		      gint       y,
		      gint       w,
		      gint       h)
{
  g_return_if_fail (GIMP_IS_IMAGE (gimage));

#if 0
  gint xoff;
  gint yoff;
  
  /*  set the construct flag, used to determine if anything
   *  has been written to the gimage raw image yet.
   */
  gimage->construct_flag = 0;

  if (gimage->layers)
    {
      gimp_drawable_offsets (GIMP_DRAWABLE ((GimpLayer*) gimage->layers->data),
			     &xoff, &yoff);
    }

  if ((gimage->layers) &&                         /* There's a layer.      */
      (! g_slist_next (gimage->layers)) &&        /* It's the only layer.  */
      (gimp_layer_has_alpha ((GimpLayer *) (gimage->layers->data))) && /* It's !flat.  */
                                                  /* It's visible.         */
      (gimp_drawable_get_visible (GIMP_DRAWABLE (gimage->layers->data))) &&
      (gimp_drawable_width (GIMP_DRAWABLE (gimage->layers->data)) ==
       gimage->width) &&
      (gimp_drawable_height (GIMP_DRAWABLE (gimage->layers->data)) ==
       gimage->height) &&                         /* Covers all.           */
                                                  /* Not indexed.          */
      (!gimp_drawable_is_indexed (GIMP_DRAWABLE (gimage->layers->data))) &&
      (((GimpLayer *)(gimage->layers->data))->opacity == OPAQUE_OPACITY) /*opaq */
      )
    {
      gint xoff;
      gint yoff;
      
      gimp_drawable_offsets (GIMP_DRAWABLE (gimage->layers->data),
			     &xoff, &yoff);

      if ((xoff==0) && (yoff==0)) /* Starts at 0,0         */
	{
	  PixelRegion srcPR, destPR;
	  gpointer    pr;
	
	  g_warning("Can use cow-projection hack.  Yay!");

	  pixel_region_init (&srcPR, gimp_drawable_data
			     (GIMP_DRAWABLE (gimage->layers->data)),
			     x, y, w,h, FALSE);
	  pixel_region_init (&destPR,
			     gimp_image_projection (gimage),
			     x, y, w,h, TRUE);

	  for (pr = pixel_regions_register (2, &srcPR, &destPR);
	       pr != NULL;
	       pr = pixel_regions_process (pr))
	    {
	      tile_manager_map_over_tile (destPR.tiles,
					  destPR.curtile, srcPR.curtile);
	    }

	  gimage->construct_flag = 1;
	  gimp_image_construct_channels (gimage, x, y, w, h);

	  return;
	}
    }
#else
  gimage->construct_flag = 0;
#endif
  
  /*  First, determine if the projection image needs to be
   *  initialized--this is the case when there are no visible
   *  layers that cover the entire canvas--either because layers
   *  are offset or only a floating selection is visible
   */
  gimp_image_initialize_projection (gimage, x, y, w, h);
  
  /*  call functions which process the list of layers and
   *  the list of channels
   */
  gimp_image_construct_layers (gimage, x, y, w, h);
  gimp_image_construct_channels (gimage, x, y, w, h);
}

void
gimp_image_invalidate_without_render (GimpImage *gimage, 
				      gint       x,
				      gint       y,
				      gint       w,
				      gint       h,
				      gint       x1,
				      gint       y1,
				      gint       x2,
				      gint       y2)
{
  Tile        *tile;
  TileManager *tm;
  gint         i, j;

  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  tm = gimp_image_projection (gimage);

  /*  invalidate all tiles which are located outside of the displayed area
   *   all tiles inside the displayed area are constructed.
   */
  for (i = y; i < (y + h); i += (TILE_HEIGHT - (i % TILE_HEIGHT)))
    for (j = x; j < (x + w); j += (TILE_WIDTH - (j % TILE_WIDTH)))
      {
	tile = tile_manager_get_tile (tm, j, i, FALSE, FALSE);

        /*  check if the tile is outside the bounds  */
        if ((MIN ((j + tile_ewidth(tile)), x2) - MAX (j, x1)) <= 0)
          {
            tile_invalidate_tile (&tile, tm, j, i);
          }
        else if (MIN ((i + tile_eheight(tile)), y2) - MAX (i, y1) <= 0)
          {
            tile_invalidate_tile (&tile, tm, j, i);
          }
      }
}

void
gimp_image_invalidate (GimpImage *gimage, 
		       gint       x,
		       gint       y,
		       gint       w,
		       gint       h,
		       gint       x1,
		       gint       y1,
		       gint       x2,
		       gint       y2)
{
  Tile        *tile;
  TileManager *tm;
  gint         i, j;
  gint         startx, starty;
  gint         endx, endy;
  gint         tilex, tiley;

  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  tm = gimp_image_projection (gimage);

  startx = x;
  starty = y;
  endx   = x + w;
  endy   = y + h;

  /*  invalidate all tiles which are located outside of the displayed area
   *   all tiles inside the displayed area are constructed.
   */
  for (i = y; i < (y + h); i += (TILE_HEIGHT - (i % TILE_HEIGHT)))
    for (j = x; j < (x + w); j += (TILE_WIDTH - (j % TILE_WIDTH)))
      {
	tile = tile_manager_get_tile (tm, j, i, FALSE, FALSE);

        /*  check if the tile is outside the bounds  */
        if ((MIN ((j + tile_ewidth(tile)), x2) - MAX (j, x1)) <= 0)
          {
            tile_invalidate_tile (&tile, tm, j, i);
            if (j < x1)
              startx = MAX (startx, (j + tile_ewidth(tile)));
            else
              endx = MIN (endx, j);
          }
        else if (MIN ((i + tile_eheight(tile)), y2) - MAX (i, y1) <= 0)
          {
            tile_invalidate_tile (&tile, tm, j, i);
            if (i < y1)
              starty = MAX (starty, (i + tile_eheight(tile)));
            else
              endy = MIN (endy, i);
          }
        else
          {
            /*  If the tile is not valid, make sure we get the entire tile
             *   in the construction extents
             */
            if (tile_is_valid (tile) == FALSE)
              {
                tilex = j - (j % TILE_WIDTH);
                tiley = i - (i % TILE_HEIGHT);
                
                startx = MIN (startx, tilex);
                endx   = MAX (endx, tilex + tile_ewidth (tile));
                starty = MIN (starty, tiley);
                endy   = MAX (endy, tiley + tile_eheight (tile));
                
                tile_mark_valid (tile); /* hmmmmmmm..... */
              }
          }
      }

  if ((endx - startx) > 0 && (endy - starty) > 0)
    gimp_image_construct (gimage, 
			  startx, starty, 
			  (endx - startx), (endy - starty));
}

void
gimp_image_validate (TileManager *tm, 
		     Tile        *tile)
{
  GimpImage *gimage;
  gint       x, y;
  gint       w, h;

  gimp_set_busy_until_idle ();

  /*  Get the gimage from the tilemanager  */
  gimage = (GimpImage *) tile_manager_get_user_data (tm);

  /*  Find the coordinates of this tile  */
  tile_manager_get_tile_coordinates (tm, tile, &x, &y);
  w = tile_ewidth  (tile);
  h = tile_eheight (tile);
  
  gimp_image_construct (gimage, x, y, w, h);
}

void
gimp_image_invalidate_layer_previews (GimpImage *gimage)
{
  g_return_if_fail (gimage != NULL);
  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  gimp_container_foreach (gimage->layers, 
			  (GFunc) gimp_viewable_invalidate_preview, 
			  NULL);
}

void
gimp_image_invalidate_channel_previews (GimpImage *gimage)
{
  g_return_if_fail (gimage != NULL);
  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  gimp_container_foreach (gimage->channels, 
			  (GFunc) gimp_viewable_invalidate_preview, 
			  NULL);
}

GimpContainer *
gimp_image_get_layers (const GimpImage *gimage)
{
  g_return_val_if_fail (gimage != NULL, NULL);
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  return gimage->layers;
}

GimpContainer *
gimp_image_get_channels (const GimpImage *gimage)
{
  g_return_val_if_fail (gimage != NULL, NULL);
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  return gimage->channels;
}

gint            
gimp_image_get_layer_index (const GimpImage   *gimage,
			    const GimpLayer   *layer)
{
  g_return_val_if_fail (gimage != NULL, -1);
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), -1);

  g_return_val_if_fail (layer != NULL, -1);
  g_return_val_if_fail (GIMP_IS_LAYER (layer), -1);

  return gimp_container_get_child_index (gimage->layers, 
					 GIMP_OBJECT (layer));
}

gint            
gimp_image_get_channel_index (const GimpImage   *gimage,
			      const GimpChannel *channel)
{
  g_return_val_if_fail (gimage != NULL, -1);
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), -1);

  g_return_val_if_fail (channel != NULL, -1);
  g_return_val_if_fail (GIMP_IS_CHANNEL (channel), -1);

  return gimp_container_get_child_index (gimage->channels, 
					 GIMP_OBJECT (channel));
}

GimpLayer *
gimp_image_get_active_layer (const GimpImage *gimage)
{
  g_return_val_if_fail (gimage != NULL, NULL);
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  return gimage->active_layer;
}

GimpChannel *
gimp_image_get_active_channel (const GimpImage *gimage)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  return gimage->active_channel;
}

GimpLayer *
gimp_image_get_layer_by_tattoo (const GimpImage *gimage, 
				Tattoo           tattoo)
{
  GimpLayer *layer;
  GList     *list;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  for (list = GIMP_LIST (gimage->layers)->list; 
       list; 
       list = g_list_next (list))
    {
      layer = (GimpLayer *) list->data;

      if (gimp_drawable_get_tattoo (GIMP_DRAWABLE (layer)) == tattoo)
	return layer;
    }

  return NULL;
}

GimpChannel *
gimp_image_get_channel_by_tattoo (const GimpImage *gimage, 
				  Tattoo           tattoo)
{
  GimpChannel *channel;
  GList       *list;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  for (list = GIMP_LIST (gimage->channels)->list; 
       list; 
       list = g_list_next (list))
    {
      channel = (GimpChannel *) list->data;

      if (gimp_drawable_get_tattoo (GIMP_DRAWABLE (channel)) == tattoo)
	return channel;
    }

  return NULL;
}

GimpChannel *
gimp_image_get_channel_by_name (const GimpImage *gimage,
				const gchar     *name)
{
  GimpChannel *channel;
  GList       *list;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  for (list = GIMP_LIST (gimage->channels)->list; 
       list; 
       list = g_list_next (list))
    {
      channel = (GimpChannel *) list->data;
      if (! strcmp (gimp_object_get_name (GIMP_OBJECT (channel)), name))
      return channel;
    }

  return NULL;
}

GimpChannel *
gimp_image_get_mask (const GimpImage *gimage)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  return gimage->selection_mask;
}

void
gimp_image_set_component_active (GimpImage   *gimage, 
				 ChannelType  type, 
				 gboolean     active)
{
  gint pixel = -1;

  g_return_if_fail (gimage != NULL);
  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  switch (type)
    {
    case RED_CHANNEL:     pixel = RED_PIX;     break;
    case GREEN_CHANNEL:   pixel = GREEN_PIX;   break;
    case BLUE_CHANNEL:    pixel = BLUE_PIX;    break;
    case GRAY_CHANNEL:    pixel = GRAY_PIX;    break;
    case INDEXED_CHANNEL: pixel = INDEXED_PIX; break;
    case ALPHA_CHANNEL:
      switch (gimp_image_base_type (gimage))
	{
	case RGB:     pixel = ALPHA_PIX;   break;
	case GRAY:    pixel = ALPHA_G_PIX; break;
	case INDEXED: pixel = ALPHA_I_PIX; break;
	}
      break;

    default:
      break;
    }

  if (pixel != -1 && active != gimage->active[pixel])
    {
      gimage->active[pixel] = active ? TRUE : FALSE;

      /*  If there is an active channel and we mess with the components,
       *  the active channel gets unset...
       */
      gimp_image_unset_active_channel (gimage);

      gtk_signal_emit (GTK_OBJECT (gimage),
		       gimp_image_signals[COMPONENT_ACTIVE_CHANGED],
		       type);
    }
}

gboolean
gimp_image_get_component_active (const GimpImage *gimage, 
				 ChannelType      type)
{
  /*  No sanity checking here...  */
  switch (type)
    {
    case RED_CHANNEL:     return gimage->active[RED_PIX];     break;
    case GREEN_CHANNEL:   return gimage->active[GREEN_PIX];   break;
    case BLUE_CHANNEL:    return gimage->active[BLUE_PIX];    break;
    case GRAY_CHANNEL:    return gimage->active[GRAY_PIX];    break;
    case INDEXED_CHANNEL: return gimage->active[INDEXED_PIX]; break;
    case ALPHA_CHANNEL:
      switch (gimp_image_base_type (gimage))
	{
	case RGB:     return gimage->active[ALPHA_PIX];   break;
	case GRAY:    return gimage->active[ALPHA_G_PIX]; break;
	case INDEXED: return gimage->active[ALPHA_I_PIX]; break;
	}
      break;

    default:
      break;
    }

  return FALSE;
}

void
gimp_image_set_component_visible (GimpImage   *gimage, 
				  ChannelType  type, 
				  gboolean     visible)
{
  gint pixel = -1;

  g_return_if_fail (gimage != NULL);
  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  switch (type)
    {
    case RED_CHANNEL:     pixel = RED_PIX;     break;
    case GREEN_CHANNEL:   pixel = GREEN_PIX;   break;
    case BLUE_CHANNEL:    pixel = BLUE_PIX;    break;
    case GRAY_CHANNEL:    pixel = GRAY_PIX;    break;
    case INDEXED_CHANNEL: pixel = INDEXED_PIX; break;
    case ALPHA_CHANNEL:
      switch (gimp_image_base_type (gimage))
	{
	case RGB:     pixel = ALPHA_PIX;   break;
	case GRAY:    pixel = ALPHA_G_PIX; break;
	case INDEXED: pixel = ALPHA_I_PIX; break;
	}
      break;

    default:
      break;
    }

  if (pixel != -1 && visible != gimage->visible[pixel])
    {
      gimage->visible[pixel] = visible ? TRUE : FALSE;

      gtk_signal_emit (GTK_OBJECT (gimage),
		       gimp_image_signals[COMPONENT_VISIBILITY_CHANGED],
		       type);
    }
}

gboolean
gimp_image_get_component_visible (const GimpImage *gimage, 
				  ChannelType      type)
{
  /*  No sanity checking here...  */
  switch (type)
    {
    case RED_CHANNEL:     return gimage->visible[RED_PIX];     break;
    case GREEN_CHANNEL:   return gimage->visible[GREEN_PIX];   break;
    case BLUE_CHANNEL:    return gimage->visible[BLUE_PIX];    break;
    case GRAY_CHANNEL:    return gimage->visible[GRAY_PIX];    break;
    case INDEXED_CHANNEL: return gimage->visible[INDEXED_PIX]; break;
    case ALPHA_CHANNEL:
      switch (gimp_image_base_type (gimage))
	{
	case RGB:     return gimage->visible[ALPHA_PIX];   break;
	case GRAY:    return gimage->visible[ALPHA_G_PIX]; break;
	case INDEXED: return gimage->visible[ALPHA_I_PIX]; break;
	}
      break;

    default:
      break;
    }

  return FALSE;
}

gboolean
gimp_image_layer_boundary (const GimpImage  *gimage, 
			   BoundSeg        **segs, 
			   gint             *n_segs)
{
  GimpLayer *layer;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (segs != NULL, FALSE);
  g_return_val_if_fail (n_segs != NULL, FALSE);

  /*  The second boundary corresponds to the active layer's
   *  perimeter...
   */
  layer = gimp_image_get_active_layer (gimage);

  if (layer)
    {
      *segs = gimp_layer_boundary (layer, n_segs);
      return TRUE;
    }
  else
    {
      *segs = NULL;
      *n_segs = 0;
      return FALSE;
    }
}

GimpLayer *
gimp_image_set_active_layer (GimpImage *gimage, 
			     GimpLayer *layer)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);
  g_return_val_if_fail (GIMP_IS_LAYER (layer), NULL);

  /*  First, find the layer in the gimage
   *  If it isn't valid, find the first layer that is
   */
  if (! gimp_container_have (gimage->layers, GIMP_OBJECT (layer)))
    layer = (GimpLayer *) gimp_container_get_child_by_index (gimage->layers, 0);

  if (layer)
    {
      /*  Configure the layer stack to reflect this change  */
      gimage->layer_stack = g_slist_remove (gimage->layer_stack, layer);
      gimage->layer_stack = g_slist_prepend (gimage->layer_stack, layer);

      /*  invalidate the selection boundary because of a layer modification  */
      gimp_layer_invalidate_boundary (layer);
    }

  if (layer != gimage->active_layer)
    {
      gimage->active_layer = layer;

      gtk_signal_emit (GTK_OBJECT (gimage),
		       gimp_image_signals[ACTIVE_LAYER_CHANGED]);

      if (gimage->active_channel)
	{
	  gimage->active_channel = NULL;

	  gtk_signal_emit (GTK_OBJECT (gimage),
			   gimp_image_signals[ACTIVE_CHANNEL_CHANGED]);
	}
    }

  /*  return the layer  */
  return layer;
}

GimpChannel *
gimp_image_set_active_channel (GimpImage   *gimage, 
			       GimpChannel *channel)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  /*  Not if there is a floating selection  */
  if (gimp_image_floating_sel (gimage))
    return NULL;

  /*  First, find the channel
   *  If it doesn't exist, find the first channel that does
   */
  if (! gimp_container_have (gimage->channels, GIMP_OBJECT (channel)))
    channel = (GimpChannel *) gimp_container_get_child_by_index (gimage->channels, 0);

  if (channel != gimage->active_channel)
    {
      gimage->active_channel = channel;

      gtk_signal_emit (GTK_OBJECT (gimage),
		       gimp_image_signals[ACTIVE_CHANNEL_CHANGED]);

      if (gimage->active_layer)
	{
	  gimage->active_layer = NULL;

	  gtk_signal_emit (GTK_OBJECT (gimage),
			   gimp_image_signals[ACTIVE_LAYER_CHANGED]);
	}
    }

  /*  return the channel  */
  return channel;
}

GimpChannel *
gimp_image_unset_active_channel (GimpImage *gimage)
{
  GimpChannel *channel;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  channel = gimp_image_get_active_channel (gimage);

  if (channel)
    {
      gimage->active_channel = NULL;

      gtk_signal_emit (GTK_OBJECT (gimage),
		       gimp_image_signals[ACTIVE_CHANNEL_CHANGED]);

      if (gimage->layer_stack)
	{
	  GimpLayer *layer;

	  layer = (GimpLayer *) gimage->layer_stack->data;

	  gimp_image_set_active_layer (gimage, layer);
	}
    }

  return channel;
}

GimpLayer *
gimp_image_pick_correlate_layer (const GimpImage *gimage, 
				 gint             x, 
				 gint             y)
{
  GimpLayer *layer;
  GList     *list;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  for (list = GIMP_LIST (gimage->layers)->list; 
       list; 
       list = g_list_next (list))
    {
      layer = (GimpLayer *) list->data;

      if (gimp_layer_pick_correlate (layer, x, y))
	return layer;
    }

  return NULL;
}

gboolean
gimp_image_raise_layer (GimpImage *gimage, 
			GimpLayer *layer)
{
  gint curpos;
  
  g_return_val_if_fail (gimage != NULL, FALSE);
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  
  g_return_val_if_fail (layer != NULL, FALSE);
  g_return_val_if_fail (GIMP_IS_LAYER (layer), FALSE);

  curpos = gimp_container_get_child_index (gimage->layers, 
					   GIMP_OBJECT (layer));

  /* is this the top layer already? */
  if (curpos == 0)
    {
      g_message (_("%s(): layer cannot be raised any further"),
		 G_GNUC_FUNCTION);
      return FALSE;
    }
  
  return gimp_image_position_layer (gimage, layer, curpos - 1, TRUE);
}

gboolean
gimp_image_lower_layer (GimpImage *gimage, 
			GimpLayer *layer)
{
  gint curpos;
  gint length;
  
  g_return_val_if_fail (gimage != NULL, FALSE);
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  
  g_return_val_if_fail (layer != NULL, FALSE);
  g_return_val_if_fail (GIMP_IS_LAYER (layer), FALSE);

  curpos = gimp_container_get_child_index (gimage->layers, 
					   GIMP_OBJECT (layer));
  
  /* is this the bottom layer already? */
  length = gimp_container_num_children (gimage->layers);
  if (curpos >= length - 1)
    {
      g_message (_("%s(): layer cannot be lowered any further"),
		 G_GNUC_FUNCTION);
      return FALSE;
    }
  
  return gimp_image_position_layer (gimage, layer, curpos + 1, TRUE);
}

gboolean
gimp_image_raise_layer_to_top (GimpImage *gimage, 
			       GimpLayer *layer)
{
  gint curpos;
  
  g_return_val_if_fail (gimage != NULL, FALSE);
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  
  g_return_val_if_fail (layer != NULL, FALSE);
  g_return_val_if_fail (GIMP_IS_LAYER (layer), FALSE);

  curpos = gimp_container_get_child_index (gimage->layers, 
					   GIMP_OBJECT (layer));
  
  if (curpos == 0)
    {
      g_message (_("%s(): layer is already on top"),
		 G_GNUC_FUNCTION);
      return FALSE;
    }
  
  if (! gimp_layer_has_alpha (layer))
    {
      g_message (_("%s(): can't raise Layer without alpha"), G_GNUC_FUNCTION);
      return FALSE;
    }
  
  return gimp_image_position_layer (gimage, layer, 0, TRUE);
}

gboolean
gimp_image_lower_layer_to_bottom (GimpImage *gimage, 
				  GimpLayer *layer)
{
  gint curpos;
  gint length;
  
  g_return_val_if_fail (gimage != NULL, FALSE);
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  
  g_return_val_if_fail (layer != NULL, FALSE);
  g_return_val_if_fail (GIMP_IS_LAYER (layer), FALSE);

  curpos = gimp_container_get_child_index (gimage->layers, 
					   GIMP_OBJECT (layer));
 
  length = gimp_container_num_children (gimage->layers);

  if (curpos >= length - 1)
    {
      g_message (_("%s(): layer is already on bottom"), G_GNUC_FUNCTION);
      return FALSE;
    }
  
  return gimp_image_position_layer (gimage, layer, length - 1, TRUE);
}

gboolean
gimp_image_position_layer (GimpImage *gimage, 
			   GimpLayer *layer,
			   gint       new_index,
			   gboolean   push_undo)
{
  gint x_min, y_min, x_max, y_max;
  gint off_x, off_y;
  gint index;
  gint num_layers;

  g_return_val_if_fail (gimage != NULL, FALSE);
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  
  g_return_val_if_fail (layer != NULL, FALSE);
  g_return_val_if_fail (GIMP_IS_LAYER (layer), FALSE);

  index = gimp_container_get_child_index (gimage->layers, 
					  GIMP_OBJECT (layer));
  if (index < 0)
    return FALSE;

  num_layers = gimp_container_num_children (gimage->layers);

  if (new_index < 0)
    new_index = 0;

  if (new_index >= num_layers)
    new_index = num_layers - 1;

  if (new_index == index)
    return TRUE;

  /* check if we want to move it below a bottom layer without alpha */
  if (new_index == num_layers - 1)
    {
      GimpLayer *tmp;
      
      tmp = (GimpLayer *) gimp_container_get_child_by_index (gimage->layers, 
							     num_layers - 1);

      if (new_index == num_layers - 1 &&
	  ! gimp_layer_has_alpha (tmp))
	{
	  g_message (_("BG has no alpha, layer was placed above"));
	  new_index--;
	}
    }

  if (push_undo)
    undo_push_layer_reposition (gimage, layer);

  gimp_container_reorder (gimage->layers, GIMP_OBJECT (layer), new_index);

  gimp_drawable_offsets (GIMP_DRAWABLE (layer), &off_x, &off_y);
  x_min = off_x;
  y_min = off_y;
  x_max = off_x + gimp_drawable_width (GIMP_DRAWABLE (layer));
  y_max = off_y + gimp_drawable_height (GIMP_DRAWABLE (layer));
  gtk_signal_emit (GTK_OBJECT (gimage),
		   gimp_image_signals[REPAINT],
		   x_min, y_min, x_max, y_max);

  /*  invalidate the composite preview  */
  gimp_viewable_invalidate_preview (GIMP_VIEWABLE (gimage));

  return TRUE;
}

GimpLayer *
gimp_image_merge_visible_layers (GimpImage *gimage, 
				 MergeType  merge_type)
{
  GList     *list;
  GSList    *merge_list       = NULL;
  gboolean   had_floating_sel = FALSE;
  GimpLayer *layer            = NULL;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  /* if there's a floating selection, anchor it */
  if (gimp_image_floating_sel (gimage))
    {
      floating_sel_anchor (gimage->floating_sel);
      had_floating_sel = TRUE;
    }

  for (list = GIMP_LIST (gimage->layers)->list;
       list;
       list = g_list_next (list))
    {
      layer = (GimpLayer *) list->data;

      if (gimp_drawable_get_visible (GIMP_DRAWABLE (layer)))
	merge_list = g_slist_append (merge_list, layer);
    }

  if (merge_list && merge_list->next)
    {
      gimp_set_busy ();

      layer = gimp_image_merge_layers (gimage, merge_list, merge_type);
      g_slist_free (merge_list);

      gimp_unset_busy ();

      return layer;
    }
  else
    {
      g_slist_free (merge_list);

      /* If there was a floating selection, we have done something.
	 No need to warn the user. Return the active layer instead */
      if (had_floating_sel)
	return layer;
      else
	g_message (_("There are not enough visible layers for a merge.\nThere must be at least two."));

      return NULL;
    }
}

GimpLayer *
gimp_image_flatten (GimpImage *gimage)
{
  GList     *list;
  GSList    *merge_list = NULL;
  GimpLayer *layer;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  gimp_set_busy ();

  /* if there's a floating selection, anchor it */
  if (gimp_image_floating_sel (gimage))
    floating_sel_anchor (gimage->floating_sel);

  for (list = GIMP_LIST (gimage->layers)->list;
       list;
       list = g_list_next (list))
    {
      layer = (GimpLayer *) list->data;

      if (gimp_drawable_get_visible (GIMP_DRAWABLE (layer)))
	merge_list = g_slist_append (merge_list, layer);
    }

  layer = gimp_image_merge_layers (gimage, merge_list, FLATTEN_IMAGE);
  g_slist_free (merge_list);

  gimp_image_alpha_changed (gimage);

  gimp_unset_busy ();

  return layer;
}

GimpLayer *
gimp_image_merge_down (GimpImage *gimage,
		       GimpLayer *current_layer,
		       MergeType  merge_type)
{
  GimpLayer *layer;
  GList     *list;
  GList     *layer_list;
  GSList    *merge_list;
  
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  for (list = GIMP_LIST (gimage->layers)->list, layer_list = NULL;
       list && !layer_list;
       list = g_list_next (list))
    {
      layer = (GimpLayer *) list->data;
      
      if (layer == current_layer)
	break;
    }
    
  for (layer_list = g_list_next (list), merge_list = NULL; 
       layer_list && !merge_list; 
       layer_list = g_list_next (layer_list))
    {
      layer = (GimpLayer *) layer_list->data;
      
      if (gimp_drawable_get_visible (GIMP_DRAWABLE (layer)))
	merge_list = g_slist_append (NULL, layer);
    }

  if (merge_list)
    {
      merge_list = g_slist_prepend (merge_list, current_layer);

      gimp_set_busy ();

      layer = gimp_image_merge_layers (gimage, merge_list, merge_type);
      g_slist_free (merge_list);

      gimp_unset_busy ();

      return layer;
    }
  else 
    {
      g_message (_("There are not enough visible layers for a merge down."));
      return NULL;
    }
}

GimpLayer *
gimp_image_merge_layers (GimpImage *gimage, 
			 GSList    *merge_list, 
			 MergeType  merge_type)
{
  GList            *list;
  GSList           *reverse_list = NULL;
  PixelRegion       src1PR, src2PR, maskPR;
  PixelRegion      *mask;
  GimpLayer        *merge_layer;
  GimpLayer        *layer;
  GimpLayer        *bottom_layer;
  LayerModeEffects  bottom_mode;
  guchar            bg[4] = {0, 0, 0, 0};
  GimpImageType     type;
  gint              count;
  gint              x1, y1, x2, y2;
  gint              x3, y3, x4, y4;
  gint              operation;
  gint              position;
  gint              active[MAX_CHANNELS] = {1, 1, 1, 1};
  gint              off_x, off_y;
  gchar            *name;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  layer        = NULL;
  type         = RGBA_GIMAGE;
  x1 = y1      = 0;
  x2 = y2      = 0;
  bottom_layer = NULL;
  bottom_mode  = NORMAL_MODE;

  /*  Get the layer extents  */
  count = 0;
  while (merge_list)
    {
      layer = (GimpLayer *) merge_list->data;
      gimp_drawable_offsets (GIMP_DRAWABLE (layer), &off_x, &off_y);

      switch (merge_type)
	{
	case EXPAND_AS_NECESSARY:
	case CLIP_TO_IMAGE:
	  if (!count)
	    {
	      x1 = off_x;
	      y1 = off_y;
	      x2 = off_x + gimp_drawable_width (GIMP_DRAWABLE (layer));
	      y2 = off_y + gimp_drawable_height (GIMP_DRAWABLE (layer));
	    }
	  else
	    {
	      if (off_x < x1)
		x1 = off_x;
	      if (off_y < y1)
		y1 = off_y;
	      if ((off_x + gimp_drawable_width (GIMP_DRAWABLE (layer))) > x2)
		x2 = (off_x + gimp_drawable_width (GIMP_DRAWABLE (layer)));
	      if ((off_y + gimp_drawable_height (GIMP_DRAWABLE (layer))) > y2)
		y2 = (off_y + gimp_drawable_height (GIMP_DRAWABLE (layer)));
	    }
	  if (merge_type == CLIP_TO_IMAGE)
	    {
	      x1 = CLAMP (x1, 0, gimage->width);
	      y1 = CLAMP (y1, 0, gimage->height);
	      x2 = CLAMP (x2, 0, gimage->width);
	      y2 = CLAMP (y2, 0, gimage->height);
	    }
	  break;

	case CLIP_TO_BOTTOM_LAYER:
	  if (merge_list->next == NULL)
	    {
	      x1 = off_x;
	      y1 = off_y;
	      x2 = off_x + gimp_drawable_width (GIMP_DRAWABLE (layer));
	      y2 = off_y + gimp_drawable_height (GIMP_DRAWABLE (layer));
	    }
	  break;

	case FLATTEN_IMAGE:
	  if (merge_list->next == NULL)
	    {
	      x1 = 0;
	      y1 = 0;
	      x2 = gimage->width;
	      y2 = gimage->height;
	    }
	  break;
	}

      count ++;
      reverse_list = g_slist_prepend (reverse_list, layer);
      merge_list = g_slist_next (merge_list);
    }

  if ((x2 - x1) == 0 || (y2 - y1) == 0)
    return NULL;

  /*  Start a merge undo group  */
  undo_push_group_start (gimage, LAYER_MERGE_UNDO);

  name = g_strdup (gimp_object_get_name (GIMP_OBJECT (layer)));

  if (merge_type == FLATTEN_IMAGE ||
      gimp_drawable_type (GIMP_DRAWABLE (layer)) == INDEXED_GIMAGE)
    {
      switch (gimp_image_base_type (gimage))
	{
	case RGB: type = RGB_GIMAGE; break;
	case GRAY: type = GRAY_GIMAGE; break;
	case INDEXED: type = INDEXED_GIMAGE; break;
	}

      merge_layer = gimp_layer_new (gimage, (x2 - x1), (y2 - y1),
				    type,
				    gimp_object_get_name (GIMP_OBJECT (layer)),
				    OPAQUE_OPACITY, NORMAL_MODE);
      if (!merge_layer)
	{
	  g_message ("gimp_image_merge_layers: could not allocate merge layer");
	  return NULL;
	}

      GIMP_DRAWABLE (merge_layer)->offset_x = x1;
      GIMP_DRAWABLE (merge_layer)->offset_y = y1;

      /*  get the background for compositing  */
      gimp_image_get_background (gimage, GIMP_DRAWABLE (merge_layer), bg);

      /*  init the pixel region  */
      pixel_region_init (&src1PR, 
			 gimp_drawable_data (GIMP_DRAWABLE (merge_layer)), 
			 0, 0, 
			 gimage->width, gimage->height, 
			 TRUE);

      /*  set the region to the background color  */
      color_region (&src1PR, bg);

      position = 0;
    }
  else
    {
      /*  The final merged layer inherits the name of the bottom most layer
       *  and the resulting layer has an alpha channel
       *  whether or not the original did
       *  Opacity is set to 100% and the MODE is set to normal
       */

      merge_layer =
	gimp_layer_new (gimage, (x2 - x1), (y2 - y1),
			gimp_drawable_type_with_alpha (GIMP_DRAWABLE (layer)),
			"merged layer",
			OPAQUE_OPACITY, NORMAL_MODE);

      if (!merge_layer)
	{
	  g_message ("gimp_image_merge_layers: could not allocate merge layer");
	  return NULL;
	}

      GIMP_DRAWABLE (merge_layer)->offset_x = x1;
      GIMP_DRAWABLE (merge_layer)->offset_y = y1;

      /*  Set the layer to transparent  */
      pixel_region_init (&src1PR, 
			 gimp_drawable_data (GIMP_DRAWABLE (merge_layer)), 
			 0, 0, 
			 (x2 - x1), (y2 - y1), 
			 TRUE);

      /*  set the region to 0's  */
      color_region (&src1PR, bg);

      /*  Find the index in the layer list of the bottom layer--we need this
       *  in order to add the final, merged layer to the layer list correctly
       */
      layer = (GimpLayer *) reverse_list->data;
      position = 
	gimp_container_num_children (gimage->layers) - 
	gimp_container_get_child_index (gimage->layers, GIMP_OBJECT (layer));
      
      /* set the mode of the bottom layer to normal so that the contents
       *  aren't lost when merging with the all-alpha merge_layer
       *  Keep a pointer to it so that we can set the mode right after it's
       *  been merged so that undo works correctly.
       */
      bottom_layer = layer;
      bottom_mode  = bottom_layer->mode;

      /* DISSOLVE_MODE is special since it is the only mode that does not
       *  work on the projection with the lower layer, but only locally on
       *  the layers alpha channel. 
       */
      if (bottom_layer->mode != DISSOLVE_MODE)
	gimp_layer_set_mode (bottom_layer, NORMAL_MODE);
    }

  /* Copy the tattoo and parasites of the bottom layer to the new layer */
  gimp_drawable_set_tattoo (GIMP_DRAWABLE (merge_layer),
			    gimp_drawable_get_tattoo (GIMP_DRAWABLE (layer)));
  GIMP_DRAWABLE (merge_layer)->parasites =
    gimp_parasite_list_copy (GIMP_DRAWABLE (layer)->parasites);

  while (reverse_list)
    {
      layer = (GimpLayer *) reverse_list->data;

      /*  determine what sort of operation is being attempted and
       *  if it's actually legal...
       */
      operation =
	valid_combinations[gimp_drawable_type (GIMP_DRAWABLE (merge_layer))][gimp_drawable_bytes (GIMP_DRAWABLE (layer))];

      if (operation == -1)
	{
	  g_message ("gimp_image_merge_layers attempting to merge incompatible layers\n");
	  return NULL;
	}

      gimp_drawable_offsets (GIMP_DRAWABLE (layer), &off_x, &off_y);
      x3 = CLAMP (off_x, x1, x2);
      y3 = CLAMP (off_y, y1, y2);
      x4 = CLAMP (off_x + gimp_drawable_width (GIMP_DRAWABLE (layer)), x1, x2);
      y4 = CLAMP (off_y + gimp_drawable_height (GIMP_DRAWABLE (layer)), y1, y2);

      /* configure the pixel regions  */
      pixel_region_init (&src1PR, 
			 gimp_drawable_data (GIMP_DRAWABLE (merge_layer)), 
			 (x3 - x1), (y3 - y1), (x4 - x3), (y4 - y3), 
			 TRUE);
      pixel_region_init (&src2PR, 
			 gimp_drawable_data (GIMP_DRAWABLE (layer)), 
			 (x3 - off_x), (y3 - off_y),
			 (x4 - x3), (y4 - y3), 
			 FALSE);

      if (layer->mask && layer->mask->apply_mask)
	{
	  pixel_region_init (&maskPR, 
			     gimp_drawable_data (GIMP_DRAWABLE (layer->mask)), 
			     (x3 - off_x), (y3 - off_y),
			     (x4 - x3), (y4 - y3), 
			     FALSE);
	  mask = &maskPR;
	}
      else
	{
	  mask = NULL;
	}

      combine_regions (&src1PR, &src2PR, &src1PR, mask, NULL,
		       layer->opacity, layer->mode, active, operation);

      gimp_image_remove_layer (gimage, layer);
      reverse_list = g_slist_next (reverse_list);
    }

  /* Save old mode in undo */
  if (bottom_layer)
    gimp_layer_set_mode (bottom_layer, bottom_mode);

  g_slist_free (reverse_list);

  /*  if the type is flatten, remove all the remaining layers  */
  if (merge_type == FLATTEN_IMAGE)
    {
      list = GIMP_LIST (gimage->layers)->list;
      while (list)
	{
	  layer = (GimpLayer *) list->data;

	  list = g_list_next (list);
	  gimp_image_remove_layer (gimage, layer);
	}

      gimp_image_add_layer (gimage, merge_layer, position);
    }
  else
    {
      /*  Add the layer to the gimage  */
      gimp_image_add_layer (gimage, merge_layer,
	 gimp_container_num_children (gimage->layers) - position + 1);
    }

  /* set the name after the original layers have been removed so we
   * don't end up with #2 appended to the name
   */
  gimp_object_set_name (GIMP_OBJECT (merge_layer), name);
  g_free (name);

  /*  End the merge undo group  */
  undo_push_group_end (gimage);

  gimp_drawable_set_visible (GIMP_DRAWABLE (merge_layer), TRUE);

  drawable_update (GIMP_DRAWABLE (merge_layer), 
		   0, 0, 
		   gimp_drawable_width (GIMP_DRAWABLE (merge_layer)), 
		   gimp_drawable_height (GIMP_DRAWABLE (merge_layer)));

  /*reinit_layer_idlerender (gimage, merge_layer);*/

  return merge_layer;
}

gboolean
gimp_image_add_layer (GimpImage *gimage, 
		      GimpLayer *layer, 
		      gint       position)
{
  LayerUndo *lu;

  g_return_val_if_fail (gimage != NULL, FALSE);
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  
  g_return_val_if_fail (layer != NULL, FALSE);
  g_return_val_if_fail (GIMP_IS_LAYER (layer), FALSE);

  if (GIMP_DRAWABLE (layer)->gimage != NULL && 
      GIMP_DRAWABLE (layer)->gimage != gimage) 
    {
      g_message ("gimp_image_add_layer: attempt to add layer to wrong image");
      return FALSE;
    }

  if (gimp_container_have (gimage->layers, GIMP_OBJECT (layer)))
    {
      g_message ("gimp_image_add_layer: trying to add layer to image twice");
      return FALSE;
    }

  /*  Prepare a layer undo and push it  */
  lu = g_new (LayerUndo, 1);
  lu->layer         = layer;
  lu->prev_position = 0;
  lu->prev_layer    = gimp_image_get_active_layer (gimage);
  undo_push_layer (gimage, LAYER_ADD_UNDO, lu);

  /*  If the layer is a floating selection, set the ID  */
  if (gimp_layer_is_floating_sel (layer))
    gimage->floating_sel = layer;

  /*  let the layer know about the gimage  */
  gimp_drawable_set_gimage (GIMP_DRAWABLE (layer), gimage);
  
  /*  If the layer has a mask, set the mask's gimage  */
  if (layer->mask)
    {
      gimp_drawable_set_gimage (GIMP_DRAWABLE (layer->mask), gimage);
    }

  /*  add the layer to the list at the specified position  */
  if (position == -1)
    {
      GimpLayer *active_layer;

      active_layer = gimp_image_get_active_layer (gimage);

      if (active_layer)
	{
	  position = gimp_container_get_child_index (gimage->layers, 
						     GIMP_OBJECT (active_layer));
	}
      else
	{
	  position = 0;
	}
    }

  /*  If there is a floating selection (and this isn't it!),
   *  make sure the insert position is greater than 0
   */
  if (position == 0                    &&
      gimp_image_floating_sel (gimage) &&
      (gimage->floating_sel != layer))
    {
      position = 1;
    }

  gimp_container_insert (gimage->layers, GIMP_OBJECT (layer), position);

  /*  notify the layers dialog of the currently active layer  */
  gimp_image_set_active_layer (gimage, layer);

  /*  update the new layer's area  */
  drawable_update (GIMP_DRAWABLE (layer),
		   0, 0,
		   gimp_drawable_width  (GIMP_DRAWABLE (layer)),
		   gimp_drawable_height (GIMP_DRAWABLE (layer)));

  /*  invalidate the composite preview  */
  gimp_viewable_invalidate_preview (GIMP_VIEWABLE (gimage));

  return TRUE;
}

void
gimp_image_remove_layer (GimpImage *gimage, 
			 GimpLayer *layer)
{
  LayerUndo *lu;

  g_return_if_fail (gimage != NULL);
  g_return_if_fail (GIMP_IS_IMAGE (gimage));
  
  g_return_if_fail (layer != NULL);
  g_return_if_fail (GIMP_IS_LAYER (layer));

  g_return_if_fail (gimp_container_have (gimage->layers, 
					 GIMP_OBJECT (layer)));

  /*  Push a layer undo  */
  lu = g_new (LayerUndo, 1);
  lu->layer         = layer;
  lu->prev_position = gimp_container_get_child_index (gimage->layers, 
						      GIMP_OBJECT (layer));
  lu->prev_layer    = layer;
  
  undo_push_layer (gimage, LAYER_REMOVE_UNDO, lu);

  gtk_object_ref (GTK_OBJECT (layer));

  gimp_container_remove (gimage->layers, GIMP_OBJECT (layer));
  gimage->layer_stack = g_slist_remove (gimage->layer_stack, layer);  

  /*  If this was the floating selection, reset the fs pointer  */
  if (gimage->floating_sel == layer)
    {
      gimage->floating_sel = NULL;
      
      floating_sel_reset (layer);
    }

  if (layer == gimp_image_get_active_layer (gimage))
    {
      if (gimage->layer_stack)
	{
	  gimp_image_set_active_layer (gimage, gimage->layer_stack->data);
	}
      else
	{
	  gimage->active_layer = NULL;

	  gtk_signal_emit (GTK_OBJECT (gimage),
			   gimp_image_signals[ACTIVE_LAYER_CHANGED]);
	}
    }

  /* Send out REMOVED signal from layer */
  gimp_drawable_removed (GIMP_DRAWABLE (layer));

  gtk_object_unref (GTK_OBJECT (layer));

  /*  invalidate the composite preview  */
  gimp_viewable_invalidate_preview (GIMP_VIEWABLE (gimage));
  gdisplays_update_full (gimage);
}

gboolean
gimp_image_raise_channel (GimpImage   *gimage, 
			  GimpChannel *channel)
{
  gint index;

  g_return_val_if_fail (gimage != NULL, FALSE);
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);

  g_return_val_if_fail (channel != NULL, FALSE);
  g_return_val_if_fail (GIMP_IS_CHANNEL (channel), FALSE);  

  index = gimp_container_get_child_index (gimage->channels, 
					  GIMP_OBJECT (channel));
  if (index == 0)
    {
      g_message (_("Channel cannot be raised any further"));
      return FALSE;
    }

  return gimp_image_position_channel (gimage, channel, index - 1, TRUE);
}

gboolean
gimp_image_lower_channel (GimpImage   *gimage, 
			  GimpChannel *channel)
{
  gint index;

  g_return_val_if_fail (gimage != NULL, FALSE);
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);

  g_return_val_if_fail (channel != NULL, FALSE);
  g_return_val_if_fail (GIMP_IS_CHANNEL (channel), FALSE);  
  
  index = gimp_container_get_child_index (gimage->channels, 
					  GIMP_OBJECT (channel));
  if (index == gimp_container_num_children (gimage->channels) - 1)
    {
      g_message (_("Channel cannot be lowered any further"));
      return FALSE;
    }

  return gimp_image_position_channel (gimage, channel, index + 1, TRUE);
}

gboolean
gimp_image_position_channel (GimpImage   *gimage, 
			     GimpChannel *channel,
			     gint         new_index,
                             gboolean     push_undo /* FIXME unused */)
{
  gint index;
  gint num_channels;

  g_return_val_if_fail (gimage != NULL, FALSE);
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);

  g_return_val_if_fail (channel != NULL, FALSE);
  g_return_val_if_fail (GIMP_IS_CHANNEL (channel), FALSE);  

  index = gimp_container_get_child_index (gimage->channels, 
					  GIMP_OBJECT (channel));
  if (index < 0)
    return FALSE;

  num_channels = gimp_container_num_children (gimage->channels);

  new_index = CLAMP (new_index, 0, num_channels - 1);

  if (new_index == index)
    return TRUE;

  gimp_container_reorder (gimage->channels, 
			  GIMP_OBJECT (channel), new_index);

  drawable_update (GIMP_DRAWABLE (channel),
		   0, 0,
		   gimp_drawable_width  (GIMP_DRAWABLE (channel)),
		   gimp_drawable_height (GIMP_DRAWABLE (channel)));

  return TRUE;
}

gboolean
gimp_image_add_channel (GimpImage   *gimage, 
			GimpChannel *channel, 
			gint         position)
{
  ChannelUndo *cu;

  g_return_val_if_fail (gimage != NULL, FALSE);
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  
  g_return_val_if_fail (channel != NULL, FALSE);
  g_return_val_if_fail (GIMP_IS_CHANNEL (channel), FALSE);

  if (GIMP_DRAWABLE (channel)->gimage != NULL &&
      GIMP_DRAWABLE (channel)->gimage != gimage)
    {
      g_message ("%s(): attempt to add channel to wrong image",
		 G_GNUC_FUNCTION);
      return FALSE;
    }

  if (gimp_container_have (gimage->channels, GIMP_OBJECT (channel)))
    {
      g_message ("%s(): trying to add channel to image twice",
		 G_GNUC_FUNCTION);
      return FALSE;
    }

  /*  Push a channel undo  */
  cu = g_new (ChannelUndo, 1);
  cu->channel       = channel;
  cu->prev_position = 0;
  cu->prev_channel  = gimp_image_get_active_channel (gimage);
  undo_push_channel (gimage, CHANNEL_ADD_UNDO, cu);

  /*  add the channel to the list  */
  gimp_container_add (gimage->channels, GIMP_OBJECT (channel));

  /*  notify this gimage of the currently active channel  */
  gimp_image_set_active_channel (gimage, channel);

  /*  if channel is visible, update the image  */
  if (gimp_drawable_get_visible (GIMP_DRAWABLE (channel)))
    drawable_update (GIMP_DRAWABLE (channel), 
		     0, 0, 
		     gimp_drawable_width (GIMP_DRAWABLE (channel)), 
		     gimp_drawable_height (GIMP_DRAWABLE (channel)));

  return TRUE;
}

void
gimp_image_remove_channel (GimpImage   *gimage, 
			   GimpChannel *channel)
{
  ChannelUndo *cu;

  g_return_if_fail (gimage != NULL);
  g_return_if_fail (GIMP_IS_IMAGE (gimage));
  
  g_return_if_fail (channel != NULL);
  g_return_if_fail (GIMP_IS_CHANNEL (channel));

  g_return_if_fail (gimp_container_have (gimage->channels, 
					 GIMP_OBJECT (channel)));

  /*  Prepare a channel undo--push it below  */
  cu = g_new (ChannelUndo, 1);
  cu->channel       = channel;
  cu->prev_position = gimp_container_get_child_index (gimage->channels, 
						      GIMP_OBJECT (channel));
  cu->prev_channel  = gimp_image_get_active_channel (gimage);
  undo_push_channel (gimage, CHANNEL_REMOVE_UNDO, cu);

  gtk_object_ref (GTK_OBJECT (channel));

  gimp_container_remove (gimage->channels, GIMP_OBJECT (channel));

  /* Send out REMOVED signal from channel */
  gimp_drawable_removed (GIMP_DRAWABLE (channel));

  if (channel == gimp_image_get_active_channel (gimage))
    {
      if (gimp_container_num_children (gimage->channels) > 0)
	{
	  gimp_image_set_active_channel
	    (gimage,
	     GIMP_CHANNEL (gimp_container_get_child_by_index (gimage->channels,
							      0)));
	}
      else
	{
	  gimp_image_unset_active_channel (gimage);
	}
    }

  gtk_object_unref (GTK_OBJECT (channel));

  /*  invalidate the composite preview  */
  gimp_viewable_invalidate_preview (GIMP_VIEWABLE (gimage));
  gdisplays_update_full (gimage);
}

/************************************************************/
/*  Access functions                                        */
/************************************************************/

gboolean
gimp_image_is_empty (const GimpImage *gimage)
{
  g_return_val_if_fail (gimage != NULL, TRUE);
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), TRUE);

  return (gimp_container_num_children (gimage->layers) == 0);
}

GimpDrawable *
gimp_image_active_drawable (const GimpImage *gimage)
{
  g_return_val_if_fail (gimage != NULL, NULL);
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  /*  If there is an active channel (a saved selection, etc.),
   *  we ignore the active layer
   */
  if (gimage->active_channel)
    {
      return GIMP_DRAWABLE (gimage->active_channel);
    }
  else if (gimage->active_layer)
    {
      GimpLayer *layer;

      layer = gimage->active_layer;

      if (layer->mask && layer->mask->edit_mask)
	return GIMP_DRAWABLE (layer->mask);
      else
	return GIMP_DRAWABLE (layer);
    }

  return NULL;
}

GimpImageBaseType
gimp_image_base_type (const GimpImage *gimage)
{
  g_return_val_if_fail (gimage != NULL, -1);
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), -1);

  return gimage->base_type;
}

GimpImageType
gimp_image_base_type_with_alpha (const GimpImage *gimage)
{
  g_return_val_if_fail (gimage != NULL, -1);
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), -1);

  switch (gimage->base_type)
    {
    case RGB:
      return RGBA_GIMAGE;
    case GRAY:
      return GRAYA_GIMAGE;
    case INDEXED:
      return INDEXEDA_GIMAGE;
    }
  return RGB_GIMAGE;
}

const gchar *
gimp_image_filename (const GimpImage *gimage)
{
  const gchar *filename;

  g_return_val_if_fail (gimage != NULL, NULL);
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  filename = gimp_object_get_name (GIMP_OBJECT (gimage));

  return filename ? filename : _("Untitled");
}

gboolean
gimp_image_undo_is_enabled (const GimpImage *gimage)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);

  return gimage->undo_on;
}

gboolean
gimp_image_undo_freeze (GimpImage *gimage)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);

  gimage->undo_on = FALSE;

  return TRUE;
}

gboolean
gimp_image_undo_thaw (GimpImage *gimage)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);

  gimage->undo_on = TRUE;

  return TRUE;
}

gboolean
gimp_image_undo_disable (GimpImage *gimage)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);

  return gimp_image_undo_freeze (gimage);
}

gboolean
gimp_image_undo_enable (GimpImage *gimage)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);

  /*  Free all undo steps as they are now invalidated  */
  undo_free (gimage);

  return gimp_image_undo_thaw (gimage);
}

void
gimp_image_undo_event (GimpImage *gimage, 
		       gint       event)
{
  gtk_signal_emit (GTK_OBJECT (gimage), gimp_image_signals[UNDO_EVENT], event);
}


/* NOTE about the gimage->dirty counter:
 *   If 0, then the image is clean (ie, copy on disk is the same as the one 
 *      in memory).
 *   If positive, then that's the number of dirtying operations done
 *       on the image since the last save.
 *   If negative, then user has hit undo and gone back in time prior
 *       to the saved copy.  Hitting redo will eventually come back to
 *       the saved copy.
 *
 *   The image is dirty (ie, needs saving) if counter is non-zero.
 *
 *   If the counter is around 10000, this is due to undo-ing back
 *   before a saved version, then mutating the image (thus destroying
 *   the redo stack).  Once this has happened, it's impossible to get
 *   the image back to the state on disk, since the redo info has been
 *   freed.  See undo.c for the gorey details.
 */


/*
 * NEVER CALL gimp_image_dirty() directly!
 *
 * If your code has just dirtied the image, push an undo instead.
 * Failing that, push the trivial undo which tells the user the
 * command is not undoable: undo_push_cantundo() (But really, it would
 * be best to push a proper undo).  If you just dirty the image
 * without pushing an undo then the dirty count is increased, but
 * popping that many undo actions won't lead to a clean image.
 */

gint
gimp_image_dirty (GimpImage *gimage)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);

  gimage->dirty++;
  gtk_signal_emit (GTK_OBJECT (gimage), gimp_image_signals[DIRTY]);

  TRC (("dirty %d -> %d\n", gimage->dirty-1, gimage->dirty));

  return gimage->dirty;
}

gint
gimp_image_clean (GimpImage *gimage)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);

  gimage->dirty--;
  gtk_signal_emit (GTK_OBJECT (gimage), gimp_image_signals[CLEAN]);
  
  TRC (("clean %d -> %d\n", gimage->dirty+1, gimage->dirty));
  
  return gimage->dirty;
}

void
gimp_image_clean_all (GimpImage *gimage)
{
  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  gimage->dirty = 0;

  gtk_signal_emit (GTK_OBJECT (gimage), gimp_image_signals[CLEAN]);
}

GimpLayer *
gimp_image_floating_sel (const GimpImage *gimage)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);

  if (gimage->floating_sel == NULL)
    return NULL;
  else
    return gimage->floating_sel;
}

guchar *
gimp_image_cmap (const GimpImage *gimage)
{
  return gimp_drawable_cmap (gimp_image_active_drawable (gimage));
}

/************************************************************/
/*  Projection access functions                             */
/************************************************************/

TileManager *
gimp_image_projection (GimpImage *gimage)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  if ((gimage->projection == NULL) ||
      (tile_manager_width (gimage->projection) != gimage->width) ||
      (tile_manager_height (gimage->projection) != gimage->height))
    {
      gimp_image_allocate_projection (gimage);
    }
  
  return gimage->projection;
}

GimpImageType
gimp_image_projection_type (const GimpImage *gimage)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), -1);

  return gimage->proj_type;
}

gint
gimp_image_projection_bytes (const GimpImage *gimage)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), -1);

  return gimage->proj_bytes;
}

gint
gimp_image_projection_opacity (const GimpImage *gimage)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), -1);

  return OPAQUE_OPACITY;
}

void
gimp_image_projection_realloc (GimpImage *gimage)
{
  gimp_image_allocate_projection (gimage);
}

/************************************************************/
/*  Composition access functions                            */
/************************************************************/

TileManager *
gimp_image_composite (GimpImage *gimage)
{
  return gimp_image_projection (gimage);
}

GimpImageType
gimp_image_composite_type (const GimpImage *gimage)
{
  return gimp_image_projection_type (gimage);
}

gint
gimp_image_composite_bytes (const GimpImage *gimage)
{
  return gimp_image_projection_bytes (gimage);
}

gboolean
gimp_image_preview_valid (const GimpImage *gimage)
{
  return gimage->comp_preview_valid;
}

static TempBuf *
gimp_image_get_preview (GimpViewable *viewable,
			gint          width, 
			gint          height)
{
  GimpImage *gimage;

  gimage = GIMP_IMAGE (viewable);

  if (gimage->comp_preview_valid            &&
      gimage->comp_preview->width  == width &&
      gimage->comp_preview->height == height)
    {
      /*  The easy way  */
      return gimage->comp_preview;
    }
  else
    {
      /*  The hard way  */
      if (gimage->comp_preview)
	temp_buf_free (gimage->comp_preview);

      /*  Actually construct the composite preview from the layer previews!
       *  This might seem ridiculous, but it's actually the best way, given
       *  a number of unsavory alternatives.
       */
      gimage->comp_preview = gimp_image_get_new_preview (viewable,
							 width, height);

      gimage->comp_preview_valid = TRUE;

      return gimage->comp_preview;
    }
}

static TempBuf *
gimp_image_get_new_preview (GimpViewable *viewable,
			    gint          width, 
			    gint          height)
{
  GimpImage   *gimage;
  GimpLayer   *layer;
  GimpLayer   *floating_sel;
  PixelRegion  src1PR, src2PR, maskPR;
  PixelRegion *mask;
  TempBuf     *comp;
  TempBuf     *layer_buf;
  TempBuf     *mask_buf;
  GList       *list;
  GSList      *reverse_list = NULL;
  gdouble      ratio;
  gint         x, y, w, h;
  gint         x1, y1, x2, y2;
  gint         bytes;
  gboolean     construct_flag;
  gint         visible[MAX_CHANNELS] = { 1, 1, 1, 1 };
  gint         off_x, off_y;

  gimage = GIMP_IMAGE (viewable);

  ratio = (gdouble) width / (gdouble) gimage->width;

  switch (gimp_image_base_type (gimage))
    {
    case RGB:
    case INDEXED:
      bytes = 4;
      break;
    case GRAY:
      bytes = 2;
      break;
    default:
      bytes = 0;
      break;
    }

  /*  The construction buffer  */
  comp = temp_buf_new (width, height, bytes, 0, 0, NULL);
  temp_buf_data_clear (comp);

  floating_sel = NULL;

  for (list = GIMP_LIST (gimage->layers)->list; 
       list; 
       list = g_list_next (list))
    {
      layer = (GimpLayer *) list->data;

      /*  only add layers that are visible to the list  */
      if (gimp_drawable_get_visible (GIMP_DRAWABLE (layer)))
	{
	  /*  floating selections are added right above the layer 
	   *  they are attached to
	   */
	  if (gimp_layer_is_floating_sel (layer))
	    {
	      floating_sel = layer;
	    }
	  else
	    {
	      if (floating_sel &&
		  floating_sel->fs.drawable == GIMP_DRAWABLE (layer))
		{
		  reverse_list = g_slist_prepend (reverse_list, floating_sel);
		}

	      reverse_list = g_slist_prepend (reverse_list, layer);
	    }
	}
    }

  construct_flag = FALSE;

  for (; reverse_list; reverse_list = g_slist_next (reverse_list))
    {
      layer = (GimpLayer *) reverse_list->data;

      gimp_drawable_offsets (GIMP_DRAWABLE (layer), &off_x, &off_y);

      x = (gint) RINT (ratio * off_x);
      y = (gint) RINT (ratio * off_y);
      w = (gint) RINT (ratio * gimp_drawable_width (GIMP_DRAWABLE (layer)));
      h = (gint) RINT (ratio * gimp_drawable_height (GIMP_DRAWABLE (layer)));

      if (w < 1 || h < 1)
	continue;

      x1 = CLAMP (x, 0, width);
      y1 = CLAMP (y, 0, height);
      x2 = CLAMP (x + w, 0, width);
      y2 = CLAMP (y + h, 0, height);

      src1PR.bytes     = comp->bytes;
      src1PR.x         = x1;
      src1PR.y         = y1;
      src1PR.w         = (x2 - x1);
      src1PR.h         = (y2 - y1);
      src1PR.rowstride = comp->width * src1PR.bytes;
      src1PR.data      = (temp_buf_data (comp) +
                          y1 * src1PR.rowstride + x1 * src1PR.bytes);

      layer_buf = gimp_viewable_get_preview (GIMP_VIEWABLE (layer), w, h);
      src2PR.bytes     = layer_buf->bytes;
      src2PR.w         = src1PR.w;  
      src2PR.h         = src1PR.h;
      src2PR.x         = src1PR.x; 
      src2PR.y         = src1PR.y;
      src2PR.rowstride = layer_buf->width * src2PR.bytes;
      src2PR.data      = (temp_buf_data (layer_buf) + 
                          (y1 - y) * src2PR.rowstride +
                          (x1 - x) * src2PR.bytes);

      if (layer->mask && layer->mask->apply_mask)
	{
	  mask_buf = gimp_viewable_get_preview (GIMP_VIEWABLE (layer->mask),
						w, h);
	  maskPR.bytes     = mask_buf->bytes;
	  maskPR.rowstride = mask_buf->width;
	  maskPR.data      = (mask_buf_data (mask_buf) +
                              (y1 - y) * maskPR.rowstride +
                              (x1 - x) * maskPR.bytes);
	  mask = &maskPR;
	}
      else
	{
	  mask = NULL;
	}

      /*  Based on the type of the layer, project the layer onto the
       *   composite preview...
       *  Indexed images are actually already converted to RGB and RGBA,
       *   so just project them as if they were type "intensity"
       *  Send in all TRUE for visible since that info doesn't matter
       *   for previews
       */
      switch (gimp_drawable_type (GIMP_DRAWABLE (layer)))
	{
	case RGB_GIMAGE: case GRAY_GIMAGE: case INDEXED_GIMAGE:
	  if (! construct_flag)
	    initial_region (&src2PR, &src1PR, 
			    mask, NULL, layer->opacity,
			    layer->mode, visible, INITIAL_INTENSITY);
	  else
	    combine_regions (&src1PR, &src2PR, &src1PR, 
			     mask, NULL, layer->opacity,
			     layer->mode, visible, COMBINE_INTEN_A_INTEN);
	  break;

	case RGBA_GIMAGE: case GRAYA_GIMAGE: case INDEXEDA_GIMAGE:
	  if (! construct_flag)
	    initial_region (&src2PR, &src1PR, 
			    mask, NULL, layer->opacity,
			    layer->mode, visible, INITIAL_INTENSITY_ALPHA);
	  else
	    combine_regions (&src1PR, &src2PR, &src1PR, 
			     mask, NULL, layer->opacity,
			     layer->mode, visible, COMBINE_INTEN_A_INTEN_A);
	  break;

	default:
	  break;
	}

      construct_flag = TRUE;
    }

  g_slist_free (reverse_list);

  return comp;
}
