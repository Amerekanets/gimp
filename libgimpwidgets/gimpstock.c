/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * gimpstock.c
 * Copyright (C) 2001 Michael Natterer <mitch@gimp.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <gtk/gtk.h>
#include <gtk/gtkiconfactory.h>

#include "gimpstock.h"

#include "themes/Default/images/gimp-stock-pixbufs.h"

#include "libgimp/libgimp-intl.h"


#define LIBGIMP_DOMAIN GETTEXT_PACKAGE "-libgimp"


static GtkIconFactory *gimp_stock_factory = NULL;


static void
icon_set_from_inline (GtkIconSet   *set,
                      const guchar *inline_data,
                      GtkIconSize   size,
                      gboolean      fallback)
{
  GtkIconSource *source;
  GdkPixbuf     *pixbuf;

  source = gtk_icon_source_new ();

  gtk_icon_source_set_size (source, size);
  gtk_icon_source_set_size_wildcarded (source, FALSE);

  pixbuf = gdk_pixbuf_new_from_inline (-1, inline_data, FALSE, NULL);

  g_assert (pixbuf);

  gtk_icon_source_set_pixbuf (source, pixbuf);

  g_object_unref (pixbuf);

  gtk_icon_set_add_source (set, source);

  if (fallback)
    {
      gtk_icon_source_set_size_wildcarded (source, TRUE);
      gtk_icon_set_add_source (set, source);
    }

  gtk_icon_source_free (source);
}

static void
add_sized_with_same_fallback (GtkIconFactory *factory,
			      const guchar   *inline_data,
			      GtkIconSize     size,
			      const gchar    *stock_id)
{
  GtkIconSet *set;
  gboolean    fallback = FALSE;

  set = gtk_icon_factory_lookup (factory, stock_id);

  if (! set)
    {
      set = gtk_icon_set_new ();
      gtk_icon_factory_add (factory, stock_id, set);     
      gtk_icon_set_unref (set);

      fallback = TRUE;
    }

  icon_set_from_inline (set, inline_data, size, fallback);
}


