/* The GIMP -- an image manipulation program
 * Copyright (C) 1995-1997 Spencer Kimball and Peter Mattis
 *
 * gimptemplate.c
 * Copyright (C) 2003 Michael Natterer <mitch@gimp.org>
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

#include <glib-object.h>

#include "libgimpbase/gimpbase.h"

#include "core-types.h"

#include "config/gimpconfig.h"
#include "config/gimpconfig-deserialize.h"
#include "config/gimpconfig-serialize.h"
#include "config/gimpconfig-params.h"
#include "config/gimpcoreconfig.h"

#include "gimp.h"
#include "gimpimage.h"
#include "gimplayer.h"
#include "gimptemplate.h"

#include "gimp-intl.h"


enum
{
  PROP_0,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_UNIT,
  PROP_XRESOLUTION,
  PROP_YRESOLUTION,
  PROP_RESOLUTION_UNIT,
  PROP_IMAGE_TYPE,
  PROP_FILL_TYPE,
  PROP_FILENAME
};


static void  gimp_template_class_init        (GimpTemplateClass   *klass);
static void  gimp_template_init              (GimpTemplate        *template);
static void  gimp_template_config_iface_init (GimpConfigInterface *config_iface);

static void      gimp_template_finalize      (GObject          *object);
static void      gimp_template_set_property  (GObject          *object,
                                              guint             property_id,
                                              const GValue     *value,
                                              GParamSpec       *pspec);
static void      gimp_template_get_property  (GObject          *object,
                                              guint             property_id,
                                              GValue           *value,
                                              GParamSpec       *pspec);
static void      gimp_template_notify        (GObject          *object,
                                              GParamSpec       *pspec);

static gboolean  gimp_template_serialize     (GimpConfig       *config,
                                              GimpConfigWriter *writer,
                                              gpointer          data);
static gboolean  gimp_template_deserialize   (GimpConfig       *config,
                                              GScanner         *scanner,
                                              gint              nest_level,
                                              gpointer          data);


static GimpViewableClass *parent_class = NULL;


GType
gimp_template_get_type (void)
{
  static GType template_type = 0;

  if (! template_type)
    {
      static const GTypeInfo template_info =
      {
        sizeof (GimpTemplateClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_template_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data     */
	sizeof (GimpTemplate),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_template_init,
      };
      static const GInterfaceInfo config_iface_info =
      {
        (GInterfaceInitFunc) gimp_template_config_iface_init,
        NULL,           /* iface_finalize */
        NULL            /* iface_data     */
      };

      template_type = g_type_register_static (GIMP_TYPE_VIEWABLE,
                                              "GimpTemplate",
                                              &template_info, 0);

      g_type_add_interface_static (template_type, GIMP_TYPE_CONFIG,
                                   &config_iface_info);
    }

  return template_type;
}

