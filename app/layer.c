/* TODO: make sure has_alpha gets set */

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
#include <string.h>

#include "drawable.h"
#include "floating_sel.h"
#include "gdisplay.h"
#include "gimage.h"
#include "gimage_mask.h"
#include "layer.h"
#include "paint_funcs.h"
#include "temp_buf.h"
#include "parasitelist.h"
#include "undo.h"
#include "gimpsignal.h"
#include "gimppreviewcache.h"

#include "layer_pvt.h"
#include "tile_manager_pvt.h"
#include "tile.h"			/* ick. */

#include "libgimp/gimpmath.h"
#include "libgimp/gimpparasite.h"

#include "libgimp/gimpintl.h"


enum
{
  REMOVED,
  LAST_SIGNAL
};

static void gimp_layer_class_init    (GimpLayerClass *klass);
static void gimp_layer_init          (GimpLayer      *layer);
static void gimp_layer_destroy       (GtkObject      *object);
static void layer_invalidate_preview (GtkObject      *object);

static void gimp_layer_mask_class_init (GimpLayerMaskClass *klass);
static void gimp_layer_mask_init       (GimpLayerMask      *layermask);
static void gimp_layer_mask_destroy    (GtkObject          *object);

static guint layer_signals[LAST_SIGNAL] = { 0 };

/*
  static guint layer_mask_signals[LAST_SIGNAL] = { 0 };
 */

static GimpDrawableClass *layer_parent_class = NULL;
static GimpChannelClass  *layer_mask_parent_class = NULL;

