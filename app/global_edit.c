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

#include <gtk/gtk.h>

#include "libgimpwidgets/gimpwidgets.h"

#include "apptypes.h"

#include "appenv.h"
#include "drawable.h"
#include "image_new.h"
#include "floating_sel.h"
#include "gdisplay.h"
#include "gimage.h"
#include "gimage_mask.h"
#include "gimpcontext.h"
#include "gimplayermask.h"
#include "global_edit.h"
#include "layer.h"
#include "paint_funcs.h"
#include "pixel_region.h"
#include "tile_manager.h"
#include "undo.h"

#include "tools/tools.h"

#include "libgimp/gimpintl.h"


typedef enum
{
  PASTE,
  PASTE_INTO,
  PASTE_AS_NEW
} PasteAction;  

/*  The named paste dialog  */
typedef struct _PasteNamedDlg PasteNamedDlg;

struct _PasteNamedDlg
{
  GtkWidget   *shell;
  GtkWidget   *list;
  GDisplay    *gdisp;
  PasteAction  action;
};

/*  The named buffer structure...  */
typedef struct _NamedBuffer NamedBuffer;

struct _NamedBuffer
{
  TileManager *buf;
  gchar       *name;
};


/*  The named buffer list  */
static GSList *named_buffers = NULL;

/*  The global edit buffer  */
TileManager   *global_buf = NULL;


/*  Crop the buffer to the size of pixels with non-zero transparency */

TileManager *
crop_buffer (TileManager *tiles,
	     gboolean     border)
{
  PixelRegion  PR;
  TileManager *new_tiles;
  gint         bytes, alpha;
  guchar      *data;
  gint         empty;
  gint         x1, y1, x2, y2;
  gint         x, y;
  gint         ex, ey;
  gint         found;
  void        *pr;
  guchar       black[MAX_CHANNELS] = { 0, 0, 0, 0 };

  bytes = tile_manager_bpp (tiles);
  alpha = bytes - 1;

  /*  go through and calculate the bounds  */
  x1 = tile_manager_width (tiles);
  y1 = tile_manager_height (tiles);
  x2 = 0;
  y2 = 0;

  pixel_region_init (&PR, tiles, 0, 0, x1, y1, FALSE);
  for (pr = pixel_regions_register (1, &PR); 
       pr != NULL; 
       pr = pixel_regions_process (pr))
    {
      data = PR.data + alpha;
      ex = PR.x + PR.w;
      ey = PR.y + PR.h;

      for (y = PR.y; y < ey; y++)
	{
	  found = FALSE;
	  for (x = PR.x; x < ex; x++, data+=bytes)
	    if (*data)
	      {
		if (x < x1)
		  x1 = x;
		if (x > x2)
		  x2 = x;
		found = TRUE;
	      }
	  if (found)
	    {
	      if (y < y1)
		y1 = y;
	      if (y > y2)
		y2 = y;
	    }
	}
    }

  x2 = CLAMP (x2 + 1, 0, tile_manager_width (tiles));
  y2 = CLAMP (y2 + 1, 0, tile_manager_height (tiles));

  empty = (x1 == tile_manager_width (tiles) && 
	   y1 == tile_manager_height (tiles));

  /*  If there are no visible pixels, return NULL */
  if (empty)
    {
      new_tiles = NULL;
    }
  /*  If no cropping, return original buffer  */
  else if (x1 == 0 && y1 == 0 && 
	   x2 == tile_manager_width (tiles)  &&
	   y2 == tile_manager_height (tiles) && 
	   border == 0)
    {
      new_tiles = tiles;
    }
  /*  Otherwise, crop the original area  */
  else
    {
      PixelRegion srcPR, destPR;
      int new_width, new_height;

      new_width = (x2 - x1) + border * 2;
      new_height = (y2 - y1) + border * 2;
      new_tiles = tile_manager_new (new_width, new_height, bytes);

      /*  If there is a border, make sure to clear the new tiles first  */
      if (border)
	{
	  pixel_region_init (&destPR, new_tiles, 0, 0, new_width, border, TRUE);
	  color_region (&destPR, black);
	  pixel_region_init (&destPR, new_tiles, 0, border, border, (y2 - y1), TRUE);
	  color_region (&destPR, black);
	  pixel_region_init (&destPR, new_tiles, new_width - border, border, border, (y2 - y1), TRUE);
	  color_region (&destPR, black);
	  pixel_region_init (&destPR, new_tiles, 0, new_height - border, new_width, border, TRUE);
	  color_region (&destPR, black);
	}

      pixel_region_init (&srcPR, tiles, 
			 x1, y1, (x2 - x1), (y2 - y1), FALSE);
      pixel_region_init (&destPR, new_tiles, 
			 border, border, (x2 - x1), (y2 - y1), TRUE);

      copy_region (&srcPR, &destPR);

      tile_manager_set_offsets (new_tiles, x1, y1);
    }

  return new_tiles;
}

