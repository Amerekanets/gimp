/* The GIMP -- an image manipulation program
 * Copyright (C) 1995-1997 Spencer Kimball and Peter Mattis
 *
 * cdisplay_colorblind.c
 * Copyright (C) 2002-2003 Michael Natterer <mitch@gimp.org>,
 *                         Sven Neumann <sven@gimp.org>,
 *                         Robert Dougherty <bob@vischeck.com> and
 *                         Alex Wade <alex@vischeck.com>
 *
 * This code is an implementation of an algorithm described by Hans Brettel,
 * Francoise Vienot and John Mollon in the Journal of the Optical Society of
 * America V14(10), pg 2647. (See http://vischeck.com/ for more info.)
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

#include "libgimpconfig/gimpconfig.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpmodule/gimpmodule.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "libgimp/libgimp-intl.h"


typedef enum
{
  COLORBLIND_DEFICIENCY_PROTANOPIA,
  COLORBLIND_DEFICIENCY_DEUTERANOPIA,
  COLORBLIND_DEFICIENCY_TRITANOPIA
} ColorblindDeficiency;

#define CDISPLAY_TYPE_COLORBLIND_DEFICIENCY (cdisplay_colorblind_deficiency_type)
static GType  cdisplay_colorblind_deficiency_get_type (GTypeModule *module);

static const GEnumValue enum_values[] =
{
  { COLORBLIND_DEFICIENCY_PROTANOPIA,
    "COLORBLIND_DEFICIENCY_PROTANOPIA", "protanopia"   },
  { COLORBLIND_DEFICIENCY_DEUTERANOPIA,
    "COLORBLIND_DEFICIENCY_DEUTERANOPIA", "deuteranopia" },
  { COLORBLIND_DEFICIENCY_TRITANOPIA,
    "COLORBLIND_DEFICIENCY_TRITANOPIA", "tritanopia"   },
  { 0, NULL, NULL }
};

static const GimpEnumDesc enum_descs[] =
  {
    { COLORBLIND_DEFICIENCY_PROTANOPIA,
      N_("Protanopia (insensitivity to red"), NULL },
    { COLORBLIND_DEFICIENCY_DEUTERANOPIA,
      N_("Deuteranopia (insensitivity to green)"), NULL },
    { COLORBLIND_DEFICIENCY_TRITANOPIA,
      N_("Tritanopia (insensitivity to blue)"), NULL },
    { 0, NULL, NULL }
  };

#define DEFAULT_DEFICIENCY  COLORBLIND_DEFICIENCY_DEUTERANOPIA
#define COLOR_CACHE_SIZE    1021


#define CDISPLAY_TYPE_COLORBLIND            (cdisplay_colorblind_type)
#define CDISPLAY_COLORBLIND(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CDISPLAY_TYPE_COLORBLIND, CdisplayColorblind))
#define CDISPLAY_COLORBLIND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CDISPLAY_TYPE_COLORBLIND, CdisplayColorblindClass))
#define CDISPLAY_IS_COLORBLIND(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CDISPLAY_TYPE_COLORBLIND))
#define CDISPLAY_IS_COLORBLIND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CDISPLAY_TYPE_COLORBLIND))


typedef struct _CdisplayColorblind      CdisplayColorblind;
typedef struct _CdisplayColorblindClass CdisplayColorblindClass;

struct _CdisplayColorblind
{
  GimpColorDisplay      parent_instance;

  ColorblindDeficiency  deficiency;

  gfloat                rgb2lms[9];
  gfloat                lms2rgb[9];
  gfloat                gammaRGB[3];

  gfloat                a1, b1, c1;
  gfloat                a2, b2, c2;
  gfloat                inflection;

  guint32               cache[2 * COLOR_CACHE_SIZE];
};

struct _CdisplayColorblindClass
{
  GimpColorDisplayClass  parent_instance;
};


enum
{
  PROP_0,
  PROP_DEFICIENCY
};


static GType       cdisplay_colorblind_get_type   (GTypeModule              *module);
static void        cdisplay_colorblind_class_init (CdisplayColorblindClass  *klass);
static void        cdisplay_colorblind_init       (CdisplayColorblind       *colorblind);

static void        cdisplay_colorblind_set_property  (GObject               *object,
                                                      guint                  property_id,
                                                      const GValue          *value,
                                                      GParamSpec            *pspec);
static void        cdisplay_colorblind_get_property  (GObject               *object,
                                                      guint                  property_id,
                                                      GValue                *value,
                                                      GParamSpec            *pspec);

static void        cdisplay_colorblind_convert       (GimpColorDisplay      *display,
                                                      guchar                *buf,
                                                      gint                   w,
                                                      gint                   h,
                                                      gint                   bpp,
                                                      gint                   bpl);
static GtkWidget * cdisplay_colorblind_configure      (GimpColorDisplay     *display);
static void        cdisplay_colorblind_changed        (GimpColorDisplay     *display);

static void        cdisplay_colorblind_set_deficiency (CdisplayColorblind   *colorblind,
                                                       ColorblindDeficiency  value);

static const GimpModuleInfo cdisplay_colorblind_info =
{
  GIMP_MODULE_ABI_VERSION,
  N_("Color deficit simulation filter (Brettel-Vienot-Mollon algorithm)"),
  "Michael Natterer <mitch@gimp.org>, Bob Dougherty <bob@vischeck.com>, "
  "Alex Wade <alex@vischeck.com>",
  "v0.2",
  "(c) 2002-2004, released under the GPL",
  "January 22, 2003"
};

static GType                  cdisplay_colorblind_type            = 0;
static GType                  cdisplay_colorblind_deficiency_type = 0;
static GimpColorDisplayClass *parent_class                        = NULL;


G_MODULE_EXPORT const GimpModuleInfo *
gimp_module_query (GTypeModule *module)
{
  return &cdisplay_colorblind_info;
}

G_MODULE_EXPORT gboolean
gimp_module_register (GTypeModule *module)
{
  cdisplay_colorblind_get_type (module);
  cdisplay_colorblind_deficiency_get_type (module);

  return TRUE;
}

static GType
cdisplay_colorblind_get_type (GTypeModule *module)
{
  if (! cdisplay_colorblind_type)
    {
      static const GTypeInfo display_info =
      {
        sizeof (CdisplayColorblindClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) cdisplay_colorblind_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data     */
        sizeof (CdisplayColorblind),
        0,              /* n_preallocs    */
        (GInstanceInitFunc) cdisplay_colorblind_init,
      };

      cdisplay_colorblind_type =
        g_type_module_register_type (module,
                                     GIMP_TYPE_COLOR_DISPLAY,
                                     "CdisplayColorblind",
                                     &display_info, 0);
    }

  return cdisplay_colorblind_type;
}