static GtkStockItem gimp_stock_items[] =
{
  { GIMP_STOCK_ANCHOR,         N_("Anchor"),          0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_DUPLICATE,      N_("_Duplicate"),      0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_EDIT,           N_("_Edit"),           0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_LINKED,         N_("Linked"),          0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_PASTE_AS_NEW,   N_("Paste as New"),    0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_PASTE_INTO,     N_("Paste Into"),      0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_RESET,          N_("_Reset"),          0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_VISIBLE,        N_("Visible"),         0, 0, LIBGIMP_DOMAIN },

  { GIMP_STOCK_GRAVITY_EAST,             NULL,        0, 0, LIBGIMP_DOMAIN }, 
  { GIMP_STOCK_GRAVITY_NORTH,            NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_GRAVITY_NORTH_EAST,       NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_GRAVITY_NORTH_WEST,       NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_GRAVITY_SOUTH,            NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_GRAVITY_SOUTH_EAST,       NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_GRAVITY_SOUTH_WEST,       NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_GRAVITY_WEST,             NULL,        0, 0, LIBGIMP_DOMAIN },

  { GIMP_STOCK_HCHAIN,                   NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_HCHAIN_BROKEN,            NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_VCHAIN,                   NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_VCHAIN_BROKEN,            NULL,        0, 0, LIBGIMP_DOMAIN },

  { GIMP_STOCK_SELECTION_REPLACE,        NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_SELECTION_ADD,            NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_SELECTION_SUBTRACT,       NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_SELECTION_INTERSECT,      NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_SELECTION_STROKE,         NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_SELECTION_TO_CHANNEL,     NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_SELECTION_TO_PATH,        NULL,        0, 0, LIBGIMP_DOMAIN },

  { GIMP_STOCK_PATH_STROKE,              NULL,        0, 0, LIBGIMP_DOMAIN },

  { GIMP_STOCK_CURVE_FREE,               NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_CURVE_SMOOTH,             NULL,        0, 0, LIBGIMP_DOMAIN },

  { GIMP_STOCK_COLOR_PICKER_BLACK,       NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_COLOR_PICKER_GRAY,        NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_COLOR_PICKER_WHITE,       NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_COLOR_TRIANGLE,           NULL,        0, 0, LIBGIMP_DOMAIN },

  { GIMP_STOCK_CHAR_PICKER,              NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_LETTER_SPACING, N_("L_etter Spacing"), 0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_LINE_SPACING,   N_("L_ine Spacing"),   0, 0, LIBGIMP_DOMAIN },

  { GIMP_STOCK_CONVERT_RGB,              NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_CONVERT_GRAYSCALE,        NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_CONVERT_INDEXED,          NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_INVERT,                   NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_MERGE_DOWN,               NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_LAYER_TO_IMAGESIZE,       NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_PLUGIN,                   NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_RESHOW_FILTER,            NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_ROTATE_90,                NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_ROTATE_180,               NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_ROTATE_270,               NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_RESIZE,         N_("_Resize"),         0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_SCALE,          N_("_Scale"),          0, 0, LIBGIMP_DOMAIN },

  { GIMP_STOCK_TOOL_OPTIONS,             NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_DEVICE_STATUS,            NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_IMAGES,                   NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_LAYERS,                   NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_CHANNELS,                 NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_PATHS,                    NULL,        0, 0, LIBGIMP_DOMAIN },

  { GIMP_STOCK_IMAGE,                    NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_LAYER,                    NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TEXT_LAYER,               NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_CHANNEL,                  NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_CHANNEL_RED,              NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_CHANNEL_GREEN,            NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_CHANNEL_BLUE,             NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_CHANNEL_GRAY,             NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_CHANNEL_ALPHA,            NULL,        0, 0, LIBGIMP_DOMAIN },

  { GIMP_STOCK_SELECTION_ALL,            NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_SELECTION_NONE,           NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_SELECTION_GROW,           NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_SELECTION_SHRINK,         NULL,        0, 0, LIBGIMP_DOMAIN },

  { GIMP_STOCK_NAVIGATION,               NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_QMASK_OFF,                NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_QMASK_ON,                 NULL,        0, 0, LIBGIMP_DOMAIN },

  { GIMP_STOCK_LIST,                     NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_GRID,                     NULL,        0, 0, LIBGIMP_DOMAIN },

  { GIMP_STOCK_ERROR,                    NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_INFO,                     NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_QUESTION,                 NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_WARNING,                  NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_WILBER,                   NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_WILBER_EEK,               NULL,        0, 0, LIBGIMP_DOMAIN },

  { GIMP_STOCK_TOOL_AIRBRUSH,            NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_BEZIER_SELECT,       NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_BLEND,               NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_BLUR,                NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_BRIGHTNESS_CONTRAST, NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_BUCKET_FILL,         NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_BY_COLOR_SELECT,     NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_CLONE,               NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_COLOR_BALANCE,       NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_COLOR_PICKER,        NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_CROP,         N_("Crop"),         0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_CURVES,              NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_DODGE,               NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_ELLIPSE_SELECT,      NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_ERASER,              NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_FLIP,                NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_FREE_SELECT,         NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_FUZZY_SELECT,        NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_HISTOGRAM,           NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_HUE_SATURATION,      NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_INK,                 NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_ISCISSORS,           NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_LEVELS,              NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_MEASURE,             NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_MOVE,                NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_PAINTBRUSH,          NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_PATH,                NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_PENCIL,              NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_PERSPECTIVE,  N_("_Transform"),   0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_POSTERIZE,           NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_RECT_SELECT,         NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_ROTATE,       N_("_Rotate"),      0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_SCALE,        N_("_Scale"),       0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_SHEAR,        N_("_Shear"),       0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_SMUDGE,              NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_TEXT,                NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_THRESHOLD,           NULL,        0, 0, LIBGIMP_DOMAIN },
  { GIMP_STOCK_TOOL_ZOOM,                NULL,        0, 0, LIBGIMP_DOMAIN }
};