TileManager *
edit_cut (GImage       *gimage,
	  GimpDrawable *drawable)
{
  TileManager *cut;
  TileManager *cropped_cut;
  gint         empty;

  if (!gimage || drawable == NULL)
    return NULL;

  /*  Start a group undo  */
  undo_push_group_start (gimage, EDIT_CUT_UNDO);

  /*  See if the gimage mask is empty  */
  empty = gimage_mask_is_empty (gimage);

  /*  Next, cut the mask portion from the gimage  */
  cut = gimage_mask_extract (gimage, drawable, TRUE, FALSE, TRUE);

  /*  Only crop if the gimage mask wasn't empty  */
  if (cut && empty == FALSE)
    {
      cropped_cut = crop_buffer (cut, 0);

      if (cropped_cut != cut)
	tile_manager_destroy (cut);
    }
  else if (cut)
    cropped_cut = cut;
  else
    cropped_cut = NULL;

  if (cut)
    image_new_reset_current_cut_buffer ();


  /*  end the group undo  */
  undo_push_group_end (gimage);

  if (cropped_cut)
    {
      /*  Free the old global edit buffer  */
      if (global_buf)
	tile_manager_destroy (global_buf);
      /*  Set the global edit buffer  */
      global_buf = cropped_cut;

      return cropped_cut;
    }
  else
    return NULL;
}

TileManager *
edit_copy (GImage       *gimage,
	   GimpDrawable *drawable)
{
  TileManager *copy;
  TileManager *cropped_copy;
  gint         empty;

  if (!gimage || drawable == NULL)
    return NULL;

  /*  See if the gimage mask is empty  */
  empty = gimage_mask_is_empty (gimage);

  /*  First, copy the masked portion of the gimage  */
  copy = gimage_mask_extract (gimage, drawable, FALSE, FALSE, TRUE);

  /*  Only crop if the gimage mask wasn't empty  */
  if (copy && empty == FALSE)
    {
      cropped_copy = crop_buffer (copy, 0);

      if (cropped_copy != copy)
	tile_manager_destroy (copy);
    }
  else if (copy)
    cropped_copy = copy;
  else
    cropped_copy = NULL;

  if(copy)
    image_new_reset_current_cut_buffer();


  if (cropped_copy)
    {
      /*  Free the old global edit buffer  */
      if (global_buf)
	tile_manager_destroy (global_buf);
      /*  Set the global edit buffer  */
      global_buf = cropped_copy;

      return cropped_copy;
    }
  else
    return NULL;
}