GtkType
gimp_layer_get_type (void)
{
  static GtkType layer_type = 0;

  if (!layer_type)
    {
      GtkTypeInfo layer_info =
      {
	"GimpLayer",
	sizeof (GimpLayer),
	sizeof (GimpLayerClass),
	(GtkClassInitFunc) gimp_layer_class_init,
	(GtkObjectInitFunc) gimp_layer_init,
        /* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      layer_type = gtk_type_unique (gimp_drawable_get_type (), &layer_info);
    }

  return layer_type;
}

static void
gimp_layer_class_init (GimpLayerClass *class)
{
  GtkObjectClass    *object_class;
  GimpDrawableClass *drawable_class;

  object_class = (GtkObjectClass*) class;
  drawable_class = (GimpDrawableClass*) class;

  layer_parent_class = gtk_type_class (gimp_drawable_get_type ());

  layer_signals[REMOVED] =
	  gimp_signal_new ("removed",
			   0, object_class->type, 0, gimp_sigtype_void);

  gtk_object_class_add_signals (object_class, layer_signals, LAST_SIGNAL);

  object_class->destroy = gimp_layer_destroy;
  drawable_class->invalidate_preview = layer_invalidate_preview;
}

static void
gimp_layer_init (GimpLayer *layer)
{
}

GtkType
gimp_layer_mask_get_type (void)
{
  static GtkType layer_mask_type = 0;

  if (!layer_mask_type)
    {
      GtkTypeInfo layer_mask_info =
      {
	"GimpLayerMask",
	sizeof (GimpLayerMask),
	sizeof (GimpLayerMaskClass),
	(GtkClassInitFunc) gimp_layer_mask_class_init,
	(GtkObjectInitFunc) gimp_layer_mask_init,
        /* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      layer_mask_type = gtk_type_unique (gimp_channel_get_type (), 
					 &layer_mask_info);
    }

  return layer_mask_type;
}

static void
gimp_layer_mask_class_init (GimpLayerMaskClass *class)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) class;
  layer_mask_parent_class = gtk_type_class (gimp_channel_get_type ());

  /*
  gtk_object_class_add_signals (object_class, layer_mask_signals, LAST_SIGNAL);
  */

  object_class->destroy = gimp_layer_mask_destroy;
}

static void
gimp_layer_mask_init (GimpLayerMask *layermask)
{
}

/*  static functions  */

static void transform_color     (GImage            *gimage,
				 PixelRegion       *layerPR,
				 PixelRegion       *bufPR,
				 GimpDrawable      *drawable,
				 GimpImageBaseType  type);
static void layer_preview_scale (GimpImageBaseType  type,
				 guchar            *cmap,
				 PixelRegion       *srcPR,
				 PixelRegion       *destPR,
				 gint               subsample);

/*
 *  Static variables
 */
gint layer_get_count = 0;


/********************************/
/*  Local function definitions  */
/********************************/

static void
layer_invalidate_preview (GtkObject *object)
{
  GimpLayer *layer;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GIMP_IS_LAYER (object));
  
  layer = GIMP_LAYER (object);

  if (layer_is_floating_sel (layer)) 
    floating_sel_invalidate (layer);
}

static void
transform_color (GImage            *gimage,
		 PixelRegion       *layerPR,
		 PixelRegion       *bufPR,
		 GimpDrawable      *drawable,
		 GimpImageBaseType  type)
{
  gint      i;
  gint      h;
  guchar   *s;
  guchar   *d;
  gpointer  pr;

  for (pr = pixel_regions_register (2, layerPR, bufPR); 
       pr != NULL; 
       pr = pixel_regions_process (pr))
    {
      h = layerPR->h;
      s = bufPR->data;
      d = layerPR->data;

      while (h--)
	{
	  for (i = 0; i < layerPR->w; i++)
	    {
	      gimage_transform_color (gimage, drawable,
				      s + (i * bufPR->bytes),
				      d + (i * layerPR->bytes), type);
	      /*  copy alpha channel  */
	      d[(i + 1) * layerPR->bytes - 1] = s[(i + 1) * bufPR->bytes - 1];
	    }

	  s += bufPR->rowstride;
	  d += layerPR->rowstride;
	}
    }
}

/**************************/
/*  Function definitions  */
/**************************/

Layer *
layer_new (GimpImage        *gimage,
	   gint              width,
	   gint              height,
	   GimpImageType     type,
	   gchar            *name,
	   gint              opacity,
	   LayerModeEffects  mode)
{
  Layer *layer;

  if (width == 0 || height == 0)
    {
      g_message (_("Zero width or height layers not allowed."));
      return NULL;
    }

  layer = gtk_type_new (gimp_layer_get_type ());

  gimp_drawable_configure (GIMP_DRAWABLE (layer),
			   gimage, width, height, type, name);
  layer->linked         = FALSE;
  layer->preserve_trans = FALSE;

  /*  no layer mask is present at start  */
  layer->mask       = NULL;
  layer->apply_mask = FALSE;
  layer->edit_mask  = FALSE;
  layer->show_mask  = FALSE;

  /*  mode and opacity  */
  layer->mode    = mode;
  layer->opacity = opacity;

  /*  floating selection variables  */
  layer->fs.backing_store  = NULL;
  layer->fs.drawable       = NULL;
  layer->fs.initial        = TRUE;
  layer->fs.boundary_known = FALSE;
  layer->fs.segs           = NULL;
  layer->fs.num_segs       = 0;

  return layer;
}

Layer *
layer_ref (Layer *layer)
{
  gtk_object_ref  (GTK_OBJECT (layer));
  gtk_object_sink (GTK_OBJECT (layer));

  return layer;
}

void
layer_unref (Layer *layer)
{
  gtk_object_unref (GTK_OBJECT (layer));
}

Layer *
layer_copy (Layer    *layer,
	    gboolean  add_alpha)
{
  gchar         *layer_name;
  Layer         *new_layer;
  GimpImageType  new_type;
  gchar         *ext;
  gint           number;
  gchar         *name;
  gint           len;
  PixelRegion    srcPR;
  PixelRegion    destPR;

  /*  formulate the new layer name  */
  name = layer_get_name (layer);
  ext = strrchr (name, '#');
  len = strlen (_("copy"));

  if ((strlen(name) >= len &&
       strcmp (&name[strlen (name) - len], _("copy")) == 0) ||
      (ext && (number = atoi (ext + 1)) > 0 && 
       ((gint) (log10 (number) + 1)) == strlen (ext + 1)))
    /* don't have redundant "copy"s */
    layer_name = g_strdup (name);
  else
    layer_name = g_strdup_printf (_("%s copy"), name);

  /*  when copying a layer, the copy ALWAYS has an alpha channel  */
  if (add_alpha)
    {
      switch (GIMP_DRAWABLE (layer)->type)
	{
	case RGB_GIMAGE:
	  new_type = RGBA_GIMAGE;
	  break;
	case GRAY_GIMAGE:
	  new_type = GRAYA_GIMAGE;
	  break;
	case INDEXED_GIMAGE:
	  new_type = INDEXEDA_GIMAGE;
	  break;
	default:
	  new_type = GIMP_DRAWABLE (layer)->type;
	  break;
	}
    }
  else
    new_type = GIMP_DRAWABLE (layer)->type;

  /*  allocate a new layer object  */
  new_layer = layer_new (GIMP_DRAWABLE (layer)->gimage,
			 GIMP_DRAWABLE (layer)->width,
			 GIMP_DRAWABLE (layer)->height,
			 new_type, layer_name, layer->opacity, layer->mode);
  if (!new_layer)
    {
      g_message ("layer_copy: could not allocate new layer");
      goto cleanup;
    }

  GIMP_DRAWABLE (new_layer)->offset_x = GIMP_DRAWABLE (layer)->offset_x;
  GIMP_DRAWABLE (new_layer)->offset_y = GIMP_DRAWABLE (layer)->offset_y;
  GIMP_DRAWABLE (new_layer)->visible  = GIMP_DRAWABLE (layer)->visible;
  new_layer->linked         = layer->linked;
  new_layer->preserve_trans = layer->preserve_trans;

  /*  copy the contents across layers  */
  if (new_type == GIMP_DRAWABLE (layer)->type)
    {
      pixel_region_init (&srcPR, GIMP_DRAWABLE (layer)->tiles, 
			 0, 0, 
			 GIMP_DRAWABLE (layer)->width, 
			 GIMP_DRAWABLE (layer)->height, 
			 FALSE);
      pixel_region_init (&destPR, GIMP_DRAWABLE(new_layer)->tiles, 
			 0, 0, 
			 GIMP_DRAWABLE (layer)->width, 
			 GIMP_DRAWABLE (layer)->height, 
			 TRUE);
      copy_region (&srcPR, &destPR);
    }
  else
    {
      pixel_region_init (&srcPR, GIMP_DRAWABLE (layer)->tiles, 
			 0, 0, 
			 GIMP_DRAWABLE (layer)->width, 
			 GIMP_DRAWABLE (layer)->height, 
			 FALSE);
      pixel_region_init (&destPR, GIMP_DRAWABLE (new_layer)->tiles, 
			 0, 0, 
			 GIMP_DRAWABLE (layer)->width, 
			 GIMP_DRAWABLE(layer)->height, 
			 TRUE);
      add_alpha_region (&srcPR, &destPR);
    }

  /*  duplicate the layer mask if necessary  */
  if (layer->mask)
    {
      new_layer->mask       = layer_mask_ref (layer_mask_copy (layer->mask));
      new_layer->apply_mask = layer->apply_mask;
      new_layer->edit_mask  = layer->edit_mask;
      new_layer->show_mask  = layer->show_mask;

      layer_mask_set_layer (new_layer->mask, new_layer);
    }

  /* copy the parasites */
  gtk_object_unref (GTK_OBJECT (GIMP_DRAWABLE (new_layer)->parasites));
  GIMP_DRAWABLE (new_layer)->parasites 
    = parasite_list_copy (GIMP_DRAWABLE (layer)->parasites);

 cleanup:
  /*  free up the layer_name memory  */
  g_free (layer_name);

  return new_layer;
}

Layer *
layer_new_from_tiles (GimpImage        *gimage,
                      GimpImageType     layer_type,
		      TileManager      *tiles,
		      gchar            *name,
		      gint              opacity,
		      LayerModeEffects  mode)
{
  Layer       *new_layer;
  PixelRegion  layerPR;
  PixelRegion  bufPR;

  /*  Function copies buffer to a layer
   *  taking into consideration the possibility of transforming
   *  the contents to meet the requirements of the target image type
   */

  /*  If no image or no tile manager, return NULL  */
  if (!gimage || !tiles )
    return NULL;
  
  /*  the layer_type needs to have alpha */
  g_return_val_if_fail (GIMP_IMAGE_TYPE_HAS_ALPHA (layer_type), NULL);

   /*  Create the new layer  */
  new_layer = layer_new (0, tiles->width, tiles->height,
			 layer_type, name, opacity, mode);

  if (!new_layer)
    {
      g_message ("layer_new_from_tiles: could not allocate new layer");
      return NULL;
    }

  /*  Configure the pixel regions  */
  pixel_region_init (&layerPR, GIMP_DRAWABLE (new_layer)->tiles, 
		     0, 0, 
		     GIMP_DRAWABLE (new_layer)->width, 
		     GIMP_DRAWABLE (new_layer)->height, 
		     TRUE);
  pixel_region_init (&bufPR, tiles, 
		     0, 0, 
		     GIMP_DRAWABLE (new_layer)->width, 
		     GIMP_DRAWABLE (new_layer)->height, 
		     FALSE);

  if ((tiles->bpp == 4 && GIMP_DRAWABLE (new_layer)->type == RGBA_GIMAGE) ||
      (tiles->bpp == 2 && GIMP_DRAWABLE (new_layer)->type == GRAYA_GIMAGE))
    /*  If we want a layer the same type as the buffer  */
    copy_region (&bufPR, &layerPR);
  else
    /*  Transform the contents of the buf to the new_layer  */
    transform_color (gimage, &layerPR, &bufPR, GIMP_DRAWABLE (new_layer),
		     (tiles->bpp == 4) ? RGB : GRAY);
  
  return new_layer;
}

LayerMask *
layer_add_mask (Layer     *layer,
		LayerMask *mask)
{
  if (layer->mask)
    return NULL;

  layer->mask = layer_mask_ref (mask);
  mask->layer = layer;

  /*  Set the application mode in the layer to "apply"  */
  layer->apply_mask = TRUE;
  layer->edit_mask  = TRUE;
  layer->show_mask  = FALSE;

  drawable_update (GIMP_DRAWABLE (layer),
		   0, 0,
		   GIMP_DRAWABLE (layer)->width, 
		   GIMP_DRAWABLE (layer)->height);

  return layer->mask;
}

LayerMask *
layer_create_mask (Layer       *layer,
		   AddMaskType  add_mask_type)
{
  PixelRegion  maskPR;
  PixelRegion  layerPR;
  LayerMask   *mask;
  gchar       *mask_name;
  guchar       black[3] = {0, 0, 0};
  guchar       white_mask = OPAQUE_OPACITY;
  guchar       black_mask = TRANSPARENT_OPACITY;

  mask_name = g_strdup_printf (_("%s mask"), GIMP_DRAWABLE (layer)->name);

  /*  Create the layer mask  */
  mask = layer_mask_new (GIMP_DRAWABLE (layer)->gimage,
			 GIMP_DRAWABLE (layer)->width,
			 GIMP_DRAWABLE (layer)->height,
			 mask_name, OPAQUE_OPACITY, black);
  GIMP_DRAWABLE (mask)->offset_x = GIMP_DRAWABLE (layer)->offset_x;
  GIMP_DRAWABLE (mask)->offset_y = GIMP_DRAWABLE (layer)->offset_y;

  pixel_region_init (&maskPR, GIMP_DRAWABLE (mask)->tiles, 
		     0, 0, 
		     GIMP_DRAWABLE (mask)->width, GIMP_DRAWABLE (mask)->height, 
		     TRUE);

  switch (add_mask_type)
    {
    case ADD_WHITE_MASK:
      color_region (&maskPR, &white_mask);
      break;
    case ADD_BLACK_MASK:
      color_region (&maskPR, &black_mask);
      break;
    case ADD_ALPHA_MASK:
      /*  Extract the layer's alpha channel  */
      if (layer_has_alpha (layer))
	{
	  pixel_region_init (&layerPR, GIMP_DRAWABLE (layer)->tiles, 
			     0, 0, 
			     GIMP_DRAWABLE (layer)->width, 
			     GIMP_DRAWABLE (layer)->height, 
			     FALSE);
	  extract_alpha_region (&layerPR, NULL, &maskPR);
	}
      break;
    }

  g_free (mask_name);
  return mask;
}

Layer *
layer_get_ID (gint ID)
{
  GimpDrawable *drawable;

  drawable = drawable_get_ID (ID);
  if (drawable && GIMP_IS_LAYER (drawable)) 
    return GIMP_LAYER (drawable);
  else
    return NULL;
}

void
layer_delete (Layer *layer)
{
  gtk_object_unref (GTK_OBJECT (layer));
}

static void
gimp_layer_destroy (GtkObject *object)
{
  GimpLayer *layer;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GIMP_IS_LAYER (object));

  layer = GIMP_LAYER (object);

  /*  if a layer mask exists, free it  */
  if (layer->mask)
    layer_mask_delete (layer->mask);

  /*  free the layer boundary if it exists  */
  if (layer->fs.segs)
    g_free (layer->fs.segs);

  /*  free the floating selection if it exists  */
  if (layer_is_floating_sel (layer))
    {
      tile_manager_destroy (layer->fs.backing_store);
    }

  if (GTK_OBJECT_CLASS (layer_parent_class)->destroy)
    (*GTK_OBJECT_CLASS (layer_parent_class)->destroy) (object);
}