static struct
{
  const gchar   *stock_id;
  gconstpointer  inline_data;
}
gimp_stock_button_pixbufs[] =
{
  { GIMP_STOCK_ANCHOR,                   stock_anchor_16                   },
  { GIMP_STOCK_DUPLICATE,                stock_duplicate_16                },
  { GIMP_STOCK_EDIT,                     stock_edit_16                     },
  { GIMP_STOCK_PASTE_AS_NEW,             stock_paste_as_new_16             },
  { GIMP_STOCK_PASTE_INTO,               stock_paste_into_16               },
  { GIMP_STOCK_RESET,                    stock_reset_16                    },

  { GIMP_STOCK_GRAVITY_EAST,             stock_gravity_east_24             },
  { GIMP_STOCK_GRAVITY_NORTH,            stock_gravity_north_24            },
  { GIMP_STOCK_GRAVITY_NORTH_EAST,       stock_gravity_north_east_24       },
  { GIMP_STOCK_GRAVITY_NORTH_WEST,       stock_gravity_north_west_24       },
  { GIMP_STOCK_GRAVITY_SOUTH,            stock_gravity_south_24            },
  { GIMP_STOCK_GRAVITY_SOUTH_EAST,       stock_gravity_south_east_24       },
  { GIMP_STOCK_GRAVITY_SOUTH_WEST,       stock_gravity_south_west_24       },

  { GIMP_STOCK_HCHAIN,                   stock_hchain_24                   },
  { GIMP_STOCK_HCHAIN_BROKEN,            stock_hchain_broken_24            },
  { GIMP_STOCK_VCHAIN,                   stock_vchain_24                   },
  { GIMP_STOCK_VCHAIN_BROKEN,            stock_vchain_broken_24            },

  { GIMP_STOCK_SELECTION_REPLACE,        stock_selection_replace_16        },
  { GIMP_STOCK_SELECTION_ADD,            stock_selection_add_16            },
  { GIMP_STOCK_SELECTION_SUBTRACT,       stock_selection_subtract_16       },
  { GIMP_STOCK_SELECTION_INTERSECT,      stock_selection_intersect_16      },
  { GIMP_STOCK_SELECTION_STROKE,         stock_selection_stroke_16         },
  { GIMP_STOCK_SELECTION_TO_CHANNEL,     stock_selection_to_channel_16     },
  { GIMP_STOCK_SELECTION_TO_PATH,        stock_selection_to_path_16        },

  { GIMP_STOCK_PATH_STROKE,              stock_path_stroke_16              },

  { GIMP_STOCK_CURVE_FREE,               stock_curve_free_16               },
  { GIMP_STOCK_CURVE_SMOOTH,             stock_curve_smooth_16             },

  { GIMP_STOCK_COLOR_PICKER_BLACK,       stock_color_picker_black_18       },
  { GIMP_STOCK_COLOR_PICKER_GRAY,        stock_color_picker_gray_18        },
  { GIMP_STOCK_COLOR_PICKER_WHITE,       stock_color_picker_white_18       },
  { GIMP_STOCK_COLOR_TRIANGLE,           stock_color_triangle_16           },

  { GIMP_STOCK_CHAR_PICKER,              stock_char_picker_22              },
  { GIMP_STOCK_LETTER_SPACING,           stock_letter_spacing_22           },
  { GIMP_STOCK_LINE_SPACING,             stock_line_spacing_22             },

  { GIMP_STOCK_IMAGES,                   stock_images_24                   },
  { GIMP_STOCK_LAYERS,                   stock_layers_24                   },
  { GIMP_STOCK_CHANNELS,                 stock_channels_24                 },
  { GIMP_STOCK_PATHS,                    stock_paths_22                    },

  { GIMP_STOCK_CHANNEL,                  stock_channel_24                  },
  { GIMP_STOCK_CHANNEL_RED,              stock_channel_red_24              },
  { GIMP_STOCK_CHANNEL_GREEN,            stock_channel_green_24            },
  { GIMP_STOCK_CHANNEL_BLUE,             stock_channel_blue_24             },
  { GIMP_STOCK_CHANNEL_GRAY,             stock_channel_gray_24             },
  { GIMP_STOCK_CHANNEL_ALPHA,            stock_channel_alpha_24            },
  { GIMP_STOCK_IMAGE,                    stock_image_24                    },
  { GIMP_STOCK_LAYER,                    stock_layer_24                    },
  { GIMP_STOCK_TEXT_LAYER,               stock_text_layer_24               },

  { GIMP_STOCK_LINKED,                   stock_linked_20                   },
  { GIMP_STOCK_VISIBLE,                  stock_eye_20                      },

  { GIMP_STOCK_TOOL_AIRBRUSH,            stock_tool_airbrush_22            },
  { GIMP_STOCK_TOOL_BEZIER_SELECT,       stock_tool_bezier_select_22       },
  { GIMP_STOCK_TOOL_BLEND,               stock_tool_blend_22               },
  { GIMP_STOCK_TOOL_BLUR,                stock_tool_blur_22                },
  { GIMP_STOCK_TOOL_BRIGHTNESS_CONTRAST, stock_tool_brightness_contrast_22 },
  { GIMP_STOCK_TOOL_BUCKET_FILL,         stock_tool_bucket_fill_22         },
  { GIMP_STOCK_TOOL_BY_COLOR_SELECT,     stock_tool_by_color_select_22     },
  { GIMP_STOCK_TOOL_CLONE,               stock_tool_clone_22               },
  { GIMP_STOCK_TOOL_COLOR_BALANCE,       stock_tool_color_balance_22       },
  { GIMP_STOCK_TOOL_COLOR_PICKER,        stock_tool_color_picker_22        },
  { GIMP_STOCK_TOOL_CROP,                stock_tool_crop_22                },
  { GIMP_STOCK_TOOL_CURVES,              stock_tool_curves_22              },
  { GIMP_STOCK_TOOL_DODGE,               stock_tool_dodge_22               },
  { GIMP_STOCK_TOOL_ELLIPSE_SELECT,      stock_tool_ellipse_select_22      },
  { GIMP_STOCK_TOOL_ERASER,              stock_tool_eraser_22              },
  { GIMP_STOCK_TOOL_FLIP,                stock_tool_flip_22                },
  { GIMP_STOCK_TOOL_FREE_SELECT,         stock_tool_free_select_22         },
  { GIMP_STOCK_TOOL_FUZZY_SELECT,        stock_tool_fuzzy_select_22        },
  { GIMP_STOCK_TOOL_HISTOGRAM,           stock_tool_histogram_22           },
  { GIMP_STOCK_TOOL_HUE_SATURATION,      stock_tool_hue_saturation_22      },
  { GIMP_STOCK_TOOL_INK,                 stock_tool_ink_22                 },
  { GIMP_STOCK_TOOL_ISCISSORS,           stock_tool_iscissors_22           },
  { GIMP_STOCK_TOOL_LEVELS,              stock_tool_levels_22              },
  { GIMP_STOCK_TOOL_MEASURE,             stock_tool_measure_22             },
  { GIMP_STOCK_TOOL_MOVE,                stock_tool_move_22                },
  { GIMP_STOCK_TOOL_PAINTBRUSH,          stock_tool_paintbrush_22          },
  { GIMP_STOCK_TOOL_PENCIL,              stock_tool_pencil_22              },
  { GIMP_STOCK_TOOL_PERSPECTIVE,         stock_tool_perspective_22         },
  { GIMP_STOCK_TOOL_POSTERIZE,           stock_tool_posterize_22           },
  { GIMP_STOCK_TOOL_RECT_SELECT,         stock_tool_rect_select_22         },
  { GIMP_STOCK_TOOL_ROTATE,              stock_tool_rotate_22              },
  { GIMP_STOCK_TOOL_SCALE,               stock_tool_scale_22               },
  { GIMP_STOCK_TOOL_SHEAR,               stock_tool_shear_22               },
  { GIMP_STOCK_TOOL_SMUDGE,              stock_tool_smudge_22              },
  { GIMP_STOCK_TOOL_TEXT,                stock_tool_text_22                },
  { GIMP_STOCK_TOOL_THRESHOLD,           stock_tool_threshold_22           },
  { GIMP_STOCK_TOOL_ZOOM,                stock_tool_zoom_22                },

  { GIMP_STOCK_TOOL_PATH,                stock_tool_path_22                },

  { GIMP_STOCK_INFO,                     stock_info_24                     },
  { GIMP_STOCK_WARNING,                  stock_warning_24                  }
};