GimpLayer *
edit_paste (GImage       *gimage,
	    GimpDrawable *drawable,
	    TileManager  *paste,
	    gboolean      paste_into)
{
  Layer *layer;
  gint   x1, y1, x2, y2;
  gint   cx, cy;

  /*  Make a new layer: iff drawable == NULL,
   *  user is pasting into an empty display.
   */

  if (drawable != NULL)
    layer = layer_new_from_tiles (gimage,
				  gimp_drawable_type_with_alpha (drawable),
				  paste, 
				  _("Pasted Layer"),
				  OPAQUE_OPACITY, NORMAL_MODE);
  else
    layer = layer_new_from_tiles (gimage,
				  gimp_image_base_type_with_alpha (gimage),
				  paste, 
				  _("Pasted Layer"),
				  OPAQUE_OPACITY, NORMAL_MODE);
 
  if (layer)
    {
      /*  Start a group undo  */
      undo_push_group_start (gimage, EDIT_PASTE_UNDO);

      /*  Set the offsets to the center of the image  */
      if (drawable != NULL)
	{
	  gimp_drawable_offsets (drawable, &cx, &cy);
	  gimp_drawable_mask_bounds (drawable, &x1, &y1, &x2, &y2);
	  cx += (x1 + x2) >> 1;
	  cy += (y1 + y2) >> 1;
	}
      else
	{
	  cx = gimage->width >> 1;
	  cy = gimage->height >> 1;
	}

      GIMP_DRAWABLE (layer)->offset_x = cx - (GIMP_DRAWABLE (layer)->width >> 1);
      GIMP_DRAWABLE (layer)->offset_y = cy - (GIMP_DRAWABLE (layer)->height >> 1);

      /*  If there is a selection mask clear it--
       *  this might not always be desired, but in general,
       *  it seems like the correct behavior.
       */
      if (! gimage_mask_is_empty (gimage) && !paste_into)
	channel_clear (gimp_image_get_mask (gimage));

      /*  if there's a drawable, add a new floating selection  */
      if (drawable != NULL)
	{
	  floating_sel_attach (layer, drawable);
	}
      else
	{
	  gimp_drawable_set_gimage (GIMP_DRAWABLE (layer), gimage);
	  gimp_image_add_layer (gimage, layer, 0);
	}
      
     /*  end the group undo  */
      undo_push_group_end (gimage);

      return layer;
    }

  return NULL;
}

gboolean
edit_paste_as_new (GImage      *invoke,
		   TileManager *paste)
{
  GImage   *gimage;
  Layer    *layer;
  GDisplay *gdisp;

  if (!global_buf)
    return FALSE;

  /*  create a new image  (always of type RGB)  */
  gimage = gimage_new (tile_manager_width (paste), 
		       tile_manager_height (paste), 
		       RGB);
  gimp_image_undo_disable (gimage);
  gimp_image_set_resolution (gimage, invoke->xresolution, invoke->yresolution);
  gimp_image_set_unit (gimage, invoke->unit);
  
  layer = layer_new_from_tiles (gimage,
				gimp_image_base_type_with_alpha (gimage),
				paste, 
				_("Pasted Layer"),
				OPAQUE_OPACITY, NORMAL_MODE);

  if (layer)
    {
      /*  add the new layer to the image  */
      gimp_drawable_set_gimage (GIMP_DRAWABLE (layer), gimage);
      gimp_image_add_layer (gimage, layer, 0);

      gimp_image_undo_enable (gimage);

      gdisp = gdisplay_new (gimage, 0x0101);
      gimp_context_set_display (gimp_context_get_user (), gdisp);

      return TRUE;
    }

  return FALSE;			       
}

