/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * Polarize plug-in --- maps a rectangul to a circle or vice-versa
 * Copyright (C) 1997 Daniel Dunbar
 * Email: ddunbar@diads.com
 * WWW:   http://millennium.diads.com/gimp/
 * Copyright (C) 1997 Federico Mena Quintero
 * federico@nuclecu.unam.mx
 * Copyright (C) 1996 Marc Bless
 * E-mail: bless@ai-lab.fh-furtwangen.de
 * WWW:    www.ai-lab.fh-furtwangen.de/~bless
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


/* Version 1.0:
 * This is the follow-up release.  It contains a few minor changes, the
 * most major being that the first time I released the wrong version of
 * the code, and this time the changes have been fixed.  I also added
 * tooltips to the dialog.
 *
 * Feel free to email me if you have any comments or suggestions on this
 * plugin.
 *               --Daniel Dunbar
 *                 ddunbar@diads.com
 */


/* Version .5:
 * This is the first version publicly released, it will probably be the
 * last also unless i can think of some features i want to add.
 *
 * This plug-in was created with loads of help from quartic (Frederico
 * Mena Quintero), and would surely not have come about without it.
 *
 * The polar algorithms is copied from Marc Bless' polar plug-in for
 * .54, many thanks to him also.
 *
 * If you can think of a neat addition to this plug-in, or any other
 * info about it, please email me at ddunbar@diads.com.
 *                                     - Daniel Dunbar
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "libgimp/stdplugins-intl.h"

#include "gimpoldpreview.h"

#define WITHIN(a, b, c) ((((a) <= (b)) && ((b) <= (c))) ? 1 : 0)


/***** Magic numbers *****/

#define PLUG_IN_NAME    "plug_in_polar_coords"
#define PLUG_IN_VERSION "July 1997, 0.5"
#define HELP_ID         "plug-in-polar-coords"

#define SCALE_WIDTH  200
#define ENTRY_WIDTH   60

/***** Types *****/

typedef struct
{
  gdouble circle;
  gdouble angle;
  gint backwards;
  gint inverse;
  gint polrec;
} polarize_vals_t;

static GimpOldPreview *preview;

/***** Prototypes *****/

static void query (void);
static void run   (const gchar      *name,
		   gint              nparams,
		   const GimpParam  *param,
		   gint             *nreturn_vals,
		   GimpParam       **return_vals);

static void   polarize(void);
static gboolean calc_undistorted_coords(double wx, double wy,
					double *x, double *y);

static gint polarize_dialog       (void);
static void dialog_update_preview (void);

static void dialog_scale_update (GtkAdjustment *adjustment, gdouble *value);

static void polar_toggle_callback (GtkWidget *widget, gpointer data);

/***** Variables *****/

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run    /* run_proc   */
};

static polarize_vals_t pcvals =
{
  100.0, /* circle */
  0.0,  /* angle */
  0, /* backwards */
  1,  /* inverse */
  1  /* polar to rectangular? */
};

static GimpDrawable *drawable;
static GimpRunMode run_mode;

static gint img_width, img_height, img_has_alpha;
static gint sel_x1, sel_y1, sel_x2, sel_y2;
static gint sel_width, sel_height;

static double cen_x, cen_y;
static double scale_x, scale_y;

/***** Functions *****/

MAIN ()

