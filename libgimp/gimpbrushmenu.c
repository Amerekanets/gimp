/* LIBGIMP - The GIMP Library                                                   
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball                
 * Copyright (C) 1998 Andy Thomas                
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.             
 *                                                                              
 * This library is distributed in the hope that it will be useful,              
 * but WITHOUT ANY WARRANTY; without even the implied warranty of               
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU            
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */                                                                             
#include <stdio.h>
#include <string.h>

#include "gimp.h"
#include "gimpui.h"


/* Idea is to have a function to call that returns a widget that 
 * completely controls the selection of a brush.
 * you get a widget returned that you can use in a table say.
 * In:- Initial brush name. Null means use current selection.
 *      pointer to func to call when brush changes (GRunBrushCallback).
 * Returned:- Pointer to a widget that you can use in UI.
 * 
 * Widget simply made up of a preview widget (20x20) containing the brush mask
 * and a button that can be clicked on to change the brush.
 */


#define BSEL_DATA_KEY "__bsel_data"
#define CELL_SIZE 20
#define BRUSH_EVENT_MASK  GDK_EXPOSURE_MASK | \
                          GDK_BUTTON_PRESS_MASK | \
			  GDK_BUTTON_RELEASE_MASK | \
                          GDK_BUTTON1_MOTION_MASK 

struct __brushes_sel {
  gchar * dname;
  GRunBrushCallback cback;
  GtkWidget *brush_preview;
  GtkWidget *device_brushpopup; 
  GtkWidget *device_brushpreview;
  GtkWidget *button;
  GtkWidget *top_hbox;
  gchar *brush_name; /* Local copy */
  gdouble opacity;
  gint spacing;
  gint paint_mode;
  gint width;
  gint height;
  gchar *mask_data; /* local copy */
  void *brush_popup_pnt; /* POinter use to control the popup */
  gpointer udata;
};

typedef struct __brushes_sel BSelect, * BSelectP;

static void
brush_popup_open (int          x,
		    int          y,
		    BSelectP   bsel)
{
  gint x_org, y_org;
  gint scr_w, scr_h;
  gchar *src, *buf;
  unsigned char *b,*s;

  /* make sure the popup exists and is not visible */
  if (bsel->device_brushpopup == NULL)
    {
      GtkWidget *frame;

      bsel->device_brushpopup = gtk_window_new (GTK_WINDOW_POPUP);
      gtk_window_set_policy (GTK_WINDOW (bsel->device_brushpopup), FALSE, FALSE, TRUE);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
      gtk_container_add (GTK_CONTAINER (bsel->device_brushpopup), frame);
      gtk_widget_show (frame);
      bsel->device_brushpreview = gtk_preview_new (GTK_PREVIEW_GRAYSCALE);
      gtk_container_add (GTK_CONTAINER (frame), bsel->device_brushpreview);
      gtk_widget_show (bsel->device_brushpreview);
    }
  else
    {
      gtk_widget_hide (bsel->device_brushpopup);
    }

  /* decide where to put the popup */
  gdk_window_get_origin (bsel->brush_preview->window, &x_org, &y_org);
  scr_w = gdk_screen_width ();
  scr_h = gdk_screen_height ();
  x = x_org + x - (bsel->width >> 1);
  y = y_org + y - (bsel->height >> 1);
  x = (x < 0) ? 0 : x;
  y = (y < 0) ? 0 : y;
  x = (x + bsel->width > scr_w) ? scr_w - bsel->width : x;
  y = (y + bsel->height > scr_h) ? scr_h - bsel->height : y;
  gtk_preview_size (GTK_PREVIEW (bsel->device_brushpreview), bsel->width, bsel->height);

  gtk_widget_popup (bsel->device_brushpopup, x, y);
  
  /*  Draw the brush  */
  buf = g_new (gchar, bsel->width);
  
  memset(buf, 255, sizeof(bsel->width));
  
/*   for (i = 0; i < CELL_SIZE; i++) */
/*     gtk_preview_draw_row (GTK_PREVIEW(bsel->device_brushpreview), (guchar *)buf, 0, i, CELL_SIZE); */
  
  src = bsel->mask_data;
  
  for (y = 0; y < bsel->height; y++)
    {
      int j;
      s = src;
      b = buf;
      for (j = 0; j < bsel->width ; j++)
	*b++ = 255 - *s++;

      gtk_preview_draw_row (GTK_PREVIEW (bsel->device_brushpreview), (guchar *)buf, 0, y, bsel->width);
      src += bsel->width;
    }
  g_free(buf);
  
  /*  Draw the brush preview  */
  gtk_widget_draw (bsel->device_brushpreview, NULL);
}