gboolean
edit_clear (GImage       *gimage,
	    GimpDrawable *drawable)
{
  TileManager *buf_tiles;
  PixelRegion  bufPR;
  gint         x1, y1, x2, y2;
  guchar       col[MAX_CHANNELS];

  if (!gimage || drawable == NULL)
    return FALSE;

  gimp_image_get_background (gimage, drawable, col);
  if (gimp_drawable_has_alpha (drawable))
    col [gimp_drawable_bytes (drawable) - 1] = OPAQUE_OPACITY;

  gimp_drawable_mask_bounds (drawable, &x1, &y1, &x2, &y2);

  if (!(x2 - x1) || !(y2 - y1))
    return TRUE;  /*  nothing to do, but the clear succeded  */

  buf_tiles = tile_manager_new ((x2 - x1), (y2 - y1),
				gimp_drawable_bytes (drawable));
  pixel_region_init (&bufPR, buf_tiles, 0, 0, (x2 - x1), (y2 - y1), TRUE);
  color_region (&bufPR, col);

  pixel_region_init (&bufPR, buf_tiles, 0, 0, (x2 - x1), (y2 - y1), FALSE);
  gimp_image_apply_image (gimage, drawable, &bufPR, 1, OPAQUE_OPACITY,
			  ERASE_MODE, NULL, x1, y1);

  /*  update the image  */
  drawable_update (drawable, x1, y1, (x2 - x1), (y2 - y1));

  /*  free the temporary tiles  */
  tile_manager_destroy (buf_tiles);

  return TRUE;
}

gboolean
edit_fill (GImage       *gimage,
	   GimpDrawable *drawable,
	   GimpFillType  fill_type)
{
  TileManager *buf_tiles;
  PixelRegion  bufPR;
  gint         x1, y1, x2, y2;
  guchar       col[MAX_CHANNELS];

  if (!gimage || drawable == NULL)
    return FALSE;

  if (gimp_drawable_has_alpha (drawable))
    col [gimp_drawable_bytes (drawable) - 1] = OPAQUE_OPACITY;

  switch (fill_type)
    {
    case FOREGROUND_FILL:
      gimp_image_get_foreground (gimage, drawable, col);
      break;

    case BACKGROUND_FILL:
      gimp_image_get_background (gimage, drawable, col);
      break;

    case WHITE_FILL:
      col[RED_PIX] = 255;
      col[GREEN_PIX] = 255;
      col[BLUE_PIX] = 255;
      break;

    case TRANSPARENT_FILL:
      col[RED_PIX] = 0;
      col[GREEN_PIX] = 0;
      col[BLUE_PIX] = 0;
      if (gimp_drawable_has_alpha (drawable))
	col [gimp_drawable_bytes (drawable) - 1] = TRANSPARENT_OPACITY;
      break;

    case NO_FILL:
      return TRUE;  /*  nothing to do, but the fill succeded  */

    default:
      g_warning ("unknown fill type");
      gimp_image_get_background (gimage, drawable, col);
      break;
    }

  gimp_drawable_mask_bounds (drawable, &x1, &y1, &x2, &y2);

  if (!(x2 - x1) || !(y2 - y1))
    return TRUE;  /*  nothing to do, but the fill succeded  */
 
  buf_tiles = tile_manager_new ((x2 - x1), (y2 - y1),
				gimp_drawable_bytes (drawable));
  pixel_region_init (&bufPR, buf_tiles, 0, 0, (x2 - x1), (y2 - y1), TRUE);
  color_region (&bufPR, col);

  pixel_region_init (&bufPR, buf_tiles, 0, 0, (x2 - x1), (y2 - y1), FALSE);
  gimp_image_apply_image (gimage, drawable, &bufPR, 1, OPAQUE_OPACITY,
			  NORMAL_MODE, NULL, x1, y1);

  /*  update the image  */
  drawable_update (drawable, x1, y1, (x2 - x1), (y2 - y1));

  /*  free the temporary tiles  */
  tile_manager_destroy (buf_tiles);

  return TRUE;
}

gboolean
global_edit_cut (GDisplay *gdisp)
{
  /*  stop any active tool  */
  active_tool_control (HALT, gdisp);

  if (!edit_cut (gdisp->gimage, gimp_image_active_drawable (gdisp->gimage)))
    return FALSE;

  /*  flush the display  */
  gdisplays_flush ();

  return TRUE;
}

gboolean
global_edit_copy (GDisplay *gdisp)
{
  if (!edit_copy (gdisp->gimage, gimp_image_active_drawable (gdisp->gimage)))
    return FALSE;

  return TRUE;
}