/* The removed signal is sent out when the layer is no longer
 * associcated with an image.  It's needed because layers aren't
 * destroyed immediately, but kept around for undo purposes.  Connect
 * to the removed signal to update bits of UI that are tied to a
 * particular layer. */
void
layer_removed (Layer    *layer,
	       gpointer  data)
{
  g_return_if_fail (layer != NULL);
  g_return_if_fail (GIMP_IS_LAYER (layer));

  gtk_signal_emit (GTK_OBJECT (layer), layer_signals[REMOVED]);
}

void
layer_apply_mask (Layer         *layer,
		  MaskApplyMode  mode)
{
  PixelRegion srcPR, maskPR;

  if (!layer->mask)
    return;

  /*  this operation can only be done to layers with an alpha channel  */
  if (! layer_has_alpha (layer))
    return;

  /*  Need to save the mask here for undo  */

  if (mode == APPLY)
    {
      /*  Put this apply mask operation on the undo stack  */
      drawable_apply_image (GIMP_DRAWABLE (layer),
			    0, 0,
			    GIMP_DRAWABLE (layer)->width,
			    GIMP_DRAWABLE (layer)->height,
			    NULL, FALSE);

      /*  Combine the current layer's alpha channel and the mask  */
      pixel_region_init (&srcPR, GIMP_DRAWABLE (layer)->tiles, 
			 0, 0, 
			 GIMP_DRAWABLE (layer)->width, 
			 GIMP_DRAWABLE(layer)->height, 
			 TRUE);
      pixel_region_init (&maskPR, GIMP_DRAWABLE (layer->mask)->tiles, 
			 0, 0, 
			 GIMP_DRAWABLE (layer)->width, 
			 GIMP_DRAWABLE(layer)->height, 
			 FALSE);

      apply_mask_to_region (&srcPR, &maskPR, OPAQUE_OPACITY);
      GIMP_DRAWABLE (layer)->preview_valid = FALSE;

      layer->mask       = NULL;
      layer->apply_mask = FALSE;
      layer->edit_mask  = FALSE;
      layer->show_mask  = FALSE;
    }
  else if (mode == DISCARD)
    {
      layer->mask       = NULL;
      layer->apply_mask = FALSE;
      layer->edit_mask  = FALSE;
      layer->show_mask  = FALSE;
    }
}

void
layer_translate (Layer    *layer,
		 gint      off_x,
		 gint      off_y)
{
  /*  the undo call goes here  */
  undo_push_layer_displace (GIMP_DRAWABLE (layer)->gimage, layer);

  /*  update the affected region  */
  drawable_update (GIMP_DRAWABLE (layer), 0, 0, 
		   GIMP_DRAWABLE (layer)->width, 
		   GIMP_DRAWABLE (layer)->height);

  /*  invalidate the selection boundary because of a layer modification  */
  layer_invalidate_boundary (layer);
 
  /*  update the layer offsets  */
  GIMP_DRAWABLE (layer)->offset_x += off_x;
  GIMP_DRAWABLE (layer)->offset_y += off_y;

  /*  update the affected region  */
  drawable_update (GIMP_DRAWABLE (layer), 
		   0, 0, 
		   GIMP_DRAWABLE (layer)->width, 
		   GIMP_DRAWABLE (layer)->height);

  if (layer->mask) 
    {
      GIMP_DRAWABLE (layer->mask)->offset_x += off_x;
      GIMP_DRAWABLE (layer->mask)->offset_y += off_y;

      /*  invalidate the mask preview  */
      gimp_drawable_invalidate_preview (GIMP_DRAWABLE (layer->mask),
					FALSE);
    }
}

void
layer_add_alpha (Layer *layer)
{
  PixelRegion srcPR, destPR;
  TileManager *new_tiles;
  GimpImageType type;

  /*  Don't bother if the layer already has alpha  */
  switch (GIMP_DRAWABLE (layer)->type)
    {
    case RGB_GIMAGE:
      type = RGBA_GIMAGE;
      break;
    case GRAY_GIMAGE:
      type = GRAYA_GIMAGE;
      break;
    case INDEXED_GIMAGE:
      type = INDEXEDA_GIMAGE;
      break;
    case RGBA_GIMAGE:
    case GRAYA_GIMAGE:
    case INDEXEDA_GIMAGE:
    default:
      return;
      break;
    }

  /*  Configure the pixel regions  */
  pixel_region_init (&srcPR, GIMP_DRAWABLE (layer)->tiles, 
		     0, 0, 
		     GIMP_DRAWABLE (layer)->width, 
		     GIMP_DRAWABLE (layer)->height, 
		     FALSE);

  /*  Allocate the new layer, configure dest region  */
  new_tiles = tile_manager_new (GIMP_DRAWABLE (layer)->width, 
				GIMP_DRAWABLE (layer)->height, 
				GIMP_DRAWABLE (layer)->bytes + 1);
  pixel_region_init (&destPR, new_tiles, 
		     0, 0, 
		     GIMP_DRAWABLE (layer)->width, 
		     GIMP_DRAWABLE (layer)->height, 
		     TRUE);

  /*  Add an alpha channel  */
  add_alpha_region (&srcPR, &destPR);

  /*  Push the layer on the undo stack  */
  undo_push_layer_mod (GIMP_DRAWABLE (layer)->gimage, layer);

  /*  Configure the new layer  */
  GIMP_DRAWABLE (layer)->tiles         = new_tiles;
  GIMP_DRAWABLE (layer)->type          = type;
  GIMP_DRAWABLE (layer)->bytes         = GIMP_DRAWABLE(layer)->bytes + 1;
  GIMP_DRAWABLE (layer)->has_alpha     = GIMP_IMAGE_TYPE_HAS_ALPHA (type);
  GIMP_DRAWABLE (layer)->preview_valid = FALSE;

  gtk_signal_emit_by_name (GTK_OBJECT (gimp_drawable_gimage (GIMP_DRAWABLE (layer))),
			   "restructure");
}

