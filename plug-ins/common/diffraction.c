/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * Diffraction plug-in --- Generate diffraction patterns
 * Copyright (C) 1997 Federico Mena Quintero and David Bleecker
 * federico@nuclecu.unam.mx
 * bleecker@math.hawaii.edu
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

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>

#ifdef __GNUC__
#warning GTK_DISABLE_DEPRECATED
#endif
#undef GTK_DISABLE_DEPRECATED

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "libgimp/stdplugins-intl.h"


/***** Magic numbers *****/
#define ITERATIONS      100
#define WEIRD_FACTOR      0.04

#define PREVIEW_WIDTH    64
#define PREVIEW_HEIGHT   64
#define PROGRESS_WIDTH   32
#define PROGRESS_HEIGHT  16
#define SCALE_WIDTH     150


/***** Types *****/

typedef struct
{
  gdouble lam_r;
  gdouble lam_g;
  gdouble lam_b;
  gdouble contour_r;
  gdouble contour_g;
  gdouble contour_b;
  gdouble edges_r;
  gdouble edges_g;
  gdouble edges_b;
  gdouble brightness;
  gdouble scattering;
  gdouble polarization;
} diffraction_vals_t;

typedef struct
{
  GtkWidget *preview;
  GtkWidget *progress;
  guchar     preview_row[PREVIEW_WIDTH * 3];
} diffraction_interface_t;


/***** Prototypes *****/

static void query (void);
static void run   (const gchar      *name,
		   gint              nparams,
		   const GimpParam  *param,
		   gint             *nreturn_vals,
		   GimpParam       **return_vals);

static void diffraction (GimpDrawable *drawable);

static void   diff_init_luts (void);
static void   diff_diffract  (gdouble  x,
			      gdouble  y,
			      GimpRGB *rgb);
static double diff_point     (gdouble  x,
			      gdouble  y,
			      gdouble  edges,
			      gdouble  contours,
			      gdouble  lam);
static double diff_intensity (gdouble  x,
			      gdouble  y,
			      gdouble  lam);

static gint diffraction_dialog     (void);
static void dialog_update_preview  (void);
static void dialog_update_callback (GtkWidget *widget,
				    gpointer   data);

/***** Variables *****/

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init  */
  NULL,  /* quit  */
  query, /* query */
  run    /* run   */
};

static diffraction_vals_t dvals =
{
  0.815,   /* lam_r */
  1.221,   /* lam_g */
  1.123,   /* lam_b */
  0.821,   /* contour_r */
  0.821,   /* contour_g */
  0.974,   /* contour_b */
  0.610,   /* edges_r */
  0.677,   /* edges_g */
  0.636,   /* edges_b */
  0.066,   /* brightness */
  37.126,   /* scattering */
  -0.473    /* polarization */
};

static diffraction_interface_t dint =
{
  NULL,  /* preview */
  NULL,  /* progress */
  { 0 }  /* preview_row */
};

static gdouble cos_lut[ITERATIONS + 1];
static gdouble param_lut1[ITERATIONS + 1];
static gdouble param_lut2[ITERATIONS + 1];

static GimpRunMode run_mode;


/***** Functions *****/

MAIN ()