gboolean
global_edit_paste (GDisplay *gdisp,
		   gboolean  paste_into)
{
  /*  stop any active tool  */
  active_tool_control (HALT, gdisp);

  if (!edit_paste (gdisp->gimage, gimp_image_active_drawable (gdisp->gimage), 
		   global_buf, paste_into))
    return FALSE;

  /*  flush the display  */
  gdisplays_update_title (gdisp->gimage);
  gdisplays_flush ();

  return TRUE;
}

gboolean
global_edit_paste_as_new (GDisplay *gdisp)
{
  if (!global_buf)
    return FALSE;

  /*  stop any active tool  */
  active_tool_control (HALT, gdisp);

  return edit_paste_as_new (gdisp->gimage, global_buf);
}

void
global_edit_free (void)
{
  if (global_buf)
    tile_manager_destroy (global_buf);

  global_buf = NULL;
}

/*********************************************/
/*        Named buffer operations            */

static void
set_list_of_named_buffers (GtkWidget *list_widget)
{
  GSList      *list;
  NamedBuffer *nb;
  GtkWidget   *list_item;

  gtk_list_clear_items (GTK_LIST (list_widget), 0, -1);

  for (list = named_buffers; list; list = g_slist_next (list))
    {
      nb = (NamedBuffer *) list->data;

      list_item = gtk_list_item_new_with_label (nb->name);
      gtk_container_add (GTK_CONTAINER (list_widget), list_item);
      gtk_object_set_user_data (GTK_OBJECT (list_item), (gpointer) nb);
      gtk_widget_show (list_item);
    }
}

static void
named_buffer_paste_foreach (GtkWidget *widget,
			    gpointer   data)
{
  PasteNamedDlg *pn_dlg;
  NamedBuffer   *nb;

  if (widget->state == GTK_STATE_SELECTED)
    {
      pn_dlg = (PasteNamedDlg *) data;
      nb     = (NamedBuffer *) gtk_object_get_user_data (GTK_OBJECT (widget));

      switch (pn_dlg->action)
	{
	case PASTE:
	  edit_paste (pn_dlg->gdisp->gimage,
		      gimp_image_active_drawable (pn_dlg->gdisp->gimage),
		      nb->buf, FALSE);
	  break;

	case PASTE_INTO:
	  edit_paste (pn_dlg->gdisp->gimage,
		      gimp_image_active_drawable (pn_dlg->gdisp->gimage),
		      nb->buf, TRUE);
	  break;

	case PASTE_AS_NEW:
	  edit_paste_as_new (pn_dlg->gdisp->gimage, nb->buf);
	  break;

	default:
	  break;
	}
    }
}

static void
named_buffer_paste_callback (GtkWidget *widget,
			     gpointer   data)
{
  PasteNamedDlg *pn_dlg;

  pn_dlg = (PasteNamedDlg *) data;

  pn_dlg->action = PASTE_INTO;
  gtk_container_foreach ((GtkContainer *) pn_dlg->list,
			 named_buffer_paste_foreach, data);

  /*  Destroy the box  */
  gtk_widget_destroy (pn_dlg->shell);

  g_free (pn_dlg);
      
  /*  flush the display  */
  gdisplays_flush ();
}

static void
named_buffer_paste_into_callback (GtkWidget *widget,
				  gpointer   data)
{
  PasteNamedDlg *pn_dlg;

  pn_dlg = (PasteNamedDlg *) data;

  pn_dlg->action = PASTE_INTO;
  gtk_container_foreach ((GtkContainer *) pn_dlg->list,
			 named_buffer_paste_foreach, data);

  /*  Destroy the box  */
  gtk_widget_destroy (pn_dlg->shell);

  g_free (pn_dlg);
      
  /*  flush the display  */
  gdisplays_flush ();
}