static void
layer_scale_lowlevel (Layer *layer,
		      gint   new_width,
		      gint   new_height,
		      gint   new_offset_x,
		      gint   new_offset_y)
{
  PixelRegion srcPR, destPR;
  TileManager *new_tiles;

  /*  Update the old layer position  */
  drawable_update (GIMP_DRAWABLE (layer),
		   0, 0,
		   GIMP_DRAWABLE (layer)->width, 
		   GIMP_DRAWABLE (layer)->height);

  /*  Configure the pixel regions  */
  pixel_region_init (&srcPR, GIMP_DRAWABLE(layer)->tiles, 
		     0, 0, 
		     GIMP_DRAWABLE (layer)->width, 
		     GIMP_DRAWABLE (layer)->height, 
		     FALSE);

  /*  Allocate the new layer, configure dest region  */
  new_tiles = tile_manager_new (new_width, new_height, 
				GIMP_DRAWABLE (layer)->bytes);
  pixel_region_init (&destPR, new_tiles, 
		     0, 0, 
		     new_width, new_height, 
		     TRUE);

  /*  Scale the layer -
   *   If the layer is of type INDEXED, then we don't use pixel-value
   *   resampling because that doesn't necessarily make sense for INDEXED
   *   images.
   */
  if ((GIMP_DRAWABLE (layer)->type == INDEXED_GIMAGE) || 
      (GIMP_DRAWABLE (layer)->type == INDEXEDA_GIMAGE))
    scale_region_no_resample (&srcPR, &destPR);
  else
    scale_region (&srcPR, &destPR);

  /*  Push the layer on the undo stack  */
  undo_push_layer_mod (GIMP_DRAWABLE (layer)->gimage, layer);

  /*  Configure the new layer  */

  GIMP_DRAWABLE (layer)->offset_x = new_offset_x;
  GIMP_DRAWABLE (layer)->offset_y = new_offset_y;
  GIMP_DRAWABLE (layer)->tiles    = new_tiles;
  GIMP_DRAWABLE (layer)->width    = new_width;
  GIMP_DRAWABLE (layer)->height   = new_height;

  /*  If there is a layer mask, make sure it gets scaled also  */
  if (layer->mask) 
    {
      GIMP_DRAWABLE (layer->mask)->offset_x = GIMP_DRAWABLE (layer)->offset_x;
      GIMP_DRAWABLE (layer->mask)->offset_y = GIMP_DRAWABLE (layer)->offset_y;
      channel_scale (GIMP_CHANNEL (layer->mask), new_width, new_height);
    }

  /*  Make sure we're not caching any old selection info  */
  layer_invalidate_boundary (layer);
  
  /*  Update the new layer position  */

  drawable_update (GIMP_DRAWABLE (layer),
		   0, 0,
		   GIMP_DRAWABLE (layer)->width, 
		   GIMP_DRAWABLE (layer)->height);
}

/**
 * layer_check_scaling:
 * @layer:      Layer to check
 * @new_width:  proposed width of layer, in pixels
 * @new_height: proposed height of layer, in pixels
 *
 * Scales layer dimensions, then snaps them to pixel centers
 *
 * Returns FALSE if any dimension reduces to zero as a result 
 *         of this; otherwise, returns TRUE.
 */
gboolean 
layer_check_scaling (Layer  *layer,
		     gint    new_width,
		     gint    new_height)
{
   GImage  *gimage           = GIMP_DRAWABLE (layer)->gimage;
   gdouble  img_scale_w      = (gdouble) new_width / (gdouble) gimage->width;
   gdouble  img_scale_h      = (gdouble) new_height / (gdouble) gimage->height;
   gint     new_layer_width  = 
     (gint) (0.5 + img_scale_w * (gdouble) GIMP_DRAWABLE (layer)->width); 
   gint     new_layer_height = 
     (gint) (0.5 + img_scale_h * (gdouble) GIMP_DRAWABLE (layer)->height);

   return (new_layer_width != 0 && new_layer_height != 0);  
}

/**
 * layer_scale_by_factors:
 * @layer: Layer to be transformed by explicit width and height factors.
 * @w_factor: scale factor to apply to width and horizontal offset
 * @h_factor: scale factor to apply to height and vertical offset
 * 
 * Scales layer dimensions and offsets by uniform width and
 * height factors.
 * 
 * Use layer_scale_by_factors() in circumstances when the
 * same width and height scaling factors are to be uniformly
 * applied to a set of layers. In this context, the layer's
 * dimensions and offsets from the sides of the containing
 * image all change by these predetermined factors. By fiat,
 * the fixed point of the transform is the upper left hand
 * corner of the image. Returns gboolean FALSE if a requested
 * scale factor is zero or if a scaling zero's out a layer
 * dimension; returns TRUE otherwise.
 *
 * Use layer_scale() in circumstances where new layer width
 * and height dimensions are predetermined instead.
 *
 * Side effects: Undo set created for layer. Old layer imagery 
 *               scaled & painted to new layer tiles. 
 *
 * Returns: TRUE, if the scaled layer has positive dimensions
 *          FALSE if the scaled layer has at least one zero dimension
 */
gboolean
layer_scale_by_factors (Layer   *layer,
			gdouble  w_factor,
			gdouble  h_factor)
{
  gint new_width, new_height;
  gint new_offset_x, new_offset_y;
  
  if (w_factor == 0.0 || h_factor == 0.0)
    {
      g_message ("layer_scale_by_factors: Error. Requested width or height scale equals zero.");
      return FALSE;
    }

  new_offset_x = 
    (gint) (0.5 + w_factor * (gdouble) GIMP_DRAWABLE (layer)->offset_x);
  new_offset_y = 
    (gint) (0.5 + h_factor * (gdouble) GIMP_DRAWABLE (layer)->offset_y);
  new_width    = 
    (gint) (0.5 + w_factor * (gdouble) GIMP_DRAWABLE (layer)->width);
  new_height   = 
    (gint) (0.5 + h_factor * (gdouble) GIMP_DRAWABLE (layer)->height);

  if (new_width != 0 && new_height != 0)
    {
      layer_scale_lowlevel (layer, 
			    new_width, new_height, new_offset_x, new_offset_y);
      return TRUE;
    }
  else
    return FALSE;
}