static void
query (void)
{
  static GimpParamDef args[] =
  {
    { GIMP_PDB_INT32,    "run_mode",  "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE,    "image",     "Input image" },
    { GIMP_PDB_DRAWABLE, "drawable",  "Input drawable" },
    { GIMP_PDB_FLOAT,    "circle",    "Circle depth in %" },
    { GIMP_PDB_FLOAT,    "angle",     "Offset angle" },
    { GIMP_PDB_INT32,    "backwards",    "Map backwards?" },
    { GIMP_PDB_INT32,    "inverse",     "Map from top?" },
    { GIMP_PDB_INT32,    "polrec",     "Polar to rectangular?" }
  };

  gimp_install_procedure (PLUG_IN_NAME,
			  "Converts and image to and from polar coords",
			  "Remaps and image from rectangular coordinates to polar coordinates "
			  "or vice versa",
			  "Daniel Dunbar and Federico Mena Quintero",
			  "Daniel Dunbar and Federico Mena Quintero",
			  PLUG_IN_VERSION,
			  N_("<Image>/Filters/Distorts/P_olar Coords..."),
			  "RGB*, GRAY*",
			  GIMP_PLUGIN,
			  G_N_ELEMENTS (args), 0,
			  args, NULL);
}

static void
run (const gchar      *name,
     gint              nparams,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
  static GimpParam values[1];

  GimpPDBStatusType  status;
  double       xhsiz, yhsiz;

  status   = GIMP_PDB_SUCCESS;
  run_mode = param[0].data.d_int32;

  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

  *nreturn_vals = 1;
  *return_vals  = values;

  INIT_I18N ();

  /* Get the active drawable info */

  drawable = gimp_drawable_get (param[2].data.d_drawable);

  img_width     = gimp_drawable_width (drawable->drawable_id);
  img_height    = gimp_drawable_height (drawable->drawable_id);
  img_has_alpha = gimp_drawable_has_alpha (drawable->drawable_id);

  gimp_drawable_mask_bounds (drawable->drawable_id, &sel_x1, &sel_y1, &sel_x2, &sel_y2);

  /* Calculate scaling parameters */

  sel_width  = sel_x2 - sel_x1;
  sel_height = sel_y2 - sel_y1;

  cen_x = (double) (sel_x1 + sel_x2 - 1) / 2.0;
  cen_y = (double) (sel_y1 + sel_y2 - 1) / 2.0;

  xhsiz = (double) (sel_width - 1) / 2.0;
  yhsiz = (double) (sel_height - 1) / 2.0;

  if (xhsiz < yhsiz)
    {
      scale_x = yhsiz / xhsiz;
      scale_y = 1.0;
    }
  else if (xhsiz > yhsiz)
    {
      scale_x = 1.0;
      scale_y = xhsiz / yhsiz;
    }
  else
    {
      scale_x = 1.0;
      scale_y = 1.0;
    }

  /* See how we will run */

  switch (run_mode)
    {
    case GIMP_RUN_INTERACTIVE:
      /* Possibly retrieve data */
      gimp_get_data (PLUG_IN_NAME, &pcvals);

      /* Get information from the dialog */
      if (!polarize_dialog ())
	return;

      break;

    case GIMP_RUN_NONINTERACTIVE:
      /* Make sure all the arguments are present */
      if (nparams != 8)
	{
	  status = GIMP_PDB_CALLING_ERROR;
	}
      else
	{
	  pcvals.circle  = param[3].data.d_float;
	  pcvals.angle  = param[4].data.d_float;
	  pcvals.backwards  = param[5].data.d_int32;
	  pcvals.inverse  = param[6].data.d_int32;
	  pcvals.polrec  = param[7].data.d_int32;
	}
      break;

    case GIMP_RUN_WITH_LAST_VALS:
      /* Possibly retrieve data */
      gimp_get_data (PLUG_IN_NAME, &pcvals);
      break;

    default:
      break;
    }

  /* Distort the image */
  if ((status == GIMP_PDB_SUCCESS) &&
      (gimp_drawable_is_rgb (drawable->drawable_id) ||
       gimp_drawable_is_gray (drawable->drawable_id)))
    {
      /* Set the tile cache size */
      gimp_tile_cache_ntiles (2 * (drawable->width + gimp_tile_width() - 1) /
			      gimp_tile_width ());

      /* Run! */
      polarize ();

      /* If run mode is interactive, flush displays */
      if (run_mode != GIMP_RUN_NONINTERACTIVE)
	gimp_displays_flush ();

      /* Store data */
      if (run_mode == GIMP_RUN_INTERACTIVE)
	gimp_set_data (PLUG_IN_NAME, &pcvals, sizeof (polarize_vals_t));
    }
  else if (status == GIMP_PDB_SUCCESS)
    status = GIMP_PDB_EXECUTION_ERROR;

  values[0].data.d_status = status;

  gimp_drawable_detach (drawable);
}

static void
polarize_func (gint x,
	       gint y,
	       guchar *dest,
	       gint bpp,
	       gpointer data)
{
  double     cx, cy;

  if (calc_undistorted_coords (x, y, &cx, &cy))
    {
      guchar     pixel1[4], pixel2[4], pixel3[4], pixel4[4];
      guchar     *values[4] = {pixel1, pixel2, pixel3, pixel4};
      GimpPixelFetcher *pft = (GimpPixelFetcher*) data;

      gimp_pixel_fetcher_get_pixel (pft, cx, cy, pixel1);
      gimp_pixel_fetcher_get_pixel (pft, cx + 1, cy, pixel2);
      gimp_pixel_fetcher_get_pixel (pft, cx, cy + 1, pixel3);
      gimp_pixel_fetcher_get_pixel (pft, cx + 1, cy + 1, pixel4);

      gimp_bilinear_pixels_8 (dest, cx, cy, bpp, img_has_alpha, values);
    }
  else
    {
      gint b;
      for (b = 0; b < bpp; b++)
	{
	  dest[b] = 255;
	}
    }
}

static void
polarize (void)
{
  GimpRgnIterator  *iter;
  GimpPixelFetcher *pft;
  GimpRGB           background;

  pft = gimp_pixel_fetcher_new (drawable, FALSE);

  gimp_palette_get_background (&background);
  gimp_pixel_fetcher_set_bg_color (pft, &background);

  gimp_progress_init (_("Polarizing..."));

  iter = gimp_rgn_iterator_new (drawable, run_mode);
  gimp_rgn_iterator_dest (iter, polarize_func, pft);
  gimp_rgn_iterator_free (iter);

  gimp_pixel_fetcher_destroy (pft);
}

static gboolean
calc_undistorted_coords (gdouble  wx,
			 gdouble  wy,
			 gdouble *x,
			 gdouble *y)
{
  gboolean inside;
  gdouble  phi, phi2;
  gdouble  xx, xm, ym, yy;
  gint     xdiff, ydiff;
  gdouble  r;
  gdouble  m;
  gdouble  xmax, ymax, rmax;
  gdouble  x_calc, y_calc;
  gdouble  xi, yi;
  gdouble  circle, angl, t, angle;
  gint     x1, x2, y1, y2;

  /* initialize */

  phi = 0.0;
  r   = 0.0;

  x1     = 0;
  y1     = 0;
  x2     = img_width;
  y2     = img_height;
  xdiff  = x2 - x1;
  ydiff  = y2 - y1;
  xm     = xdiff / 2.0;
  ym     = ydiff / 2.0;
  circle = pcvals.circle;
  angle  = pcvals.angle;
  angl   = (gdouble) angle / 180.0 * G_PI;

  if (pcvals.polrec)
    {
      if (wx >= cen_x)
	{
	  if (wy > cen_y)
	    {
	      phi = G_PI - atan (((double)(wx - cen_x))/
				 ((double)(wy - cen_y)));
	    }
	  else if (wy < cen_y)
	    {
	      phi = atan (((double)(wx - cen_x))/((double)(cen_y - wy)));
	    }
	  else
	    {
	      phi = G_PI / 2;
	    }
	}
      else if (wx < cen_x)
	{
	  if (wy < cen_y)
	    {
	      phi = 2 * G_PI - atan (((double)(cen_x -wx)) /
				     ((double)(cen_y - wy)));
	    }
	  else if (wy > cen_y)
	    {
	      phi = G_PI + atan (((double)(cen_x - wx))/
				 ((double)(wy - cen_y)));
	    }
	  else
	    {
	      phi = 1.5 * G_PI;
	    }
	}

      r   = sqrt (SQR (wx - cen_x) + SQR (wy - cen_y));

      if (wx != cen_x)
	{
	  m = fabs (((double)(wy - cen_y)) / ((double)(wx - cen_x)));
	}
      else
	{
	  m = 0;
	}

      if (m <= ((double)(y2 - y1) / (double)(x2 - x1)))
	{
	  if (wx == cen_x)
	    {
	      xmax = 0;
	      ymax = cen_y - y1;
	    }
	  else
	    {
	      xmax = cen_x - x1;
	      ymax = m * xmax;
	    }
	}
      else
	{
	  ymax = cen_y - y1;
	  xmax = ymax / m;
	}

      rmax = sqrt ( (double)(SQR (xmax) + SQR (ymax)) );

      t = ((cen_y - y1) < (cen_x - x1)) ? (cen_y - y1) : (cen_x - x1);
      rmax = (rmax - t) / 100 * (100 - circle) + t;

      phi = fmod (phi + angl, 2*G_PI);

      if (pcvals.backwards)
	x_calc = x2 - 1 - (x2 - x1 - 1)/(2*G_PI) * phi;
      else
	x_calc = (x2 - x1 - 1)/(2*G_PI) * phi + x1;

      if (pcvals.inverse)
	y_calc = (y2 - y1)/rmax   * r   + y1;
      else
	y_calc = y2 - (y2 - y1)/rmax * r;
    }
  else
    {
      if (pcvals.backwards)
	phi = (2 * G_PI) * (x2 - wx) / xdiff;
      else
	phi = (2 * G_PI) * (wx - x1) / xdiff;

      phi = fmod (phi + angl, 2 * G_PI);

      if (phi >= 1.5 * G_PI)
	phi2 = 2 * G_PI - phi;
      else if (phi >= G_PI)
	phi2 = phi - G_PI;
      else if (phi >= 0.5 * G_PI)
	phi2 = G_PI - phi;
      else
	phi2 = phi;

      xx = tan (phi2);
      if (xx != 0)
	m = (double) 1.0 / xx;
      else
	m = 0;

      if (m <= ((double)(ydiff) / (double)(xdiff)))
	{
	  if (phi2 == 0)
	    {
	      xmax = 0;
	      ymax = ym - y1;
	    }
	  else
	    {
	      xmax = xm - x1;
	      ymax = m * xmax;
	    }
	}
      else
	{
	  ymax = ym - y1;
	  xmax = ymax / m;
	}

      rmax = sqrt ((double)(SQR (xmax) + SQR (ymax)));

      t = ((ym - y1) < (xm - x1)) ? (ym - y1) : (xm - x1);

      rmax = (rmax - t) / 100.0 * (100 - circle) + t;

      if (pcvals.inverse)
	r = rmax * (double)((wy - y1) / (double)(ydiff));
      else
	r = rmax * (double)((y2 - wy) / (double)(ydiff));

      xx = r * sin (phi2);
      yy = r * cos (phi2);

      if (phi >= 1.5 * G_PI)
	{
	  x_calc = (double)xm - xx;
	  y_calc = (double)ym - yy;
	}
      else if (phi >= G_PI)
	{
	  x_calc = (double)xm - xx;
	  y_calc = (double)ym + yy;
	}
      else if (phi >= 0.5 * G_PI)
	{
	  x_calc = (double)xm + xx;
	  y_calc = (double)ym + yy;
	}
      else
	{
	  x_calc = (double)xm + xx;
	  y_calc = (double)ym - yy;
	}
    }

  xi = (int) (x_calc + 0.5);
  yi = (int) (y_calc + 0.5);

  inside = (WITHIN (0, xi, img_width - 1) && WITHIN (0, yi, img_height - 1));
  if (inside)
    {
      *x = x_calc;
      *y = y_calc;
    }
  return inside;
}

static gint
polarize_dialog (void)
{
  GtkWidget *dialog;
  GtkWidget *main_vbox;
  GtkWidget *vbox;
  GtkWidget *frame;
  GtkWidget *table;
  GtkWidget *abox;
  GtkWidget *pframe;
  GtkWidget *toggle;
  GtkWidget *hbox;
  GtkObject *adj;
  gboolean   run;

  gimp_ui_init ("polar", TRUE);

  dialog = gimp_dialog_new (_("Polarize"), "polar",
                            NULL, 0,
			    gimp_standard_help_func, HELP_ID,

			    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			    GTK_STOCK_OK,     GTK_RESPONSE_OK,

			    NULL);

  main_vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 6);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->vbox), main_vbox,
		      FALSE, FALSE, 0);
  gtk_widget_show (main_vbox);

  /* Preview */
  frame = gtk_frame_new (_("Preview"));
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  abox = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (frame), abox);
  gtk_widget_show (abox);

  pframe = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (pframe), GTK_SHADOW_IN);
  gtk_container_set_border_width (GTK_CONTAINER (pframe), 4);
  gtk_container_add (GTK_CONTAINER (abox), pframe);
  gtk_widget_show (pframe);

  preview = gimp_old_preview_new (drawable, FALSE);
  gtk_container_add (GTK_CONTAINER (pframe), preview->widget);
  gtk_widget_show (preview->widget);

  /* Controls */
  frame = gtk_frame_new (_("Parameter Settings"));
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);

  table = gtk_table_new (2, 3, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 0,
			      _("Circle _Depth in Percent:"), SCALE_WIDTH, 0,
			      pcvals.circle, 0.0, 100.0, 1.0, 10.0, 2,
			      TRUE, 0, 0,
			      NULL, NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (dialog_scale_update),
                    &pcvals.circle);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 1,
			      _("Offset _Angle:"), SCALE_WIDTH, 0,
			      pcvals.angle, 0.0, 359.0, 1.0, 15.0, 2,
			      TRUE, 0, 0,
			      NULL, NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (dialog_scale_update),
                    &pcvals.angle);

  /* togglebuttons for backwards, top, polar->rectangular */
  hbox = gtk_hbox_new (TRUE, 4);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  toggle = gtk_check_button_new_with_mnemonic (_("_Map Backwards"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), pcvals.backwards);
  gtk_box_pack_start (GTK_BOX (hbox), toggle, TRUE, TRUE, 0);
  gtk_widget_show (toggle);

  gimp_help_set_help_data (toggle,
			   _("If checked the mapping will begin at the right "
			     "side, as opposed to beginning at the left."),
			   NULL);

  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (polar_toggle_callback),
                    &pcvals.backwards);

  toggle = gtk_check_button_new_with_mnemonic (_("Map from _Top"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), pcvals.inverse);
  gtk_box_pack_start (GTK_BOX (hbox), toggle, TRUE, TRUE, 0);
  gtk_widget_show (toggle);

  gimp_help_set_help_data (toggle,
			   _("If unchecked the mapping will put the bottom "
			     "row in the middle and the top row on the "
			     "outside.  If checked it will be the opposite."),
			   NULL);

  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (polar_toggle_callback),
                    &pcvals.inverse);

  toggle = gtk_check_button_new_with_mnemonic (_("To _Polar"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), pcvals.polrec);
  gtk_box_pack_start (GTK_BOX (hbox), toggle, TRUE, TRUE, 0);
  gtk_widget_show (toggle);

  gimp_help_set_help_data (toggle,
			   _("If unchecked the image will be circularly "
			     "mapped onto a rectangle.  If checked the image "
			     "will be mapped onto a circle."),
			   NULL);

  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (polar_toggle_callback),
                    &pcvals.polrec);

  gtk_widget_show (hbox);

  /* Done */

  gtk_widget_show (dialog);
  dialog_update_preview ();

  run = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);

  gtk_widget_destroy (dialog);

  gimp_old_preview_free (preview);

  return run;
}