static struct
{
  const gchar   *stock_id;
  gconstpointer  inline_data;
}
gimp_stock_menu_pixbufs[] =
{
  { GIMP_STOCK_CONVERT_RGB,              stock_convert_rgb_16              },
  { GIMP_STOCK_CONVERT_GRAYSCALE,        stock_convert_grayscale_16        },
  { GIMP_STOCK_CONVERT_INDEXED,          stock_convert_indexed_16          },
  { GIMP_STOCK_INVERT,                   stock_invert_16                   },
  { GIMP_STOCK_MERGE_DOWN,               stock_merge_down_16               },
  { GIMP_STOCK_LAYER_TO_IMAGESIZE,       stock_layer_to_imagesize_16       },
  { GIMP_STOCK_PLUGIN,                   stock_plugin_16                   },
  { GIMP_STOCK_RESHOW_FILTER,            stock_reshow_filter_16            },
  { GIMP_STOCK_ROTATE_90,                stock_rotate_90_16                },
  { GIMP_STOCK_ROTATE_180,               stock_rotate_180_16               },
  { GIMP_STOCK_ROTATE_270,               stock_rotate_270_16               },
  { GIMP_STOCK_RESIZE,                   stock_resize_16                   },
  { GIMP_STOCK_SCALE,                    stock_scale_16                    },

  { GIMP_STOCK_TOOL_OPTIONS,             stock_tool_options_16             },
  { GIMP_STOCK_DEVICE_STATUS,            stock_device_status_16            },
  { GIMP_STOCK_IMAGES,                   stock_images_16                   },
  { GIMP_STOCK_LAYERS,                   stock_layers_16                   },
  { GIMP_STOCK_CHANNELS,                 stock_channels_16                 },
  { GIMP_STOCK_PATHS,                    stock_paths_16                    },

  { GIMP_STOCK_CHANNEL,                  stock_channel_16                  },
  { GIMP_STOCK_CHANNEL_RED,              stock_channel_red_16              },
  { GIMP_STOCK_CHANNEL_GREEN,            stock_channel_green_16            },
  { GIMP_STOCK_CHANNEL_BLUE,             stock_channel_blue_16             },
  { GIMP_STOCK_CHANNEL_GRAY,             stock_channel_gray_16             },
  { GIMP_STOCK_CHANNEL_ALPHA,            stock_channel_alpha_16            },
  { GIMP_STOCK_IMAGE,                    stock_image_16                    },
  { GIMP_STOCK_LAYER,                    stock_layer_16                    },
  { GIMP_STOCK_TEXT_LAYER,               stock_text_layer_16               },

  { GIMP_STOCK_LINKED,                   stock_linked_12                   },
  { GIMP_STOCK_VISIBLE,                  stock_eye_12                      },

  { GIMP_STOCK_SELECTION_ALL,            stock_selection_all_16            },
  { GIMP_STOCK_SELECTION_NONE,           stock_selection_none_16           },
  { GIMP_STOCK_SELECTION_GROW,           stock_selection_grow_16           },
  { GIMP_STOCK_SELECTION_SHRINK,         stock_selection_shrink_16         },

  { GIMP_STOCK_NAVIGATION,               stock_navigation_16               },
  { GIMP_STOCK_QMASK_OFF,                stock_qmask_off_16                },
  { GIMP_STOCK_QMASK_ON,                 stock_qmask_on_16                 },

  { GIMP_STOCK_LIST,                     stock_list_16                     },
  { GIMP_STOCK_GRID,                     stock_grid_16                     },

  { GIMP_STOCK_DEFAULT_COLORS,           stock_default_colors_12           },
  { GIMP_STOCK_SWAP_COLORS,              stock_swap_colors_12              },

  { GIMP_STOCK_TOOL_AIRBRUSH,            stock_tool_airbrush_16            },
  { GIMP_STOCK_TOOL_BEZIER_SELECT,       stock_tool_bezier_select_16       },
  { GIMP_STOCK_TOOL_BLEND,               stock_tool_blend_16               },
  { GIMP_STOCK_TOOL_BLUR,                stock_tool_blur_16                },
  { GIMP_STOCK_TOOL_BRIGHTNESS_CONTRAST, stock_tool_brightness_contrast_16 },
  { GIMP_STOCK_TOOL_BUCKET_FILL,         stock_tool_bucket_fill_16         },
  { GIMP_STOCK_TOOL_BY_COLOR_SELECT,     stock_tool_by_color_select_16     },
  { GIMP_STOCK_TOOL_CLONE,               stock_tool_clone_16               },
  { GIMP_STOCK_TOOL_COLOR_BALANCE,       stock_tool_color_balance_16       },
  { GIMP_STOCK_TOOL_COLOR_PICKER,        stock_tool_color_picker_16        },
  { GIMP_STOCK_TOOL_CROP,                stock_tool_crop_16                },
  { GIMP_STOCK_TOOL_CURVES,              stock_tool_curves_16              },
  { GIMP_STOCK_TOOL_DODGE,               stock_tool_dodge_16               },
  { GIMP_STOCK_TOOL_ELLIPSE_SELECT,      stock_tool_ellipse_select_16      },
  { GIMP_STOCK_TOOL_ERASER,              stock_tool_eraser_16              },
  { GIMP_STOCK_TOOL_FLIP,                stock_tool_flip_16                },
  { GIMP_STOCK_TOOL_FREE_SELECT,         stock_tool_free_select_16         },
  { GIMP_STOCK_TOOL_FUZZY_SELECT,        stock_tool_fuzzy_select_16        },
  { GIMP_STOCK_TOOL_HISTOGRAM,           stock_tool_histogram_16           },
  { GIMP_STOCK_TOOL_HUE_SATURATION,      stock_tool_hue_saturation_16      },
  { GIMP_STOCK_TOOL_INK,                 stock_tool_ink_16                 },
  { GIMP_STOCK_TOOL_ISCISSORS,           stock_tool_iscissors_16           },
  { GIMP_STOCK_TOOL_LEVELS,              stock_tool_levels_16              },
  { GIMP_STOCK_TOOL_MEASURE,             stock_tool_measure_16             },
  { GIMP_STOCK_TOOL_MOVE,                stock_tool_move_16                },
  { GIMP_STOCK_TOOL_PAINTBRUSH,          stock_tool_paintbrush_16          },
  { GIMP_STOCK_TOOL_PENCIL,              stock_tool_pencil_16              },
  { GIMP_STOCK_TOOL_PERSPECTIVE,         stock_tool_perspective_16         },
  { GIMP_STOCK_TOOL_POSTERIZE,           stock_tool_posterize_16           },
  { GIMP_STOCK_TOOL_RECT_SELECT,         stock_tool_rect_select_16         },
  { GIMP_STOCK_TOOL_ROTATE,              stock_tool_rotate_16              },
  { GIMP_STOCK_TOOL_SCALE,               stock_tool_scale_16               },
  { GIMP_STOCK_TOOL_SHEAR,               stock_tool_shear_16               },
  { GIMP_STOCK_TOOL_SMUDGE,              stock_tool_smudge_16              },
  { GIMP_STOCK_TOOL_TEXT,                stock_tool_text_16                },
  { GIMP_STOCK_TOOL_THRESHOLD,           stock_tool_threshold_16           },
  { GIMP_STOCK_TOOL_ZOOM,                stock_tool_zoom_16                },

  { GIMP_STOCK_INFO,                     stock_info_16                     },
  { GIMP_STOCK_WARNING,                  stock_warning_16                  },
  { GIMP_STOCK_WILBER,                   stock_wilber_16                   }
};