/**
 * layer_scale:
 * @layer:        The layer to be transformed by width & height scale factors
 * @new_width:    The width that layer will acquire
 * @new_height:   The height that the layer will acquire
 * @local_origin: sets fixed point of the scaling transform. See below.
 *
 * Sets layer dimensions to new_width and
 * new_height. Derives vertical and horizontal scaling
 * transforms from new width and height. If local_origin is
 * TRUE, the fixed point of the scaling transform coincides
 * with the layer's center point.  Otherwise, the fixed
 * point is taken to be [-GIMP_DRAWABLE(layer)->offset_x,
 * -GIMP_DRAWABLE(layer)->offset_y].
 *
 * Since this function derives scale factors from new and
 * current layer dimensions, these factors will vary from
 * layer to layer because of aliasing artifacts; factor
 * variations among layers can be quite large where layer
 * dimensions approach pixel dimensions. Use 
 * layer_scale_by_factors where constant scales are to
 * be uniformly applied to a number of layers.
 *
 * Side effects: undo set created for layer.
 *               Old layer imagery scaled 
 *               & painted to new layer tiles 
 */
void
layer_scale (Layer   *layer,
	     gint     new_width,
	     gint     new_height,
	     gboolean local_origin)
{
  gint new_offset_x, new_offset_y;

  if (new_width == 0 || new_height == 0)
    {
      g_message ("layer_scale: Error. Requested width or height equals zero.");
      return;
    }
  if (local_origin)
    {
      new_offset_x = GIMP_DRAWABLE(layer)->offset_x + 
	((GIMP_DRAWABLE(layer)->width  - new_width) / 2.0);
      new_offset_y = GIMP_DRAWABLE(layer)->offset_y + 
	((GIMP_DRAWABLE(layer)->height - new_height) / 2.0);
    }
  else
    {
      new_offset_x = (gint)(((gdouble) new_width * 
			     GIMP_DRAWABLE(layer)->offset_x / 
			     (gdouble) GIMP_DRAWABLE(layer)->width));
      new_offset_y = (gint)(((gdouble) new_height * 
			     GIMP_DRAWABLE(layer)->offset_y / 
			     (gdouble) GIMP_DRAWABLE(layer)->height));
    }
  layer_scale_lowlevel (layer, 
			new_width, new_height, 
			new_offset_x, new_offset_y);
}

void
layer_resize (Layer *layer,
	      gint   new_width,
	      gint   new_height,
	      gint   offx,
	      gint   offy)
{
  PixelRegion srcPR, destPR;
  TileManager *new_tiles;
  int w, h;
  int x1, y1, x2, y2;

  if (!new_width || !new_height)
    return;

  x1 = CLAMP (offx, 0, new_width);
  y1 = CLAMP (offy, 0, new_height);
  x2 = CLAMP ((offx + GIMP_DRAWABLE(layer)->width),  0, new_width);
  y2 = CLAMP ((offy + GIMP_DRAWABLE(layer)->height), 0, new_height);
  w = x2 - x1;
  h = y2 - y1;

  if (offx > 0)
    {
      x1 = 0;
      x2 = offx;
    }
  else
    {
      x1 = -offx;
      x2 = 0;
    }

  if (offy > 0)
    {
      y1 = 0;
      y2 = offy;
    }
  else
    {
      y1 = -offy;
      y2 = 0;
    }

  /*  Update the old layer position  */
  drawable_update (GIMP_DRAWABLE(layer),
		   0, 0,
		   GIMP_DRAWABLE(layer)->width, GIMP_DRAWABLE(layer)->height);

  /*  Configure the pixel regions  */
  pixel_region_init (&srcPR, GIMP_DRAWABLE(layer)->tiles, 
		     x1, y1, 
		     w, h, 
		     FALSE);

  /*  Allocate the new layer, configure dest region  */
  new_tiles = tile_manager_new (new_width, new_height, 
				GIMP_DRAWABLE(layer)->bytes);
  pixel_region_init (&destPR, new_tiles, 
		     0, 0, 
		     new_width, new_height, 
		     TRUE);

  /*  fill with the fill color  */
  if (layer_has_alpha (layer))
    {
      /*  Set to transparent and black  */
      unsigned char bg[4] = {0, 0, 0, 0};
      color_region (&destPR, bg);
    }
  else
    {
      unsigned char bg[3];
      gimage_get_background (GIMP_DRAWABLE(layer)->gimage, 
			     GIMP_DRAWABLE(layer), bg);
      color_region (&destPR, bg);
    }
  pixel_region_init (&destPR, new_tiles, 
		     x2, y2, 
		     w, h, 
		     TRUE);

  /*  copy from the old to the new  */
  if (w && h)
    copy_region (&srcPR, &destPR);

  /*  Push the layer on the undo stack  */
  undo_push_layer_mod (GIMP_DRAWABLE(layer)->gimage, layer);

  /*  Configure the new layer  */
  GIMP_DRAWABLE(layer)->tiles = new_tiles;
  GIMP_DRAWABLE(layer)->offset_x = x1 + GIMP_DRAWABLE(layer)->offset_x - x2;
  GIMP_DRAWABLE(layer)->offset_y = y1 + GIMP_DRAWABLE(layer)->offset_y - y2;
  GIMP_DRAWABLE(layer)->width = new_width;
  GIMP_DRAWABLE(layer)->height = new_height;

  /*  If there is a layer mask, make sure it gets resized also  */
  if (layer->mask)
    {
      GIMP_DRAWABLE(layer->mask)->offset_x = GIMP_DRAWABLE(layer)->offset_x;
      GIMP_DRAWABLE(layer->mask)->offset_y = GIMP_DRAWABLE(layer)->offset_y;
      channel_resize (GIMP_CHANNEL (layer->mask),
		      new_width, new_height, offx, offy);
    }

  /*  Make sure we're not caching any old selection info  */
  layer_invalidate_boundary (layer);

  /*  update the new layer area  */
  drawable_update (GIMP_DRAWABLE(layer),
		   0, 0,
		   GIMP_DRAWABLE(layer)->width, GIMP_DRAWABLE(layer)->height);
}

void
layer_resize_to_image (Layer *layer)
{
  GImage *gimage;
  gint offset_x;
  gint offset_y;

  if (!(gimage = GIMP_DRAWABLE (layer)->gimage))
    return;

  undo_push_group_start (gimage, LAYER_RESIZE_UNDO);

  if (layer_is_floating_sel (layer))
    floating_sel_relax (layer, TRUE);

   gimp_drawable_offsets (GIMP_DRAWABLE (layer), &offset_x, &offset_y);
   layer_resize (layer, gimage->width, gimage->height, offset_x, offset_y);

  if (layer_is_floating_sel (layer))
    floating_sel_rigor (layer, TRUE);

  undo_push_group_end (gimage);
}