static void
gimp_template_class_init (GimpTemplateClass *klass)
{
  GObjectClass      *object_class;
  GimpViewableClass *viewable_class;

  object_class   = G_OBJECT_CLASS (klass);
  viewable_class = GIMP_VIEWABLE_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gimp_template_finalize;

  object_class->set_property = gimp_template_set_property;
  object_class->get_property = gimp_template_get_property;
  object_class->notify       = gimp_template_notify;

  viewable_class->default_stock_id = "gimp-template";

  GIMP_CONFIG_INSTALL_PROP_INT (object_class, PROP_WIDTH, "width",
                                "The image width in pixels.",
                                1, GIMP_MAX_IMAGE_SIZE, 256,
                                0);
  GIMP_CONFIG_INSTALL_PROP_INT (object_class, PROP_HEIGHT, "height",
                                "The image height in pixels.",
                                1, GIMP_MAX_IMAGE_SIZE, 256,
                                0);
  GIMP_CONFIG_INSTALL_PROP_UNIT (object_class, PROP_UNIT, "unit",
                                 "The unit used for coordinate display "
                                 "when not in dot-for-dot mode.",
                                 FALSE, FALSE, GIMP_UNIT_INCH,
                                 0);

  GIMP_CONFIG_INSTALL_PROP_DOUBLE (object_class, PROP_XRESOLUTION,
                                   "xresolution",
                                   "The horizonal resolution in dpi.",
                                   GIMP_MIN_RESOLUTION, GIMP_MAX_RESOLUTION,
                                   72.0,
                                   0);
  GIMP_CONFIG_INSTALL_PROP_DOUBLE (object_class, PROP_YRESOLUTION,
                                   "yresolution",
                                   "The vertical resolution in dpi.",
                                   GIMP_MIN_RESOLUTION, GIMP_MAX_RESOLUTION,
                                   72.0,
                                   0);
  GIMP_CONFIG_INSTALL_PROP_UNIT (object_class, PROP_RESOLUTION_UNIT,
                                 "resolution-unit",
                                 "The unit used to display resolutions.",
                                 FALSE, FALSE, GIMP_UNIT_INCH,
                                 0);

  GIMP_CONFIG_INSTALL_PROP_ENUM (object_class, PROP_IMAGE_TYPE,
                                 "image-type",
                                 NULL,
                                 GIMP_TYPE_IMAGE_BASE_TYPE, GIMP_RGB,
                                 0);
  GIMP_CONFIG_INSTALL_PROP_ENUM (object_class, PROP_FILL_TYPE,
                                 "fill-type",
                                 NULL,
                                 GIMP_TYPE_FILL_TYPE, GIMP_BACKGROUND_FILL,
                                 0);

  GIMP_CONFIG_INSTALL_PROP_STRING (object_class, PROP_FILENAME,
                                   "filename",
                                   NULL,
                                   NULL,
                                   0);
}

static void
gimp_template_init (GimpTemplate *template)
{
}

static void
gimp_template_config_iface_init (GimpConfigInterface *config_iface)
{
  config_iface->serialize   = gimp_template_serialize;
  config_iface->deserialize = gimp_template_deserialize;
}