static void
brush_popup_close (BSelectP   bsel)
{
  if (bsel->device_brushpopup != NULL)
    gtk_widget_hide (bsel->device_brushpopup);
}

static gint
brush_preview_events (GtkWidget    *widget,
			GdkEvent     *event,
			gpointer     udata)
{
  GdkEventButton *bevent;
  BSelectP bsel = (BSelectP)udata;

  if(bsel->mask_data)
    {
      switch (event->type)
	{
	case GDK_EXPOSE:
	  break;
	  
	case GDK_BUTTON_PRESS:
	  bevent = (GdkEventButton *) event;
	  
	  if (bevent->button == 1)
	    {
	      brush_popup_open (bevent->x, bevent->y, bsel);
	    }
	  break;
	  
	case GDK_BUTTON_RELEASE:
	  bevent = (GdkEventButton *) event;
	  
	  if (bevent->button == 1)
	    {
	      /*  Ungrab the pointer  */
	      gdk_pointer_ungrab (bevent->time);
	      
	      /*  Close the device preview popup window  */
	      brush_popup_close (bsel);
	    }
	  break;
	case GDK_DELETE:
	  break;
	  
	default:
	  break;
	}
    }

  return FALSE;
}

static void
brush_pre_update(GtkWidget *brush_preview,
		   gint brush_width,
		   gint brush_height,
		   gchar *mask_data)
{
  gint y,i;
  gchar *src, *buf;
  unsigned char *b,*s;
  int offset_x, offset_y;
  int yend;
  int ystart;
  gint width,height;
  /*  Draw the brush  */
  buf = g_new (gchar, CELL_SIZE); 

 /* Limit to cell size */
  width = (brush_width > CELL_SIZE) ? CELL_SIZE: brush_width;
  height = (brush_height > CELL_SIZE) ? CELL_SIZE: brush_height;

  /* Set buffer to white */  
  memset(buf, 255, CELL_SIZE);
  for (i = 0; i < CELL_SIZE; i++)
    gtk_preview_draw_row (GTK_PREVIEW(brush_preview), (guchar *)buf, 0, i, CELL_SIZE);
  
  offset_x = ((CELL_SIZE - width) >> 1);
  offset_y = ((CELL_SIZE - height) >> 1);

  ystart = CLAMP (offset_y, 0, CELL_SIZE);
  yend = CLAMP (offset_y + height, 0, CELL_SIZE);

  src = mask_data;

  for (y = ystart; y < yend; y++)
    {
      int j;
      s = src;
      b = buf;
      for (j = 0; j < width ; j++)
	*b++ = 255 - *s++;

      gtk_preview_draw_row (GTK_PREVIEW (brush_preview), (guchar *)buf, offset_x, y, width);
      src += brush_width;
    }
  g_free(buf);

  /*  Draw the brush preview  */
  gtk_widget_draw (brush_preview, NULL);
}

static void
brush_select_invoker(gchar    *name,
		     gdouble  opacity,
		     gint     spacing,
		     gint     paint_mode,
		     gint     width,
		     gint     height,
		     gchar *  mask_data,
		     gint     closing,
		     gpointer udata)
{
  gint mask_d_sz;
  BSelectP bsel = (BSelectP)udata;
  if(bsel->mask_data != NULL)
    g_free(bsel->mask_data);
  bsel->width = width;
  bsel->height = height;
  mask_d_sz = width*height;
  bsel->mask_data = g_malloc(mask_d_sz);
  g_memmove(bsel->mask_data,mask_data,mask_d_sz); 
  brush_pre_update(bsel->brush_preview,bsel->width,bsel->height,bsel->mask_data);
  bsel->opacity = opacity;
  bsel->spacing = spacing;
  bsel->paint_mode = paint_mode;
  if(bsel->cback != NULL)
    (bsel->cback)(name,opacity,spacing,paint_mode,width,height,mask_data,closing,bsel->udata);
  if(closing)
    {
      gtk_widget_set_sensitive(bsel->button,TRUE);
      bsel->brush_popup_pnt = NULL;
    }
}


static void
brush_select_callback (GtkWidget *widget,
		       gpointer   data)
{
  BSelectP bsel = (BSelectP)data;
  gtk_widget_set_sensitive(bsel->button,FALSE);
  bsel->brush_popup_pnt = 
    gimp_interactive_selection_brush((bsel->dname)?bsel->dname:"Brush Plugin Selection",
				       bsel->brush_name,
				       bsel->opacity,
				       bsel->spacing,
				       bsel->paint_mode,
				       brush_select_invoker,bsel);
}