BoundSeg *
layer_boundary (Layer *layer,
		gint  *num_segs)
{
  BoundSeg *new_segs;

  /*  Create the four boundary segments that encompass this
   *  layer's boundary.
   */
  new_segs = (BoundSeg *) g_malloc (sizeof (BoundSeg) * 4);
  *num_segs = 4;

  /*  if the layer is a floating selection  */
  if (layer_is_floating_sel (layer))
    {
      /*  if the owner drawable is a channel, just return nothing  */
      if (GIMP_IS_CHANNEL (layer->fs.drawable))
	{
	  *num_segs = 0;
	  return NULL;
	}
      /*  otherwise, set the layer to the owner drawable  */
      else
	layer = GIMP_LAYER (layer->fs.drawable);
    }

  new_segs[0].x1 = GIMP_DRAWABLE(layer)->offset_x;
  new_segs[0].y1 = GIMP_DRAWABLE(layer)->offset_y;
  new_segs[0].x2 = GIMP_DRAWABLE(layer)->offset_x;
  new_segs[0].y2 = GIMP_DRAWABLE(layer)->offset_y + GIMP_DRAWABLE(layer)->height;
  new_segs[0].open = 1;

  new_segs[1].x1 = GIMP_DRAWABLE(layer)->offset_x;
  new_segs[1].y1 = GIMP_DRAWABLE(layer)->offset_y;
  new_segs[1].x2 = GIMP_DRAWABLE(layer)->offset_x + GIMP_DRAWABLE(layer)->width;
  new_segs[1].y2 = GIMP_DRAWABLE(layer)->offset_y;
  new_segs[1].open = 1;

  new_segs[2].x1 = GIMP_DRAWABLE(layer)->offset_x + GIMP_DRAWABLE(layer)->width;
  new_segs[2].y1 = GIMP_DRAWABLE(layer)->offset_y;
  new_segs[2].x2 = GIMP_DRAWABLE(layer)->offset_x + GIMP_DRAWABLE(layer)->width;
  new_segs[2].y2 = GIMP_DRAWABLE(layer)->offset_y + GIMP_DRAWABLE(layer)->height;
  new_segs[2].open = 0;

  new_segs[3].x1 = GIMP_DRAWABLE(layer)->offset_x;
  new_segs[3].y1 = GIMP_DRAWABLE(layer)->offset_y + GIMP_DRAWABLE(layer)->height;
  new_segs[3].x2 = GIMP_DRAWABLE(layer)->offset_x + GIMP_DRAWABLE(layer)->width;
  new_segs[3].y2 = GIMP_DRAWABLE(layer)->offset_y + GIMP_DRAWABLE(layer)->height;
  new_segs[3].open = 0;

  return new_segs;
}

void
layer_invalidate_boundary (Layer *layer)
{
  GImage *gimage;
  Channel *mask;

  /*  first get the selection mask channel  */
  if (! (gimage = drawable_gimage (GIMP_DRAWABLE (layer))))
    return;

  /*  Turn the current selection off  */
  gdisplays_selection_visibility (gimage, SelectionOff);

  /*  clear the affected region surrounding the layer  */
  gdisplays_selection_visibility (gimage, SelectionLayerOff); 

  mask = gimage_get_mask (gimage);

  /*  Only bother with the bounds if there is a selection  */
  if (! channel_is_empty (mask))
    {
      mask->bounds_known   = FALSE;
      mask->boundary_known = FALSE;
    }

  if (layer_is_floating_sel(layer))
      floating_sel_invalidate(layer);
}

gint
layer_pick_correlate (Layer *layer,
		      gint   x,
		      gint   y)
{
  Tile *tile;
  Tile *mask_tile;
  int val;

  /*  Is the point inside the layer?
   *  First transform the point to layer coordinates...
   */
  x -= GIMP_DRAWABLE(layer)->offset_x;
  y -= GIMP_DRAWABLE(layer)->offset_y;
  if (x >= 0 && x < GIMP_DRAWABLE(layer)->width &&
      y >= 0 && y < GIMP_DRAWABLE(layer)->height &&
      GIMP_DRAWABLE(layer)->visible)
    {
      /*  If the point is inside, and the layer has no
       *  alpha channel, success!
       */
      if (! layer_has_alpha (layer))
	return TRUE;

      /*  Otherwise, determine if the alpha value at
       *  the given point is non-zero
       */
      tile = tile_manager_get_tile (GIMP_DRAWABLE(layer)->tiles, 
				    x, y, TRUE, FALSE);

      val = * ((unsigned char*) tile_data_pointer (tile,
						   x % TILE_WIDTH,
						   y % TILE_HEIGHT) +
				tile_bpp (tile) - 1);
      if (layer->mask)
	{
	  unsigned char *ptr;
	  mask_tile = tile_manager_get_tile (GIMP_DRAWABLE(layer->mask)->tiles,
					     x, y, TRUE, FALSE);
	  ptr = tile_data_pointer (mask_tile, x % TILE_WIDTH, y % TILE_HEIGHT);
	  val = val * (*ptr) / 255;
	  tile_release (mask_tile, FALSE);
	}

      tile_release (tile, FALSE);

      if (val > 63)
	return TRUE;
    }

  return FALSE;
}

/**********************/
/*  access functions  */
/**********************/

void
layer_set_name (Layer *layer,
		gchar *name)
{
  gimp_drawable_set_name (GIMP_DRAWABLE (layer), name);
}

gchar *
layer_get_name (Layer *layer)
{
  return gimp_drawable_get_name (GIMP_DRAWABLE (layer));
}

guchar *
layer_data (Layer *layer)
{
  return NULL;
}

LayerMask *
layer_get_mask (Layer *layer)
{
  return layer->mask;
}

gboolean
layer_has_alpha (Layer *layer)
{
  g_return_val_if_fail (layer != NULL, FALSE);
  g_return_val_if_fail (GIMP_IS_LAYER (layer), FALSE);

  return GIMP_IMAGE_TYPE_HAS_ALPHA (GIMP_DRAWABLE (layer)->type);
}

gboolean
layer_is_floating_sel (Layer *layer)
{
  if (layer != NULL && layer->fs.drawable != NULL)
    return TRUE;
  else
    return FALSE;
}

gboolean
layer_linked (Layer *layer)
{
  return layer->linked;
}

TempBuf *
layer_preview (Layer *layer,
	       gint   width,
	       gint   height)
{
  GImage            *gimage;
  TempBuf           *preview_buf;
  PixelRegion        srcPR, destPR;
  GimpImageBaseType  type;
  gint               bytes;
  gint               subsample;
  TempBuf           *ret_buf;

  g_return_val_if_fail (layer != NULL, NULL);
  g_return_val_if_fail (GIMP_IS_LAYER (layer), NULL);

  type  = RGB;
  bytes = 0;

  /*  The easy way  */
  if (GIMP_DRAWABLE (layer)->preview_valid &&
      (ret_buf = 
       gimp_preview_cache_get (&(GIMP_DRAWABLE (layer)->preview_cache), 
			       width, height)))
    return ret_buf;
  /*  The hard way  */
  else
    {
      gimage = GIMP_DRAWABLE (layer)->gimage;
      switch (GIMP_DRAWABLE (layer)->type)
	{
	case RGB_GIMAGE: case RGBA_GIMAGE:
	  type  = RGB;
	  bytes = GIMP_DRAWABLE (layer)->bytes;
	  break;
	case GRAY_GIMAGE: case GRAYA_GIMAGE:
	  type  = GRAY;
	  bytes = GIMP_DRAWABLE (layer)->bytes;
	  break;
	case INDEXED_GIMAGE: case INDEXEDA_GIMAGE:
	  type  = INDEXED;
	  bytes = (GIMP_DRAWABLE (layer)->type == INDEXED_GIMAGE) ? 3 : 4;
	  break;
	}

      /*  calculate 'acceptable' subsample  */
      subsample = 1;
      /* handle some truncation errors */
      if (width < 1) width = 1;
      if (height < 1) height = 1;

      while ((width * (subsample + 1) * 2 < GIMP_DRAWABLE (layer)->width) &&
	     (height * (subsample + 1) * 2 < GIMP_DRAWABLE (layer)->height)) 
	subsample = subsample + 1;

      pixel_region_init (&srcPR, GIMP_DRAWABLE (layer)->tiles, 
			 0, 0,
			 GIMP_DRAWABLE (layer)->width, 
			 GIMP_DRAWABLE (layer)->height, 
			 FALSE);

      preview_buf = temp_buf_new (width, height, bytes, 0, 0, NULL);

      destPR.bytes     = preview_buf->bytes;
      destPR.w         = width;
      destPR.h         = height;
      destPR.rowstride = width * destPR.bytes;
      destPR.data      = temp_buf_data (preview_buf);

      layer_preview_scale (type, gimage->cmap, &srcPR, &destPR, subsample);

      if (!GIMP_DRAWABLE (layer)->preview_valid)
	gimp_preview_cache_invalidate (&(GIMP_DRAWABLE(layer)->preview_cache));

      GIMP_DRAWABLE (layer)->preview_valid = TRUE;

      gimp_preview_cache_add (&(GIMP_DRAWABLE (layer)->preview_cache), 
			      preview_buf);

      return preview_buf;
    }
}