static void
gimp_template_finalize (GObject *object)
{
  GimpTemplate *template;

  template = GIMP_TEMPLATE (object);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_template_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GimpTemplate *template;

  template = GIMP_TEMPLATE (object);

  switch (property_id)
    {
    case PROP_WIDTH:
      template->width = g_value_get_int (value);
      break;
    case PROP_HEIGHT:
      template->height = g_value_get_int (value);
      break;
    case PROP_UNIT:
      template->unit = g_value_get_int (value);
      break;
    case PROP_XRESOLUTION:
      template->xresolution = g_value_get_double (value);
      break;
    case PROP_YRESOLUTION:
      template->yresolution = g_value_get_double (value);
      break;
    case PROP_RESOLUTION_UNIT:
      template->resolution_unit = g_value_get_int (value);
      break;
    case PROP_IMAGE_TYPE:
      template->image_type = g_value_get_enum (value);
      break;
    case PROP_FILL_TYPE:
      template->fill_type = g_value_get_enum (value);
      break;
    case PROP_FILENAME:
      if (template->filename)
        g_free (template->filename);
      template->filename = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_template_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GimpTemplate *template;

  template = GIMP_TEMPLATE (object);

  switch (property_id)
    {
    case PROP_WIDTH:
      g_value_set_int (value, template->width);
      break;
    case PROP_HEIGHT:
      g_value_set_int (value, template->height);
      break;
    case PROP_UNIT:
      g_value_set_int (value, template->unit);
      break;
    case PROP_XRESOLUTION:
      g_value_set_double (value, template->xresolution);
      break;
    case PROP_YRESOLUTION:
      g_value_set_double (value, template->yresolution);
      break;
    case PROP_RESOLUTION_UNIT:
      g_value_set_int (value, template->resolution_unit);
      break;
    case PROP_IMAGE_TYPE:
      g_value_set_enum (value, template->image_type);
      break;
    case PROP_FILL_TYPE:
      g_value_set_enum (value, template->fill_type);
      break;
    case PROP_FILENAME:
      g_value_set_string (value, template->filename);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_template_notify (GObject    *object,
                      GParamSpec *pspec)
{
  GimpTemplate *template;
  guint64       size;
  gint          channels;

  template = GIMP_TEMPLATE (object);

  if (G_OBJECT_CLASS (parent_class)->notify)
    G_OBJECT_CLASS (parent_class)->notify (object, pspec);

  channels = ((template->image_type == GIMP_RGB ? 3 : 1)     /* color      */ +
              (template->fill_type == GIMP_TRANSPARENT_FILL) /* alpha      */ +
              1                                              /* selection  */ +
              (template->image_type == GIMP_RGB ? 4 : 2)     /* projection */);

  size = ((guint64) channels        *
          (guint64) template->width *
          (guint64) template->height);

  if (size > G_MAXULONG)
    {
      template->initial_size           = G_MAXULONG;
      template->initial_size_too_large = TRUE;
    }
  else
    {
      template->initial_size           = (gulong) size;
      template->initial_size_too_large = FALSE;
    }

  if (! strcmp (pspec->name, "stock-id"))
    gimp_viewable_invalidate_preview (GIMP_VIEWABLE (object));
}

static gboolean
gimp_template_serialize (GimpConfig       *config,
                         GimpConfigWriter *writer,
                         gpointer          data)
{
  return gimp_config_serialize_properties (config, writer);
}

static gboolean
gimp_template_deserialize (GimpConfig *config,
                           GScanner   *scanner,
                           gint        nest_level,
                           gpointer    data)
{
  return gimp_config_deserialize_properties (config,
                                             scanner, nest_level, FALSE);
}


/*  public functions  */

GimpTemplate *
gimp_template_new (const gchar *name)
{
  g_return_val_if_fail (name != NULL, NULL);

  return g_object_new (GIMP_TYPE_TEMPLATE,
                       "name", name,
                       NULL);
}

void
gimp_template_set_from_image (GimpTemplate *template,
                              GimpImage    *gimage)
{
  gdouble           xresolution;
  gdouble           yresolution;
  GimpImageBaseType image_type;

  g_return_if_fail (GIMP_IS_TEMPLATE (template));
  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  gimp_image_get_resolution (gimage, &xresolution, &yresolution);

  image_type = gimp_image_base_type (gimage);

  if (image_type == GIMP_INDEXED)
    image_type = GIMP_RGB;

  g_object_set (template,
                "width",           gimp_image_get_width (gimage),
                "height",          gimp_image_get_height (gimage),
                "unit",            gimp_image_get_unit (gimage),
                "xresolution",     xresolution,
                "yresolution",     yresolution,
                "resolution-unit", gimage->gimp->config->default_image->resolution_unit,
                "image-type",      image_type,
                NULL);
}

GimpImage *
gimp_template_create_image (Gimp         *gimp,
                            GimpTemplate *template)
{
  GimpImage     *gimage;
  GimpLayer     *layer;
  GimpImageType  type;
  gint           width, height;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);
  g_return_val_if_fail (GIMP_IS_TEMPLATE (template), NULL);

  gimage = gimp_create_image (gimp,
			      template->width, template->height,
			      template->image_type,
			      TRUE);

  gimp_image_undo_disable (gimage);

  gimp_image_set_resolution (gimage,
                             template->xresolution, template->yresolution);
  gimp_image_set_unit (gimage, template->unit);

  width  = gimp_image_get_width (gimage);
  height = gimp_image_get_height (gimage);

  switch (template->fill_type)
    {
    case GIMP_TRANSPARENT_FILL:
      type = (template->image_type == GIMP_RGB) ? GIMP_RGBA_IMAGE : GIMP_GRAYA_IMAGE;
      break;
    default:
      type = (template->image_type == GIMP_RGB) ? GIMP_RGB_IMAGE : GIMP_GRAY_IMAGE;
      break;
    }

  layer = gimp_layer_new (gimage, width, height, type,
                          _("Background"),
			  GIMP_OPACITY_OPAQUE, GIMP_NORMAL_MODE);

  gimp_image_add_layer (gimage, layer, 0);

  gimp_drawable_fill_by_type (GIMP_DRAWABLE (layer),
                              gimp_get_current_context (gimp),
                              template->fill_type);

  gimp_image_undo_enable (gimage);
  gimp_image_clean_all (gimage);

  gimp_create_display (gimp, gimage, 0x0101);

  g_object_unref (gimage);

  return gimage;
}