static void
named_buffer_paste_as_new_callback (GtkWidget *widget,
				    gpointer   data)
{
  PasteNamedDlg *pn_dlg;

  pn_dlg = (PasteNamedDlg *) data;

  pn_dlg->action = PASTE_AS_NEW;
  gtk_container_foreach ((GtkContainer *) pn_dlg->list,
			 named_buffer_paste_foreach, data);

  /*  Destroy the box  */
  gtk_widget_destroy (pn_dlg->shell);

  g_free (pn_dlg);
      
  /*  flush the display  */
  gdisplays_flush ();
}

static void
named_buffer_delete_foreach (GtkWidget *widget,
			     gpointer   data)
{
  PasteNamedDlg *pn_dlg;
  NamedBuffer   *nb;

  if (widget->state == GTK_STATE_SELECTED)
    {
      pn_dlg = (PasteNamedDlg *) data;
      nb     = (NamedBuffer *) gtk_object_get_user_data (GTK_OBJECT (widget));

      named_buffers = g_slist_remove (named_buffers, (void *) nb);
      g_free (nb->name);
      tile_manager_destroy (nb->buf);
      g_free (nb);
    }
}

static void
named_buffer_delete_callback (GtkWidget *widget,
			      gpointer   data)
{
  PasteNamedDlg *pn_dlg;

  pn_dlg = (PasteNamedDlg *) data;

  gtk_container_foreach ((GtkContainer*) pn_dlg->list,
			 named_buffer_delete_foreach, data);
  set_list_of_named_buffers (pn_dlg->list);
}

static void
named_buffer_cancel_callback (GtkWidget *widget,
			      gpointer   data)
{
  PasteNamedDlg *pn_dlg;

  pn_dlg = (PasteNamedDlg *) data;

  /*  Destroy the box  */
  gtk_widget_destroy (pn_dlg->shell);

  g_free (pn_dlg);
}

static void
paste_named_buffer (GDisplay *gdisp)
{
  PasteNamedDlg *pn_dlg;
  GtkWidget *vbox;
  GtkWidget *label;
  GtkWidget *listbox;
  GtkWidget *bbox;
  GtkWidget *button;
  gint i;

  static gchar *paste_action_labels[] =
  {
    N_("Paste"),
    N_("Paste Into"),
    N_("Paste as New"),
  };

  static GtkSignalFunc paste_action_functions[] =
  {
    named_buffer_paste_callback,
    named_buffer_paste_into_callback,
    named_buffer_paste_as_new_callback,
  };

  pn_dlg = g_new (PasteNamedDlg, 1);
  pn_dlg->gdisp = gdisp;

  pn_dlg->shell =
    gimp_dialog_new (_("Paste Named Buffer"), "paste_named_buffer",
		     gimp_standard_help_func,
		     "dialogs/paste_named.html",
		     GTK_WIN_POS_MOUSE,
		     FALSE, TRUE, FALSE,

		     _("Delete"), named_buffer_delete_callback,
		     pn_dlg, NULL, NULL, FALSE, FALSE,
		     _("Cancel"), named_buffer_cancel_callback,
		     pn_dlg, NULL, NULL, TRUE, TRUE,

		     NULL);
		     
  vbox = gtk_vbox_new (FALSE, 1);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 1);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (pn_dlg->shell)->vbox), vbox);
  gtk_widget_show (vbox);

  label = gtk_label_new (_("Select a buffer to paste:"));
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, FALSE, 0);
  gtk_widget_show (label);

  listbox = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (listbox),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (vbox), listbox, TRUE, TRUE, 0);
  gtk_widget_set_usize (listbox, 125, 150);
  gtk_widget_show (listbox);

  pn_dlg->list = gtk_list_new ();
  gtk_list_set_selection_mode (GTK_LIST (pn_dlg->list), GTK_SELECTION_BROWSE);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (listbox),
					 pn_dlg->list);
  set_list_of_named_buffers (pn_dlg->list);
  gtk_widget_show (pn_dlg->list);

  bbox = gtk_hbutton_box_new ();
  gtk_container_set_border_width (GTK_CONTAINER (bbox), 6);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (bbox), 2);
  gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, FALSE, 0);
  for (i = 0; i < 3; i++)
    {
      button = gtk_button_new_with_label (gettext (paste_action_labels[i]));
      gtk_container_add (GTK_CONTAINER (bbox), button);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  (GtkSignalFunc) paste_action_functions[i],
			  pn_dlg);
      gtk_widget_show (button);
    }
  gtk_widget_show (bbox);

  gtk_widget_show (pn_dlg->shell);
}