static void
query (void)
{
  static GimpParamDef args[] =
  {
    { GIMP_PDB_INT32,    "run_mode",     "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE,    "image",        "Input image" },
    { GIMP_PDB_DRAWABLE, "drawable",     "Input drawable" },
    { GIMP_PDB_FLOAT,    "lam_r",        "Light frequency (red)" },
    { GIMP_PDB_FLOAT, 	  "lam_g",        "Light frequency (green)" },
    { GIMP_PDB_FLOAT, 	  "lam_b",        "Light frequency (blue)" },
    { GIMP_PDB_FLOAT, 	  "contour_r",    "Number of contours (red)" },
    { GIMP_PDB_FLOAT, 	  "contour_g",    "Number of contours (green)" },
    { GIMP_PDB_FLOAT, 	  "contour_b",    "Number of contours (blue)" },
    { GIMP_PDB_FLOAT, 	  "edges_r",      "Number of sharp edges (red)" },
    { GIMP_PDB_FLOAT, 	  "edges_g",      "Number of sharp edges (green)" },
    { GIMP_PDB_FLOAT, 	  "edges_b",      "Number of sharp edges (blue)" },
    { GIMP_PDB_FLOAT, 	  "brightness",   "Brightness and shifting/fattening of contours" },
    { GIMP_PDB_FLOAT, 	  "scattering",   "Scattering (Speed vs. quality)" },
    { GIMP_PDB_FLOAT, 	  "polarization", "Polarization" }
  };

  gimp_install_procedure ("plug_in_diffraction",
			  "Generate diffraction patterns",
			  "Help?  What help?  Real men do not need help :-)",  /* FIXME */
			  "Federico Mena Quintero",
			  "Federico Mena Quintero & David Bleecker",
			  "April 1997, 0.5",
			  N_("<Image>/Filters/Render/Pattern/_Diffraction Patterns..."),
			  "RGB*",
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

  GimpDrawable      *active_drawable;
  GimpPDBStatusType  status;

  /* Initialize */

  INIT_I18N ();

  diff_init_luts ();

  status   = GIMP_PDB_SUCCESS;
  run_mode = param[0].data.d_int32;

  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

  *nreturn_vals = 1;
  *return_vals  = values;

  switch (run_mode)
    {
    case GIMP_RUN_INTERACTIVE:
      /* Possibly retrieve data */
      gimp_get_data ("plug_in_diffraction", &dvals);

      /* Get information from the dialog */
      if (!diffraction_dialog ())
	return;

      break;

    case GIMP_RUN_NONINTERACTIVE:
      /* Make sure all the arguments are present */
      if (nparams != 15)
	status = GIMP_PDB_CALLING_ERROR;

      if (status == GIMP_PDB_SUCCESS)
	{
	  dvals.lam_r 	     = param[3].data.d_float;
	  dvals.lam_g 	     = param[4].data.d_float;
	  dvals.lam_b        = param[5].data.d_float;
	  dvals.contour_r    = param[6].data.d_float;
	  dvals.contour_g    = param[7].data.d_float;
	  dvals.contour_b    = param[8].data.d_float;
	  dvals.edges_r      = param[9].data.d_float;
	  dvals.edges_g      = param[10].data.d_float;
	  dvals.edges_b      = param[11].data.d_float;
	  dvals.brightness   = param[12].data.d_float;
	  dvals.scattering   = param[13].data.d_float;
	  dvals.polarization = param[14].data.d_float;
	}

      break;

    case GIMP_RUN_WITH_LAST_VALS:
      /* Possibly retrieve data */
      gimp_get_data ("plug_in_diffraction", &dvals);
      break;

    default:
      break;
    }

  /* Get the active drawable */
  active_drawable = gimp_drawable_get (param[2].data.d_drawable);

  /* Create the diffraction pattern */
  if ((status == GIMP_PDB_SUCCESS) && gimp_drawable_is_rgb(active_drawable->drawable_id))
    {
      /* Set the tile cache size */
      gimp_tile_cache_ntiles ((active_drawable->width + gimp_tile_width() - 1) /
			      gimp_tile_width());

      /* Run! */
      diffraction (active_drawable);

      /* If run mode is interactive, flush displays */
      if (run_mode != GIMP_RUN_NONINTERACTIVE)
	gimp_displays_flush ();

      /* Store data */
      if (run_mode == GIMP_RUN_INTERACTIVE)
	gimp_set_data ("plug_in_diffraction",
		       &dvals, sizeof(diffraction_vals_t));
    }
  else if (status == GIMP_PDB_SUCCESS)
    status = GIMP_PDB_EXECUTION_ERROR;

  values[0].data.d_status = status;

  gimp_drawable_detach (active_drawable);
}

typedef struct {
  gdouble 	dhoriz;
  gdouble 	dvert;
  gint		x1;
  gint 		y1;
} DiffractionParam_t;

static void
diffraction_func (gint x,
		  gint y,
		  guchar *dest,
		  gint bpp,
		  gpointer data)
{
  DiffractionParam_t *param = (DiffractionParam_t*) data;
  gdouble px, py;
  GimpRGB rgb;

  px = -5.0 + param->dhoriz * (x - param->x1);
  py = 5.0 + param->dvert * (y - param->y1);

  diff_diffract (px, py, &rgb);
  
  dest[0] = 255.0 * rgb.r;
  dest[1] = 255.0 * rgb.g;
  dest[2] = 255.0 * rgb.b;
  
  if (bpp == 4)
    dest[3] = 255;
}

static void
diffraction (GimpDrawable *drawable)
{
  GimpRgnIterator *iter;
  DiffractionParam_t param;
  gint x1, y1, x2, y2;

  gimp_drawable_mask_bounds (drawable->drawable_id, &x1, &y1, &x2, &y2);
  param.x1 = x1;
  param.y1 = y1;
  param.dhoriz = 10.0 / (x2 - x1 - 1);
  param.dvert  = -10.0 / (y2 - y1 - 1);

  gimp_progress_init (_("Creating diffraction pattern..."));
  iter = gimp_rgn_iterator_new (drawable, run_mode);
  gimp_rgn_iterator_dest (iter, diffraction_func, &param);
  gimp_rgn_iterator_free (iter);
}

static void
diff_init_luts (void)
{
  int    i;
  double a;
  double sina;

  a = -G_PI;

  for (i = 0; i <= ITERATIONS; i++)
    {
      sina = sin (a);

      cos_lut[i]    = cos (a);

      param_lut1[i] = 0.75 * sina;
      param_lut2[i] = 0.5 * (4.0 * cos_lut[i] * cos_lut[i] + sina * sina);

      a += (2.0 * G_PI / ITERATIONS);
    }
}

static void
diff_diffract (double   x,
	       double   y,
	       GimpRGB* rgb)
{
  rgb->r = diff_point (x, y, dvals.edges_r, dvals.contour_r, dvals.lam_r);
  rgb->g = diff_point (x, y, dvals.edges_g, dvals.contour_g, dvals.lam_g);
  rgb->b = diff_point (x, y, dvals.edges_b, dvals.contour_b, dvals.lam_b);
}

static double
diff_point (double x,
	    double y,
	    double edges,
	    double contours,
	    double lam)
{
  return fabs (edges * sin (contours * atan (dvals.brightness *
					     diff_intensity (x, y, lam))));
}

static double
diff_intensity (double x,
		double y,
		double lam)
{
  int    i;
  double cxy, sxy;
  double param;
  double polpi2;
  double cospolpi2, sinpolpi2;

  cxy = 0.0;
  sxy = 0.0;

  lam *= 4.0;

  for (i = 0; i <= ITERATIONS; i++)
    {
      param = lam *
	(cos_lut[i] * x +
	 param_lut1[i] * y -
	 param_lut2[i]);

      cxy += cos (param);
      sxy += sin (param);
    }

  cxy *= WEIRD_FACTOR;
  sxy *= WEIRD_FACTOR;

  polpi2 = dvals.polarization * (G_PI / 2.0);

  cospolpi2 = cos (polpi2);
  sinpolpi2 = sin (polpi2);

  return dvals.scattering * ((cospolpi2 + sinpolpi2) * cxy * cxy +
			     (cospolpi2 - sinpolpi2) * sxy * sxy);
}

static gint
diffraction_dialog (void)
{
  GtkWidget *dialog;
  GtkWidget *top_table;
  GtkWidget *notebook;
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *button;
  GtkObject *adj;
  gboolean   run;

  gimp_ui_init ("diffraction", TRUE);

  dialog = gimp_dialog_new (_("Diffraction Patterns"), "diffraction",
                            NULL, 0,
			    gimp_standard_help_func, "filters/diffraction.html",

			    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			    GTK_STOCK_OK,     GTK_RESPONSE_OK,

			    NULL);

  top_table = gtk_table_new (2, 2, FALSE);
  gtk_container_set_border_width(GTK_CONTAINER (top_table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (top_table), 4);
  gtk_table_set_col_spacings (GTK_TABLE (top_table), 4);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), top_table,
		      FALSE, FALSE, 0);
  gtk_widget_show (top_table);

  /* Preview */

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_table_attach (GTK_TABLE (top_table), vbox, 0, 1, 0, 1,
		    GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_widget_show (vbox);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  dint.preview = gtk_preview_new (GTK_PREVIEW_COLOR);
  gtk_preview_size (GTK_PREVIEW(dint.preview), PREVIEW_WIDTH, PREVIEW_HEIGHT);
  gtk_container_add (GTK_CONTAINER (frame), dint.preview);
  gtk_widget_show (dint.preview);

  dint.progress = gtk_progress_bar_new ();
  gtk_widget_set_size_request (dint.progress, PROGRESS_WIDTH, PROGRESS_HEIGHT);
  gtk_box_pack_start (GTK_BOX (vbox), dint.progress, TRUE, FALSE, 0);
  gtk_widget_show (dint.progress);

  button = gtk_button_new_with_mnemonic (_("_Preview!"));
  gtk_table_attach (GTK_TABLE (top_table), button, 0, 1, 1, 2,
		    GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_widget_show (button);

  g_signal_connect (button, "clicked",
                    G_CALLBACK (dialog_update_callback),
                    NULL);

  /* Notebook */

  notebook = gtk_notebook_new ();
  gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_TOP);
  gtk_table_attach (GTK_TABLE (top_table), notebook, 1, 2, 0, 2,
		    GTK_EXPAND | GTK_FILL, 0, 0, 0);
  gtk_widget_show (notebook);

  /* Frequencies tab */

  table = gtk_table_new (3, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_widget_show (table);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 0,
			      _("_Red:"), SCALE_WIDTH, 0,
			      dvals.lam_r, 0.0, 20.0, 0.2, 1.0, 3,
			      TRUE, 0, 0,
			      NULL, NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &dvals.lam_r);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 1,
			      _("_Green:"), SCALE_WIDTH, 0,
			      dvals.lam_g, 0.0, 20.0, 0.2, 1.0, 3,
			      TRUE, 0, 0,
			      NULL, NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &dvals.lam_g);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 2,
			      _("_Blue:"), SCALE_WIDTH, 0,
			      dvals.lam_b, 0.0, 20.0, 0.2, 1.0, 3,
			      TRUE, 0, 0,
			      NULL, NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &dvals.lam_b);

  label = gtk_label_new_with_mnemonic (_("_Frequencies"));
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), table, label);

  /* Contours tab */

  table = gtk_table_new (3, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_widget_show (table);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 0,
			      _("_Red:"), SCALE_WIDTH, 0,
			      dvals.contour_r, 0.0, 10.0, 0.1, 1.0, 3,
			      TRUE, 0, 0,
			      NULL, NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &dvals.contour_r);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 1,
			      _("_Green:"), SCALE_WIDTH, 0,
			      dvals.contour_g, 0.0, 10.0, 0.1, 1.0, 3,
			      TRUE, 0, 0,
			      NULL, NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &dvals.contour_g);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 2,
			      _("_Blue:"), SCALE_WIDTH, 0,
			      dvals.contour_b, 0.0, 10.0, 0.1, 1.0, 3,
			      TRUE, 0, 0,
			      NULL, NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &dvals.contour_b);

  label = gtk_label_new_with_mnemonic (_("Co_ntours"));
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), table, label);

  /* Sharp edges tab */

  table = gtk_table_new (3, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_widget_show (table);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 0,
			      _("_Red:"), SCALE_WIDTH, 0,
			      dvals.edges_r, 0.0, 1.0, 0.01, 0.1, 3,
			      TRUE, 0, 0,
			      NULL, NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &dvals.edges_r);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 1,
			      _("_Green:"), SCALE_WIDTH, 0,
			      dvals.edges_g, 0.0, 1.0, 0.01, 0.1, 3,
			      TRUE, 0, 0,
			      NULL, NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &dvals.edges_g);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 2,
			      _("_Blue:"), SCALE_WIDTH, 0,
			      dvals.edges_b, 0.0, 1.0, 0.01, 0.1, 3,
			      TRUE, 0, 0,
			      NULL, NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &dvals.edges_b);

  label = gtk_label_new_with_mnemonic (_("_Sharp edges"));
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), table, label);

  /* Other options tab */

  table = gtk_table_new (3, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_widget_show (table);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 0,
			      _("_Brightness:"), SCALE_WIDTH, 7,
			      dvals.brightness, 0.0, 1.0, 0.01, 0.1, 3,
			      TRUE, 0, 0,
			      NULL, NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &dvals.brightness);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 1,
			      _("Sc_attering:"), SCALE_WIDTH, 7,
			      dvals.scattering, 0.0, 100.0, 1.0, 10.0, 3,
			      TRUE, 0, 0,
			      NULL, NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &dvals.scattering);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 2,
			      _("Po_latization:"), SCALE_WIDTH, 7,
			      dvals.polarization, -1.0, 1.0, 0.02, 0.2, 3,
			      TRUE, 0, 0,
			      NULL, NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &dvals.polarization);

  label = gtk_label_new_with_mnemonic (_("O_ther options"));
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), table, label);

  /* Done */

  gtk_widget_show (dialog);
  dialog_update_preview ();

  run = (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK);

  gtk_widget_show (dialog);

  return run;
}