GtkWidget * 
gimp_brush_select_widget(gchar *dname,
			 gchar *ibrush, 
			 gdouble opacity,
			 gint spacing,
			 gint paint_mode,
			 GRunBrushCallback cback,
			 gpointer udata)
{
  GtkWidget * frame;
  GtkWidget * hbox;
  GtkWidget * brush;
  GtkWidget * button;
  gint width,height;
  gint init_spacing,init_paint_mode;
  gdouble init_opacity;
  gchar *mask_data;
  gchar *brush_name;

  BSelectP bsel = g_new(BSelect, sizeof(BSelect));

  hbox = gtk_hbox_new (FALSE, 3);
  gtk_widget_show(hbox);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_OUT);
  gtk_widget_show(frame);

  brush = gtk_preview_new (GTK_PREVIEW_GRAYSCALE);
  gtk_preview_size (GTK_PREVIEW (brush), CELL_SIZE, CELL_SIZE); 
  gtk_widget_show(brush);
  gtk_container_add (GTK_CONTAINER (frame),brush); 

  gtk_widget_set_events (brush, BRUSH_EVENT_MASK);

  gtk_signal_connect (GTK_OBJECT (brush), "event",
		      (GtkSignalFunc) brush_preview_events,
		      (gpointer)bsel);

  bsel->cback = cback;
  bsel->udata = udata;
  bsel->mask_data = NULL;
  bsel->device_brushpopup = bsel->device_brushpreview = NULL;
  bsel->brush_preview = brush;
  bsel->brush_name = ibrush;
  bsel->dname = dname;
  bsel->brush_popup_pnt = NULL;

  /* Do initial brush setup */
  brush_name = gimp_brushes_get_brush_data(ibrush,
					   &init_opacity,
					   &init_spacing,
					   &init_paint_mode,
					   &width,
					   &height,
					   &mask_data);
  if(brush_name)
    {
      brush_pre_update(bsel->brush_preview,width,height,mask_data);
      bsel->mask_data = mask_data;
      bsel->brush_name = brush_name;
      bsel->width = width;
      bsel->height = height;
      if(opacity != -1)
	bsel->opacity = opacity;
      else
	bsel->opacity = init_opacity;
      if(spacing != -1)
	bsel->spacing = spacing;
      else
	bsel->spacing = init_spacing;
      if(paint_mode != -1)
	bsel->paint_mode = paint_mode;
      else
	bsel->paint_mode = init_paint_mode;
    }

  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);

  button = gtk_button_new_with_label ("... ");
  gtk_container_add (GTK_CONTAINER (hbox), button); 
  gtk_widget_show(button);

  bsel->button = button;
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
                      (GtkSignalFunc) brush_select_callback,
                      (gpointer)bsel);

  gtk_object_set_data(GTK_OBJECT(hbox),BSEL_DATA_KEY,(gpointer)bsel);

  return hbox;
}


gint
gimp_brush_select_widget_close_popup(GtkWidget *w)
{
  gint ret_val = FALSE;

  BSelectP bsel = (BSelectP)gtk_object_get_data(GTK_OBJECT(w),BSEL_DATA_KEY);
  if(bsel && bsel->brush_popup_pnt)
    {
      ret_val = gimp_brush_close_popup(bsel->brush_popup_pnt);
      bsel->brush_popup_pnt = NULL;
    }

  return ret_val;
}

gint 
gimp_brush_select_widget_set_popup(GtkWidget *w,
				   gchar *bname,
				   gdouble opacity,
				   gint spacing,
				   gint paint_mode)
{
  gint ret_val = FALSE;
  gint width,height;
  gint init_spacing,init_paint_mode;
  gdouble init_opacity;
  gchar *mask_data;
  gchar *brush_name;

  BSelectP bsel = (BSelectP)gtk_object_get_data(GTK_OBJECT(w),BSEL_DATA_KEY);

  if(bsel)
    {
      brush_name = gimp_brushes_get_brush_data(bname,
					       &init_opacity,
					       &init_spacing,
					       &init_paint_mode,
					       &width,&height,&mask_data);

      if(opacity == -1.0)
	opacity = init_opacity;

      if(spacing == -1)
	spacing = init_spacing;

      if(paint_mode == -1)
	paint_mode = init_paint_mode;
  
      brush_select_invoker(bname,opacity,spacing,paint_mode,width,height,mask_data,0,bsel);

      if(bsel->brush_popup_pnt)
	{
	  if(gimp_brush_set_popup(bsel->brush_popup_pnt,bname,opacity,spacing,paint_mode))
	    {
	      ret_val = TRUE;
	    }
	}
    }
  return ret_val;
}