static struct
{
  const gchar   *stock_id;
  gconstpointer  inline_data;
}
gimp_stock_dnd_pixbufs[] =
{
  { GIMP_STOCK_CHANNEL,              stock_channel_32              },
  { GIMP_STOCK_CHANNEL_RED,          stock_channel_red_32          },
  { GIMP_STOCK_CHANNEL_GREEN,        stock_channel_green_32        },
  { GIMP_STOCK_CHANNEL_BLUE,         stock_channel_blue_32         },
  { GIMP_STOCK_CHANNEL_GRAY,         stock_channel_gray_32         },
  { GIMP_STOCK_CHANNEL_ALPHA,        stock_channel_alpha_32        },
  { GIMP_STOCK_IMAGE,                stock_image_32                },
  { GIMP_STOCK_LAYER,                stock_layer_32                },
  { GIMP_STOCK_TEXT_LAYER,           stock_text_layer_32           }
};

static struct
{
  const gchar   *stock_id;
  gconstpointer  inline_data;
}
gimp_stock_dialog_pixbufs[] =
{
  { GIMP_STOCK_CHANNEL,              stock_channel_48              },
  { GIMP_STOCK_CHANNEL_RED,          stock_channel_red_48          },
  { GIMP_STOCK_CHANNEL_GREEN,        stock_channel_green_48        },
  { GIMP_STOCK_CHANNEL_BLUE,         stock_channel_blue_48         },
  { GIMP_STOCK_CHANNEL_GRAY,         stock_channel_gray_48         },
  { GIMP_STOCK_CHANNEL_ALPHA,        stock_channel_alpha_48        },
  { GIMP_STOCK_IMAGE,                stock_image_48                },
  { GIMP_STOCK_LAYER,                stock_layer_48                },
  { GIMP_STOCK_TEXT_LAYER,           stock_text_layer_48           },

  { GIMP_STOCK_ERROR,                stock_error_64                },
  { GIMP_STOCK_INFO,                 stock_info_64                 },
  { GIMP_STOCK_QUESTION,             stock_question_64             },
  { GIMP_STOCK_WARNING,              stock_warning_64              },
  { GIMP_STOCK_WILBER,               stock_wilber_64               },
  { GIMP_STOCK_WILBER_EEK,           stock_wilber_eek_64           }
};