static void
dialog_update_preview (void)
{
  double  left, right, bottom, top;
  double  px, py;
  double  dx, dy;
  GimpRGB rgb;
  int     x, y;
  guchar *p;

  left   = -5.0;
  right  =  5.0;
  bottom = -5.0;
  top    =  5.0;

  dx = (right - left) / (PREVIEW_WIDTH - 1);
  dy = (bottom - top) / (PREVIEW_HEIGHT - 1);

  py = top;

  for (y = 0; y < PREVIEW_HEIGHT; y++)
    {
      px = left;
      p  = dint.preview_row;

      for (x = 0; x < PREVIEW_WIDTH; x++)
	{
	  diff_diffract (px, py, &rgb);

	  *p++ = 255.0 * rgb.r;
	  *p++ = 255.0 * rgb.g;
	  *p++ = 255.0 * rgb.b;

	  px += dx;
	}

      gtk_preview_draw_row (GTK_PREVIEW (dint.preview),
			    dint.preview_row, 0, y, PREVIEW_WIDTH);

      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (dint.progress),
                                     (double) y / (double) (PREVIEW_HEIGHT - 1));
      gtk_widget_queue_draw (dint.progress);

      py += dy;
    }

  gtk_widget_queue_draw (dint.preview);
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (dint.progress), 0.0);
}

static void
dialog_update_callback (GtkWidget *widget,
			gpointer   data)
{
  dialog_update_preview ();
}
