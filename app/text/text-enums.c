
/* Generated data (by gimp-mkenums) */

#include "config.h"
#include <glib-object.h>
#include "text-enums.h"
#include "gimp-intl.h"

/* enumerations from "./text-enums.h" */

static const GEnumValue gimp_text_box_mode_enum_values[] =
{
  { GIMP_TEXT_BOX_DYNAMIC, "GIMP_TEXT_BOX_DYNAMIC", "dynamic" },
  { GIMP_TEXT_BOX_FIXED, "GIMP_TEXT_BOX_FIXED", "fixed" },
  { 0, NULL, NULL }
};

GType
gimp_text_box_mode_get_type (void)
{
  static GType enum_type = 0;

  if (!enum_type)
    enum_type = g_enum_register_static ("GimpTextBoxMode", gimp_text_box_mode_enum_values);

  return enum_type;
}


static const GEnumValue gimp_text_direction_enum_values[] =
{
  { GIMP_TEXT_DIRECTION_LTR, N_("From Left to Right"), "ltr" },
  { GIMP_TEXT_DIRECTION_RTL, N_("From Right to Left"), "rtl" },
  { 0, NULL, NULL }
};

GType
gimp_text_direction_get_type (void)
{
  static GType enum_type = 0;

  if (!enum_type)
    enum_type = g_enum_register_static ("GimpTextDirection", gimp_text_direction_enum_values);

  return enum_type;
}


static const GEnumValue gimp_text_justification_enum_values[] =
{
  { GIMP_TEXT_JUSTIFY_LEFT, N_("Left Justified"), "left" },
  { GIMP_TEXT_JUSTIFY_RIGHT, N_("Right Justified"), "right" },
  { GIMP_TEXT_JUSTIFY_CENTER, N_("Centered"), "center" },
  { GIMP_TEXT_JUSTIFY_FILL, N_("Filled"), "fill" },
  { 0, NULL, NULL }
};

GType
gimp_text_justification_get_type (void)
{
  static GType enum_type = 0;

  if (!enum_type)
    enum_type = g_enum_register_static ("GimpTextJustification", gimp_text_justification_enum_values);

  return enum_type;
}


/* Generated data ends here */