static void
new_named_buffer (TileManager *tiles,
		  gchar       *name)
{
  PixelRegion  srcPR, destPR;
  NamedBuffer *nb;
  gint         width, height;

  if (! tiles) 
    return;

  width  = tile_manager_width (tiles);
  height = tile_manager_height (tiles);

  nb = (NamedBuffer *) g_malloc (sizeof (NamedBuffer));

  nb->buf = tile_manager_new (width, height, tile_manager_bpp (tiles));
  pixel_region_init (&srcPR, tiles, 0, 0, width, height, FALSE); 
  pixel_region_init (&destPR, nb->buf, 0, 0, width, height, TRUE);
  copy_region (&srcPR, &destPR);

  nb->name = g_strdup ((gchar *) name);
  named_buffers = g_slist_append (named_buffers, (void *) nb);
}

static void
cut_named_buffer_callback (GtkWidget *widget,
			   gchar     *name,
			   gpointer   data)
{
  TileManager *new_tiles;
  GDisplay    *gdisp;

  gdisp = (GDisplay *) data;
  
  new_tiles = edit_cut (gdisp->gimage,
			gimp_image_active_drawable (gdisp->gimage));
  if (new_tiles) 
    new_named_buffer (new_tiles, name);
  gdisplays_flush ();
}

gboolean
named_edit_cut (GDisplay *gdisp)
{
  GtkWidget *qbox;

  /*  stop any active tool  */
  active_tool_control (HALT, gdisp);

  qbox = gimp_query_string_box (_("Cut Named"),
				gimp_standard_help_func,
				"dialogs/cut_named.html",
				_("Enter a name for this buffer"),
				NULL,
				GTK_OBJECT (gdisp->gimage), "destroy",
				cut_named_buffer_callback, gdisp);
  gtk_widget_show (qbox);

  return TRUE;
}

static void
copy_named_buffer_callback (GtkWidget *widget,
			    gchar     *name,
			    gpointer   data)
{
  TileManager *new_tiles;
  GDisplay    *gdisp;

  gdisp = (GDisplay *) data;
  
  new_tiles = edit_copy (gdisp->gimage,
			 gimp_image_active_drawable (gdisp->gimage));
  if (new_tiles) 
    new_named_buffer (new_tiles, name);
}

gboolean
named_edit_copy (GDisplay *gdisp)
{
  GtkWidget *qbox;

  qbox = gimp_query_string_box (_("Copy Named"),
				gimp_standard_help_func,
				"dialogs/copy_named.html",
				_("Enter a name for this buffer"),
				NULL,
				GTK_OBJECT (gdisp->gimage), "destroy",
				copy_named_buffer_callback, gdisp);
  gtk_widget_show (qbox);

  return TRUE;
}

gboolean
named_edit_paste (GDisplay *gdisp)
{
  paste_named_buffer (gdisp);

  gdisplays_flush ();

  return TRUE;
}

void
named_buffers_free (void)
{
  GSList      *list;
  NamedBuffer *nb;

  for (list = named_buffers; list; list = g_slist_next (list))
    {
      nb = (NamedBuffer *) list->data;

      tile_manager_destroy (nb->buf);
      g_free (nb->name);
      g_free (nb);
    }

  g_slist_free (named_buffers);
  named_buffers = NULL;
}