TempBuf *
layer_mask_preview (Layer *layer,
		    gint   width,
		    gint   height)
{
  TempBuf     *preview_buf;
  LayerMask   *mask;
  PixelRegion  srcPR, destPR;
  gint         subsample;
  TempBuf     *ret_buf;

  g_return_val_if_fail (layer != NULL, NULL);
  g_return_val_if_fail (GIMP_IS_LAYER (layer), NULL);

  mask = layer->mask;
  if (!mask)
    return NULL;

  /*  The easy way  */
  if (GIMP_DRAWABLE(mask)->preview_valid &&
      (ret_buf = gimp_preview_cache_get (&(GIMP_DRAWABLE(mask)->preview_cache),
					 width, height)))
    return ret_buf;
  /*  The hard way  */
  else
    {
      /*  calculate 'acceptable' subsample  */
      subsample = 1;
      if (width < 1) width = 1;
      if (height < 1) height = 1;
      while ((width * (subsample + 1) * 2 < GIMP_DRAWABLE(layer)->width) &&
	     (height * (subsample + 1) * 2 < GIMP_DRAWABLE(layer)->height))
	subsample = subsample + 1;

      pixel_region_init (&srcPR, GIMP_DRAWABLE(mask)->tiles, 
			 0, 0, 
			 GIMP_DRAWABLE(mask)->width, 
			 GIMP_DRAWABLE(mask)->height, 
			 FALSE);

      preview_buf = temp_buf_new (width, height, 1, 0, 0, NULL);

      destPR.bytes     = preview_buf->bytes;
      destPR.w         = width;
      destPR.h         = height;
      destPR.rowstride = width * destPR.bytes;
      destPR.data      = temp_buf_data (preview_buf);

      layer_preview_scale (GRAY, NULL, &srcPR, &destPR, subsample);

      if (!GIMP_DRAWABLE (mask)->preview_valid)
	gimp_preview_cache_invalidate (&(GIMP_DRAWABLE (mask)->preview_cache));

      GIMP_DRAWABLE (mask)->preview_valid = TRUE;
      gimp_preview_cache_add (&(GIMP_DRAWABLE (mask)->preview_cache),
			      preview_buf);

      return preview_buf;
    }
}

Tattoo
layer_get_tattoo (const Layer *layer)
{
  return (gimp_drawable_get_tattoo (GIMP_DRAWABLE (layer)));
}

void
layer_set_tattoo (const Layer *layer, 
		  Tattoo       value)
{
  gimp_drawable_set_tattoo (GIMP_DRAWABLE (layer), value);
}


void
layer_invalidate_previews (GimpImage *gimage)
{
  GSList *tmp;
  Layer  *layer;

  g_return_if_fail (gimage != NULL);

  for (tmp = gimage->layers; tmp; tmp = g_slist_next (tmp))
    {
      layer = (Layer *) tmp->data;
      gimp_drawable_invalidate_preview (GIMP_DRAWABLE(layer), TRUE);
    }
}

static void
layer_preview_scale (GimpImageBaseType  type,
		     guchar            *cmap,
		     PixelRegion       *srcPR,
		     PixelRegion       *destPR,
		     gint               subsample)
{
#define EPSILON 0.000001
  guchar  *src, *s;
  guchar  *dest, *d;
  gdouble *row, *r;
  gint     destwidth;
  gint     src_row, src_col;
  gint     bytes, b;
  gint     width, height;
  gint     orig_width, orig_height;
  gdouble  x_rat, y_rat;
  gdouble  x_cum, y_cum;
  gdouble  x_last, y_last;
  gdouble *x_frac, y_frac, tot_frac;
  gint     i, j;
  gint     frac;
  gboolean advance_dest;
  guchar   rgb[MAX_CHANNELS];

  orig_width  = srcPR->w / subsample;
  orig_height = srcPR->h / subsample;
  width       = destPR->w;
  height      = destPR->h;

  /*  Some calculations...  */
  bytes     = destPR->bytes;
  destwidth = destPR->rowstride;

  /*  the data pointers...  */
  src  = g_new (guchar, orig_width * bytes);
  dest = destPR->data;

  /*  find the ratios of old x to new x and old y to new y  */
  x_rat = (gdouble) orig_width  / (gdouble) width;
  y_rat = (gdouble) orig_height / (gdouble) height;

  /*  allocate an array to help with the calculations  */
  row    = g_new (gdouble, width * bytes);
  x_frac = g_new (gdouble, width + orig_width);

  /*  initialize the pre-calculated pixel fraction array  */
  src_col = 0;
  x_cum = (gdouble) src_col;
  x_last = x_cum;

  for (i = 0; i < width + orig_width; i++)
    {
      if (x_cum + x_rat <= (src_col + 1 + EPSILON))
	{
	  x_cum += x_rat;
	  x_frac[i] = x_cum - x_last;
	}
      else
	{
	  src_col ++;
	  x_frac[i] = src_col - x_last;
	}
      x_last += x_frac[i];
    }

  /*  clear the "row" array  */
  memset (row, 0, sizeof (gdouble) * width * bytes);

  /*  counters...  */
  src_row = 0;
  y_cum = (double) src_row;
  y_last = y_cum;

  pixel_region_get_row (srcPR, 
			0, 
			src_row * subsample, 
			orig_width * subsample, 
			src, 
			subsample);

  /*  Scale the selected region  */
  for (i = 0; i < height; )
    {
      src_col = 0;
      x_cum = (gdouble) src_col;

      /* determine the fraction of the src pixel we are using for y */
      if (y_cum + y_rat <= (src_row + 1 + EPSILON))
	{
	  y_cum += y_rat;
	  y_frac = y_cum - y_last;
	  advance_dest = TRUE;
	}
      else
	{
	  src_row ++;
	  y_frac = src_row - y_last;
	  advance_dest = FALSE;
	}

      y_last += y_frac;

      s = src;
      r = row;

      frac = 0;
      j = width;

      while (j)
	{
	  tot_frac = x_frac[frac++] * y_frac;

	  /*  If indexed, transform the color to RGB  */
	  if (type == INDEXED)
	    {
	      map_to_color (2, cmap, s, rgb);

	      r[RED_PIX] += rgb[RED_PIX] * tot_frac;
	      r[GREEN_PIX] += rgb[GREEN_PIX] * tot_frac;
	      r[BLUE_PIX] += rgb[BLUE_PIX] * tot_frac;
	      if (bytes == 4)
		r[ALPHA_PIX] += s[ALPHA_I_PIX] * tot_frac;
	    }
	  else
	    for (b = 0; b < bytes; b++)
	      r[b] += s[b] * tot_frac;

	  /*  increment the destination  */
	  if (x_cum + x_rat <= (src_col + 1 + EPSILON))
	    {
	      r += bytes;
	      x_cum += x_rat;
	      j--;
	    }

	  /* increment the source */
	  else
	    {
	      s += srcPR->bytes;
	      src_col++;
	    }
	}

      if (advance_dest)
	{
	  tot_frac = 1.0 / (x_rat * y_rat);

	  /*  copy "row" to "dest"  */
	  d = dest;
	  r = row;

	  j = width;
	  while (j--)
	    {
	      b = bytes;
	      while (b--)
		*d++ = (guchar) ((*r++ * tot_frac)+0.5);
	    }

	  dest += destwidth;

	  /*  clear the "row" array  */
	  memset (row, 0, sizeof (gdouble) * destwidth);

	  i++;
	}
      else
	pixel_region_get_row (srcPR, 
			      0, 
			      src_row * subsample, 
			      orig_width * subsample, 
			      src, 
			      subsample);
    }

  /*  free up temporary arrays  */
  g_free (row);
  g_free (x_frac);
  g_free (src);
}