static GType
cdisplay_colorblind_deficiency_get_type (GTypeModule *module)
{
  if (! cdisplay_colorblind_deficiency_type)
    {
      cdisplay_colorblind_deficiency_type =
        gimp_module_register_enum (module,
                                   "CDisplayColorblindDeficiency", enum_values);

      gimp_enum_set_value_descriptions (cdisplay_colorblind_deficiency_type,
                                        enum_descs);
    }

  return cdisplay_colorblind_deficiency_type;
}

static void
cdisplay_colorblind_class_init (CdisplayColorblindClass *klass)
{
  GObjectClass          *object_class  = G_OBJECT_CLASS (klass);
  GimpColorDisplayClass *display_class = GIMP_COLOR_DISPLAY_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->get_property = cdisplay_colorblind_get_property;
  object_class->set_property = cdisplay_colorblind_set_property;

  GIMP_CONFIG_INSTALL_PROP_ENUM (object_class, PROP_DEFICIENCY,
                                 "deficiency", NULL,
                                 CDISPLAY_TYPE_COLORBLIND_DEFICIENCY,
                                 DEFAULT_DEFICIENCY,
                                 0);

  display_class->name        = _("Color Deficient Vision");
  display_class->help_id     = "gimp-colordisplay-colorblind";
  display_class->convert     = cdisplay_colorblind_convert;
  display_class->configure   = cdisplay_colorblind_configure;
  display_class->changed     = cdisplay_colorblind_changed;
}