static void
dialog_update_preview (void)
{
  gdouble  left, right, bottom, top;
  gdouble  dx, dy;
  gdouble  px, py;
  gdouble  cx = 0.0, cy = 0.0;
  gint     ix, iy;
  gint     x, y;
  gint	   bpp;
  gdouble  scale_x, scale_y;
  guchar  *p_ul, *i;
  GimpRGB  background;
  guchar   outside[4];
  guchar  *buffer;

  gimp_palette_get_background (&background);
  gimp_rgb_set_alpha (&background, 0.0);
  gimp_drawable_get_color_uchar (drawable->drawable_id, &background, outside);

  left   = sel_x1;
  right  = sel_x2 - 1;
  bottom = sel_y2 - 1;
  top    = sel_y1;

  dx = (right - left) / (preview->width - 1);
  dy = (bottom - top) / (preview->height - 1);

  scale_x = (double) preview->width / (right - left + 1);
  scale_y = (double) preview->height / (bottom - top + 1);

  bpp = preview->bpp;

  py = top;

  buffer = g_new (guchar, preview->rowstride);

  for (y = 0; y < preview->height; y++)
    {
      px = left;
      p_ul = buffer;

      for (x = 0; x < preview->width; x++)
	{
	  calc_undistorted_coords (px, py, &cx, &cy);

	  cx = (cx - left) * scale_x;
	  cy = (cy - top) * scale_y;

	  ix = (int) (cx + 0.5);
	  iy = (int) (cy + 0.5);

	  if ((ix >= 0) && (ix < preview->width) &&
	      (iy >= 0) && (iy < preview->height))
	    i = preview->cache + preview->rowstride * iy + bpp * ix;
	  else
	    i = outside;

	  p_ul[0] = i[0];
	  p_ul[1] = i[1];
	  p_ul[2] = i[2];
	  p_ul[3] = i[3];

	  p_ul += bpp;

	  px += dx;
	}

      gimp_old_preview_do_row (preview, y, preview->width, buffer);

      py += dy;
    }

  g_free (buffer);

  gtk_widget_queue_draw (preview->widget);
}

static void
dialog_scale_update (GtkAdjustment *adjustment,
		     gdouble       *value)
{
  gimp_double_adjustment_update (adjustment, value);

  dialog_update_preview ();
}

static void
polar_toggle_callback (GtkWidget *widget,
		       gpointer   data)
{
  gimp_toggle_button_update (widget, data);

  dialog_update_preview ();
}