void
gimp_stock_init (void)
{
  static gboolean initialized = FALSE;

  gint i;

  if (initialized)
    return;

  gimp_stock_factory = gtk_icon_factory_new ();

  for (i = 0; i < G_N_ELEMENTS (gimp_stock_dialog_pixbufs); i++)
    {
      add_sized_with_same_fallback (gimp_stock_factory,
				    gimp_stock_dialog_pixbufs[i].inline_data,
				    GTK_ICON_SIZE_DIALOG,
				    gimp_stock_dialog_pixbufs[i].stock_id);
    }

  for (i = 0; i < G_N_ELEMENTS (gimp_stock_dnd_pixbufs); i++)
    {
      add_sized_with_same_fallback (gimp_stock_factory,
				    gimp_stock_dnd_pixbufs[i].inline_data,
				    GTK_ICON_SIZE_DND,
				    gimp_stock_dnd_pixbufs[i].stock_id);
    }

  for (i = 0; i < G_N_ELEMENTS (gimp_stock_button_pixbufs); i++)
    {
      add_sized_with_same_fallback (gimp_stock_factory,
				    gimp_stock_button_pixbufs[i].inline_data,
				    GTK_ICON_SIZE_BUTTON,
				    gimp_stock_button_pixbufs[i].stock_id);
    }

  for (i = 0; i < G_N_ELEMENTS (gimp_stock_menu_pixbufs); i++)
    {
      add_sized_with_same_fallback (gimp_stock_factory,
				    gimp_stock_menu_pixbufs[i].inline_data,
				    GTK_ICON_SIZE_MENU,
				    gimp_stock_menu_pixbufs[i].stock_id);
    }

  gtk_icon_factory_add_default (gimp_stock_factory);

  gtk_stock_add_static (gimp_stock_items, G_N_ELEMENTS (gimp_stock_items));

  initialized = TRUE;
}