static void
cdisplay_colorblind_init (CdisplayColorblind *colorblind)
{
  /* For most modern Cathode-Ray Tube monitors (CRTs), the following
   * are good estimates of the RGB->LMS and LMS->RGB transform
   * matrices.  They are based on spectra measured on a typical CRT
   * with a PhotoResearch PR650 spectral photometer and the Stockman
   * human cone fundamentals. NOTE: these estimates will NOT work well
   * for LCDs!
   */
  colorblind->rgb2lms[0] = 0.05059983;
  colorblind->rgb2lms[1] = 0.08585369;
  colorblind->rgb2lms[2] = 0.00952420;

  colorblind->rgb2lms[3] = 0.01893033;
  colorblind->rgb2lms[4] = 0.08925308;
  colorblind->rgb2lms[5] = 0.01370054;

  colorblind->rgb2lms[6] = 0.00292202;
  colorblind->rgb2lms[7] = 0.00975732;
  colorblind->rgb2lms[8] = 0.07145979;

  colorblind->lms2rgb[0] =  30.830854;
  colorblind->lms2rgb[1] = -29.832659;
  colorblind->lms2rgb[2] =   1.610474;

  colorblind->lms2rgb[3] =  -6.481468;
  colorblind->lms2rgb[4] =  17.715578;
  colorblind->lms2rgb[5] =  -2.532642;

  colorblind->lms2rgb[6] =  -0.375690;
  colorblind->lms2rgb[7] =  -1.199062;
  colorblind->lms2rgb[8] =  14.273846;

  /* The RGB<->LMS transforms above are computed from the human cone
   * photo-pigment absorption spectra and the monitor phosphor
   * emission spectra. These parameters are fairly constant for most
   * humans and most montiors (at least for modern CRTs). However,
   * gamma will vary quite a bit, as it is a property of the monitor
   * (eg. amplifier gain), the video card, and even the
   * software. Further, users can adjust their gammas (either via
   * adjusting the monitor amp gains or in software). That said, the
   * following are the gamma estimates that we have used in the
   * Vischeck code. Many colorblind users have viewed our simulations
   * and told us that they "work" (simulated and original images are
   * indistinguishabled).
   */
  colorblind->gammaRGB[0] = 2.1;
  colorblind->gammaRGB[1] = 2.0;
  colorblind->gammaRGB[2] = 2.1;
}