static void
gimp_layer_mask_destroy (GtkObject *object)
{
  GimpLayerMask *layermask;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GIMP_IS_LAYER_MASK (object));

  layermask = GIMP_LAYER_MASK (object);

  if (GTK_OBJECT_CLASS (layer_mask_parent_class)->destroy)
    (*GTK_OBJECT_CLASS (layer_mask_parent_class)->destroy) (object);
}

LayerMask *
layer_mask_new (GimpImage *gimage,
		gint       width,
		gint       height,
		gchar     *name,
		gint       opacity,
		guchar    *col)
{
  LayerMask *layer_mask;
  gint       i;

  layer_mask = gtk_type_new (gimp_layer_mask_get_type ());

  gimp_drawable_configure (GIMP_DRAWABLE (layer_mask), 
			   gimage, width, height, GRAY_GIMAGE, name);

  /*  set the layer_mask color and opacity  */
  for (i = 0; i < 3; i++)
    GIMP_CHANNEL (layer_mask)->col[i]       = col[i];

  GIMP_CHANNEL (layer_mask)->opacity        = opacity;
  GIMP_CHANNEL (layer_mask)->show_masked    = TRUE;

  /*  selection mask variables  */
  GIMP_CHANNEL (layer_mask)->empty          = TRUE;
  GIMP_CHANNEL (layer_mask)->segs_in        = NULL;
  GIMP_CHANNEL (layer_mask)->segs_out       = NULL;
  GIMP_CHANNEL (layer_mask)->num_segs_in    = 0;
  GIMP_CHANNEL (layer_mask)->num_segs_out   = 0;
  GIMP_CHANNEL (layer_mask)->bounds_known   = TRUE;
  GIMP_CHANNEL (layer_mask)->boundary_known = TRUE;
  GIMP_CHANNEL (layer_mask)->x1             = GIMP_CHANNEL (layer_mask)->y1 = 0;
  GIMP_CHANNEL (layer_mask)->x2             = width;
  GIMP_CHANNEL (layer_mask)->y2             = height;

  return layer_mask;
}

LayerMask *
layer_mask_copy (LayerMask *layer_mask)
{
  gchar       *layer_mask_name;
  LayerMask   *new_layer_mask;
  PixelRegion  srcPR, destPR;

  /*  formulate the new layer_mask name  */
  layer_mask_name = g_strdup_printf (_("%s copy"), 
				     GIMP_DRAWABLE(layer_mask)->name);

  /*  allocate a new layer_mask object  */
  new_layer_mask = layer_mask_new (GIMP_DRAWABLE(layer_mask)->gimage, 
				   GIMP_DRAWABLE(layer_mask)->width, 
				   GIMP_DRAWABLE(layer_mask)->height, 
				   layer_mask_name, 
				   GIMP_CHANNEL(layer_mask)->opacity, 
				   GIMP_CHANNEL(layer_mask)->col);
  GIMP_DRAWABLE(new_layer_mask)->visible = 
    GIMP_DRAWABLE(layer_mask)->visible;
  GIMP_DRAWABLE(new_layer_mask)->offset_x = 
    GIMP_DRAWABLE(layer_mask)->offset_x;
  GIMP_DRAWABLE(new_layer_mask)->offset_y = 
    GIMP_DRAWABLE(layer_mask)->offset_y;
  GIMP_CHANNEL(new_layer_mask)->show_masked = 
    GIMP_CHANNEL(layer_mask)->show_masked;

  /*  copy the contents across layer masks  */
  pixel_region_init (&srcPR, GIMP_DRAWABLE(layer_mask)->tiles, 
		     0, 0, 
		     GIMP_DRAWABLE(layer_mask)->width, 
		     GIMP_DRAWABLE(layer_mask)->height, 
		     FALSE);
  pixel_region_init (&destPR, GIMP_DRAWABLE(new_layer_mask)->tiles, 
		     0, 0, 
		     GIMP_DRAWABLE(layer_mask)->width, 
		     GIMP_DRAWABLE(layer_mask)->height, 
		     TRUE);
  copy_region (&srcPR, &destPR);

  /*  free up the layer_mask_name memory  */
  g_free (layer_mask_name);

  return new_layer_mask;
}

LayerMask *
layer_mask_get_ID (gint ID)
{
  GimpDrawable *drawable;

  drawable = drawable_get_ID (ID);

  if (drawable && GIMP_IS_LAYER_MASK (drawable)) 
    return GIMP_LAYER_MASK (drawable);
  else
    return NULL;
}

void
layer_mask_delete (LayerMask *layermask)
{
  gtk_object_unref (GTK_OBJECT (layermask));
}

LayerMask *
layer_mask_ref (LayerMask *mask)
{
  gtk_object_ref  (GTK_OBJECT (mask));
  gtk_object_sink (GTK_OBJECT (mask));

  return mask;
}

void
layer_mask_unref (LayerMask *mask)
{
  gtk_object_unref (GTK_OBJECT (mask));
}

void
layer_mask_set_layer (LayerMask *mask,
		      Layer     *layer)
{
  mask->layer = layer;
}

Layer *
layer_mask_get_layer (LayerMask *mask)
{
  return mask->layer;
}

void
channel_layer_mask (Channel *mask,
		    Layer   *layer)
{
  PixelRegion srcPR, destPR;
  guchar      empty = 0;
  gint        x1, y1, x2, y2;

  /*  push the current mask onto the undo stack  */
  channel_push_undo (mask);

  /*  clear the mask  */
  pixel_region_init (&destPR, GIMP_DRAWABLE(mask)->tiles, 
		     0, 0, 
		     GIMP_DRAWABLE(mask)->width, GIMP_DRAWABLE(mask)->height, 
		     TRUE);
  color_region (&destPR, &empty);

  x1 = CLAMP (GIMP_DRAWABLE(layer)->offset_x, 0, GIMP_DRAWABLE(mask)->width);
  y1 = CLAMP (GIMP_DRAWABLE(layer)->offset_y, 0, GIMP_DRAWABLE(mask)->height);
  x2 = CLAMP (GIMP_DRAWABLE(layer)->offset_x + GIMP_DRAWABLE(layer)->width, 
	      0, GIMP_DRAWABLE(mask)->width);
  y2 = CLAMP (GIMP_DRAWABLE(layer)->offset_y + GIMP_DRAWABLE(layer)->height, 
	      0, GIMP_DRAWABLE(mask)->height);

  pixel_region_init (&srcPR, GIMP_DRAWABLE(layer->mask)->tiles,
		     (x1 - GIMP_DRAWABLE(layer)->offset_x), 
		     (y1 - GIMP_DRAWABLE(layer)->offset_y),
		     (x2 - x1), (y2 - y1), 
		     FALSE);
  pixel_region_init (&destPR, GIMP_DRAWABLE(mask)->tiles, 
		     x1, y1, 
		     (x2 - x1), (y2 - y1), 
		     TRUE);
  copy_region (&srcPR, &destPR);

  mask->bounds_known = FALSE;
}