static void
cdisplay_colorblind_get_property (GObject    *object,
                                  guint       property_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  CdisplayColorblind *colorblind = CDISPLAY_COLORBLIND (object);

  switch (property_id)
    {
    case PROP_DEFICIENCY:
      g_value_set_enum (value, colorblind->deficiency);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
cdisplay_colorblind_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  CdisplayColorblind *colorblind = CDISPLAY_COLORBLIND (object);

  switch (property_id)
    {
    case PROP_DEFICIENCY:
      cdisplay_colorblind_set_deficiency (colorblind,
                                          g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
cdisplay_colorblind_convert (GimpColorDisplay *display,
                             guchar           *buf,
                             gint              width,
                             gint              height,
                             gint              bpp,
                             gint              bpl)
{
  CdisplayColorblind *colorblind = CDISPLAY_COLORBLIND (display);
  guchar             *b;
  gfloat              rgb2lms[9],lms2rgb[9];
  gfloat              a1, b1, c1, a2, b2, c2;
  gfloat              tmp;
  gfloat              red, green, blue, redOld, greenOld;
  gint                x, y;

  /* Require 3 bytes per pixel (assume RGB) */
  if (bpp != 3)
    return;

  /* to improve readability, copy the parameters into local variables */
  memcpy (rgb2lms, colorblind->rgb2lms, sizeof (rgb2lms));
  memcpy (lms2rgb, colorblind->lms2rgb, sizeof (lms2rgb));
  a1 = colorblind->a1; b1 = colorblind->b1; c1 = colorblind->c1;
  a2 = colorblind->a2; b2 = colorblind->b2; c2 = colorblind->c2;

  for (y = 0; y < height; y++, buf += bpl)
    for (x = 0, b = buf; x < width; x++, b += bpp)
      {
        guint32 pixel;
        guint   index;

        /* First check our cache */
        pixel = b[0] << 16 | b[1] << 8 | b[2];
        index = pixel % COLOR_CACHE_SIZE;

        if (colorblind->cache[2 * index] == pixel)
          {
            pixel = colorblind->cache[2 * index + 1];

            b[2] = pixel & 0xFF; pixel >>= 8;
            b[1] = pixel & 0xFF; pixel >>= 8;
            b[0] = pixel & 0xFF;

            continue;
          }

        red   = b[0];
        green = b[1];
        blue  = b[2];

        /* Remove gamma to linearize RGB intensities */
        red   = pow (red,   1.0 / colorblind->gammaRGB[0]);
        green = pow (green, 1.0 / colorblind->gammaRGB[1]);
        blue  = pow (blue,  1.0 / colorblind->gammaRGB[2]);

        /* Convert to LMS (dot product with transform matrix) */
        redOld   = red;
        greenOld = green;

        red   = redOld * rgb2lms[0] + greenOld * rgb2lms[1] + blue * rgb2lms[2];
        green = redOld * rgb2lms[3] + greenOld * rgb2lms[4] + blue * rgb2lms[5];
        blue  = redOld * rgb2lms[6] + greenOld * rgb2lms[7] + blue * rgb2lms[8];

        switch (colorblind->deficiency)
          {
          case COLORBLIND_DEFICIENCY_DEUTERANOPIA:
            tmp = blue / red;
            /* See which side of the inflection line we fall... */
            if (tmp < colorblind->inflection)
            green = -(a1 * red + c1 * blue) / b1;
            else
              green = -(a2 * red + c2 * blue) / b2;
            break;

          case COLORBLIND_DEFICIENCY_PROTANOPIA:
            tmp = blue / green;
            /* See which side of the inflection line we fall... */
            if (tmp < colorblind->inflection)
              red = -(b1 * green + c1 * blue) / a1;
            else
              red = -(b2 * green + c2 * blue) / a2;
            break;

          case COLORBLIND_DEFICIENCY_TRITANOPIA:
            tmp = green / red;
            /* See which side of the inflection line we fall... */
            if (tmp < colorblind->inflection)
              blue = -(a1 * red + b1 * green) / c1;
            else
              blue = -(a2 * red + b2 * green) / c2;
            break;

          default:
            break;
          }

        /* Convert back to RGB (cross product with transform matrix) */
        redOld   = red;
        greenOld = green;

        red   = redOld * lms2rgb[0] + greenOld * lms2rgb[1] + blue * lms2rgb[2];
        green = redOld * lms2rgb[3] + greenOld * lms2rgb[4] + blue * lms2rgb[5];
        blue  = redOld * lms2rgb[6] + greenOld * lms2rgb[7] + blue * lms2rgb[8];

        /* Apply gamma to go back to non-linear intensities */
        red   = pow (red,   colorblind->gammaRGB[0]);
        green = pow (green, colorblind->gammaRGB[1]);
        blue  = pow (blue,  colorblind->gammaRGB[2]);

        /* Ensure that we stay within the RGB gamut */
        /* *** FIX THIS: it would be better to desaturate than blindly clip. */
        red   = CLAMP (red,   0, 255);
        green = CLAMP (green, 0, 255);
        blue  = CLAMP (blue,  0, 255);

        /* Stuff result back into buffer */
        b[0] = (guchar) red;
        b[1] = (guchar) green;
        b[2] = (guchar) blue;

        /* Put the result into our cache */
        colorblind->cache[2 * index]     = pixel;
        colorblind->cache[2 * index + 1] = b[0] << 16 | b[1] << 8 | b[2];
      }
}

static GtkWidget *
cdisplay_colorblind_configure (GimpColorDisplay *display)
{
  CdisplayColorblind *colorblind = CDISPLAY_COLORBLIND (display);
  GtkWidget          *hbox;
  GtkWidget          *label;
  GtkWidget          *combo;

  hbox = gtk_hbox_new (FALSE, 6);

  label = gtk_label_new_with_mnemonic (_("Color _Deficiency Type:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  combo =
    gimp_prop_enum_combo_box_new (G_OBJECT (colorblind), "deficiency", 0, 0);

  gtk_box_pack_start (GTK_BOX (hbox), combo, TRUE, TRUE, 0);
  gtk_widget_show (combo);

  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);

  return hbox;
}

static void
cdisplay_colorblind_changed (GimpColorDisplay *display)
{
  CdisplayColorblind *colorblind = CDISPLAY_COLORBLIND (display);
  gfloat              anchor_e[3];
  gfloat              anchor[12];

  /*  This function performs initialisations that are dependant
   *  on the type of color deficiency.
   */

  /* Performs protan, deutan or tritan color image simulation based on
   * Brettel, Vienot and Mollon JOSA 14/10 1997
   *  L,M,S for lambda=475,485,575,660
   *
   * Load the LMS anchor-point values for lambda = 475 & 485 nm (for
   * protans & deutans) and the LMS values for lambda = 575 & 660 nm
   * (for tritans)
   */
  anchor[0] = 0.08008;  anchor[1]  = 0.1579;    anchor[2]  = 0.5897;
  anchor[3] = 0.1284;   anchor[4]  = 0.2237;    anchor[5]  = 0.3636;
  anchor[6] = 0.9856;   anchor[7]  = 0.7325;    anchor[8]  = 0.001079;
  anchor[9] = 0.0914;   anchor[10] = 0.007009;  anchor[11] = 0.0;

  /* We also need LMS for RGB=(1,1,1)- the equal-energy point (one of
   * our anchors) (we can just peel this out of the rgb2lms transform
   * matrix)
   */
  anchor_e[0] =
    colorblind->rgb2lms[0] + colorblind->rgb2lms[1] + colorblind->rgb2lms[2];
  anchor_e[1] =
    colorblind->rgb2lms[3] + colorblind->rgb2lms[4] + colorblind->rgb2lms[5];
  anchor_e[2] =
    colorblind->rgb2lms[6] + colorblind->rgb2lms[7] + colorblind->rgb2lms[8];

  switch (colorblind->deficiency)
    {
    case COLORBLIND_DEFICIENCY_DEUTERANOPIA:
      /* find a,b,c for lam=575nm and lam=475 */
      colorblind->a1 = anchor_e[1] * anchor[8] - anchor_e[2] * anchor[7];
      colorblind->b1 = anchor_e[2] * anchor[6] - anchor_e[0] * anchor[8];
      colorblind->c1 = anchor_e[0] * anchor[7] - anchor_e[1] * anchor[6];
      colorblind->a2 = anchor_e[1] * anchor[2] - anchor_e[2] * anchor[1];
      colorblind->b2 = anchor_e[2] * anchor[0] - anchor_e[0] * anchor[2];
      colorblind->c2 = anchor_e[0] * anchor[1] - anchor_e[1] * anchor[0];
      colorblind->inflection = (anchor_e[2] / anchor_e[0]);
      break;

    case COLORBLIND_DEFICIENCY_PROTANOPIA:
      /* find a,b,c for lam=575nm and lam=475 */
      colorblind->a1 = anchor_e[1] * anchor[8] - anchor_e[2] * anchor[7];
      colorblind->b1 = anchor_e[2] * anchor[6] - anchor_e[0] * anchor[8];
      colorblind->c1 = anchor_e[0] * anchor[7] - anchor_e[1] * anchor[6];
      colorblind->a2 = anchor_e[1] * anchor[2] - anchor_e[2] * anchor[1];
      colorblind->b2 = anchor_e[2] * anchor[0] - anchor_e[0] * anchor[2];
      colorblind->c2 = anchor_e[0] * anchor[1] - anchor_e[1] * anchor[0];
      colorblind->inflection = (anchor_e[2] / anchor_e[1]);
      break;

    case COLORBLIND_DEFICIENCY_TRITANOPIA:
      /* Set 1: regions where lambda_a=575, set 2: lambda_a=475 */
      colorblind->a1 = anchor_e[1] * anchor[11] - anchor_e[2] * anchor[10];
      colorblind->b1 = anchor_e[2] * anchor[9]  - anchor_e[0] * anchor[11];
      colorblind->c1 = anchor_e[0] * anchor[10] - anchor_e[1] * anchor[9];
      colorblind->a2 = anchor_e[1] * anchor[5]  - anchor_e[2] * anchor[4];
      colorblind->b2 = anchor_e[2] * anchor[3]  - anchor_e[0] * anchor[5];
      colorblind->c2 = anchor_e[0] * anchor[4]  - anchor_e[1] * anchor[3];
      colorblind->inflection = (anchor_e[1] / anchor_e[0]);
      break;
    }

  /* Invalidate the cache */
  memset (colorblind->cache, 0, sizeof (colorblind->cache));
}

static void
cdisplay_colorblind_set_deficiency (CdisplayColorblind   *colorblind,
                                    ColorblindDeficiency  value)
{
  if (value != colorblind->deficiency)
    {
      GEnumClass *enum_class;

      enum_class = g_type_class_peek (CDISPLAY_TYPE_COLORBLIND_DEFICIENCY);

      if (! g_enum_get_value (enum_class, value))
        return;

      colorblind->deficiency = value;

      g_object_notify (G_OBJECT (colorblind), "deficiency");
      gimp_color_display_changed (GIMP_COLOR_DISPLAY (colorblind));
    }
}
