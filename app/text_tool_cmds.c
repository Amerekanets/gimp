/* The GIMP -- an image manipulation program
 * Copyright (C) 1995-1999 Spencer Kimball and Peter Mattis
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

/* NOTE: This file is autogenerated by pdbgen.pl. */

#include "procedural_db.h"

#include <stdio.h>
#include <string.h>

#include "drawable.h"
#include "layer.h"
#include "text_tool.h"

static ProcRecord text_fontname_proc;
static ProcRecord text_get_extents_fontname_proc;
static ProcRecord text_proc;
static ProcRecord text_get_extents_proc;

void
register_text_tool_procs (void)
{
  procedural_db_register (&text_fontname_proc);
  procedural_db_register (&text_get_extents_fontname_proc);
  procedural_db_register (&text_proc);
  procedural_db_register (&text_get_extents_proc);
}

static gchar *
text_xlfd_insert_size (gchar    *fontname,
		       gdouble   size,
		       SizeType  metric,
		       gboolean  antialias)
{
  gchar *newfont, *workfont;
  gchar buffer[16];
  gint pos = 0;

  if (size <= 0)
    return NULL;

  if (antialias)
    size *= SUPERSAMPLE;

  if (metric == POINTS)
    size *= 10;

  sprintf (buffer, "%d", (int) size);

  newfont = workfont = g_new (char, strlen (fontname) + 16);

  while (*fontname)
    {
      *workfont++ = *fontname;

      if (*fontname++ == '-')
	{
	  pos++;
	  if ((pos == 7 && metric == PIXELS) ||
	      (pos == 8 && metric == POINTS))
	    {
	      int len = strlen (buffer);
	      memcpy (workfont, buffer, len);
	      workfont += len;

	      while (*fontname != '-')
		fontname++;
	    }
	}
    }

   *workfont = '\0';
   return newfont;
}

static gchar *
text_xlfd_create (gchar    *foundry,
		  gchar    *family,
		  gchar    *weight,
		  gchar    *slant,
		  gchar    *set_width,
		  gchar    *spacing,
		  gchar    *registry,
		  gchar    *encoding)
{
  /* create the fontname */
  return g_strdup_printf ("-%s-%s-%s-%s-%s-*-*-*-*-*-%s-*-%s-%s",
			  foundry, family, weight, slant, set_width,
			  spacing, registry, encoding);
}

static Argument *
text_fontname_invoker (Argument *args)
{
  gboolean success = TRUE;
  Argument *return_args;
  GimpImage *gimage;
  GimpDrawable *drawable;
  gdouble x;
  gdouble y;
  gchar *text;
  gint32 border;
  gboolean antialias;
  gdouble size;
  gint32 size_type;
  gchar *fontname;
  GimpLayer *text_layer = NULL;
  gchar *real_fontname;

  gimage = pdb_id_to_image (args[0].value.pdb_int);
  if (gimage == NULL)
    success = FALSE;

  drawable = gimp_drawable_get_ID (args[1].value.pdb_int);

  x = args[2].value.pdb_float;

  y = args[3].value.pdb_float;

  text = (gchar *) args[4].value.pdb_pointer;
  if (text == NULL)
    success = FALSE;

  border = args[5].value.pdb_int;
  if (border < -1)
    success = FALSE;

  antialias = args[6].value.pdb_int ? TRUE : FALSE;

  size = args[7].value.pdb_float;
  if (size <= 0.0)
    success = FALSE;

  size_type = args[8].value.pdb_int;
  if (size_type < PIXELS || size_type > POINTS)
    success = FALSE;

  fontname = (gchar *) args[9].value.pdb_pointer;
  if (fontname == NULL)
    success = FALSE;

  if (success)
    {
      real_fontname = text_xlfd_insert_size (fontname, size, size_type,
					     antialias);
    
      text_layer = text_render (gimage, drawable, x, y, real_fontname, text,
				border, antialias);
    
      if (text_layer == NULL)
	success = FALSE;
    
      g_free (real_fontname);
    }

  return_args = procedural_db_return_args (&text_fontname_proc, success);

  if (success)
    return_args[1].value.pdb_int = drawable_ID (GIMP_DRAWABLE (text_layer));

  return return_args;
}

static ProcArg text_fontname_inargs[] =
{
  {
    PDB_IMAGE,
    "image",
    "The image"
  },
  {
    PDB_DRAWABLE,
    "drawable",
    "The affected drawable: (-1 for a new text layer)"
  },
  {
    PDB_FLOAT,
    "x",
    "The x coordinate for the left of the text bounding box"
  },
  {
    PDB_FLOAT,
    "y",
    "The y coordinate for the top of the text bounding box"
  },
  {
    PDB_STRING,
    "text",
    "The text to generate"
  },
  {
    PDB_INT32,
    "border",
    "The size of the border: -1 <= border"
  },
  {
    PDB_INT32,
    "antialias",
    "Antialiasing (TRUE or FALSE)"
  },
  {
    PDB_FLOAT,
    "size",
    "The size of text in either pixels or points"
  },
  {
    PDB_INT32,
    "size_type",
    "The units of specified size: PIXELS (0) or POINTS (1)"
  },
  {
    PDB_STRING,
    "fontname",
    "The fontname (conforming to the X Logical Font Description Conventions)"
  }
};

static ProcArg text_fontname_outargs[] =
{
  {
    PDB_LAYER,
    "text_layer",
    "The new text layer"
  }
};

static ProcRecord text_fontname_proc =
{
  "gimp_text_fontname",
  "Add text at the specified location as a floating selection or a new layer.",
  "This tool requires font information as a fontname conforming to the 'X Logical Font Description Conventions'. You can specify the fontsize in units of pixels or points, and the appropriate metric is specified using the size_type argument. The x and y parameters together control the placement of the new text by specifying the upper left corner of the text bounding box. If the antialias parameter is non-zero, the generated text will blend more smoothly with underlying layers. This option requires more time and memory to compute than non-antialiased text; the resulting floating selection or layer, however, will require the same amount of memory with or without antialiasing. If the specified drawable parameter is valid, the text will be created as a floating selection attached to the drawable. If the drawable parameter is not valid (-1), the text will appear as a new layer. Finally, a border can be specified around the final rendered text. The border is measured in pixels.",
  "Martin Edlman & Sven Neumann",
  "Spencer Kimball & Peter Mattis",
  "1998",
  PDB_INTERNAL,
  10,
  text_fontname_inargs,
  1,
  text_fontname_outargs,
  { { text_fontname_invoker } }
};

static Argument *
text_get_extents_fontname_invoker (Argument *args)
{
  gboolean success = TRUE;
  Argument *return_args;
  gchar *text;
  gdouble size;
  gint32 size_type;
  gchar *fontname;
  gint32 width;
  gint32 height;
  gint32 ascent;
  gint32 descent;
  gchar *real_fontname;

  text = (gchar *) args[0].value.pdb_pointer;
  if (text == NULL)
    success = FALSE;

  size = args[1].value.pdb_float;
  if (size <= 0.0)
    success = FALSE;

  size_type = args[2].value.pdb_int;
  if (size_type < PIXELS || size_type > POINTS)
    success = FALSE;

  fontname = (gchar *) args[3].value.pdb_pointer;
  if (fontname == NULL)
    success = FALSE;

  if (success)
    {
      real_fontname = text_xlfd_insert_size (fontname, size, size_type, FALSE);
    
      success = text_get_extents (real_fontname, text,
				  &width, &height,
				  &ascent, &descent);
    
      g_free (real_fontname);
    }

  return_args = procedural_db_return_args (&text_get_extents_fontname_proc, success);

  if (success)
    {
      return_args[1].value.pdb_int = width;
      return_args[2].value.pdb_int = height;
      return_args[3].value.pdb_int = ascent;
      return_args[4].value.pdb_int = descent;
    }

  return return_args;
}

static ProcArg text_get_extents_fontname_inargs[] =
{
  {
    PDB_STRING,
    "text",
    "The text to generate"
  },
  {
    PDB_FLOAT,
    "size",
    "The size of text in either pixels or points"
  },
  {
    PDB_INT32,
    "size_type",
    "The units of specified size: PIXELS (0) or POINTS (1)"
  },
  {
    PDB_STRING,
    "fontname",
    "The fontname (conforming to the X Logical Font Description Conventions)"
  }
};

static ProcArg text_get_extents_fontname_outargs[] =
{
  {
    PDB_INT32,
    "width",
    "The width of the specified font"
  },
  {
    PDB_INT32,
    "height",
    "The height of the specified font"
  },
  {
    PDB_INT32,
    "ascent",
    "The ascent of the specified font"
  },
  {
    PDB_INT32,
    "descent",
    "The descent of the specified font"
  }
};

static ProcRecord text_get_extents_fontname_proc =
{
  "gimp_text_get_extents_fontname",
  "Get extents of the bounding box for the specified text.",
  "This tool returns the width and height of a bounding box for the specified text string with the specified font information. Ascent and descent for the specified font are returned as well.",
  "Martin Edlman & Sven Neumann",
  "Spencer Kimball & Peter Mattis",
  "1998",
  PDB_INTERNAL,
  4,
  text_get_extents_fontname_inargs,
  4,
  text_get_extents_fontname_outargs,
  { { text_get_extents_fontname_invoker } }
};

static Argument *
text_invoker (Argument *args)
{
  gboolean success = TRUE;
  int i;
  Argument argv[10];
  gchar *foundry;
  gchar *family;
  gchar *weight;
  gchar *slant;
  gchar *set_width;
  gchar *spacing;
  gchar *registry;
  gchar *encoding;

  foundry = (gchar *) args[9].value.pdb_pointer;
  if (foundry == NULL)
    success = FALSE;

  family = (gchar *) args[10].value.pdb_pointer;
  if (family == NULL)
    success = FALSE;

  weight = (gchar *) args[11].value.pdb_pointer;
  if (weight == NULL)
    success = FALSE;

  slant = (gchar *) args[12].value.pdb_pointer;
  if (slant == NULL)
    success = FALSE;

  set_width = (gchar *) args[13].value.pdb_pointer;
  if (set_width == NULL)
    success = FALSE;

  spacing = (gchar *) args[14].value.pdb_pointer;
  if (spacing == NULL)
    success = FALSE;

  registry = (gchar *) args[15].value.pdb_pointer;
  if (registry == NULL)
    success = FALSE;

  encoding = (gchar *) args[16].value.pdb_pointer;
  if (encoding == NULL)
    success = FALSE;

  if (!success)
    return procedural_db_return_args (&text_proc, FALSE);

  for (i = 0; i < 9; i++)
    argv[i] = args[i];

  argv[9].arg_type = PDB_STRING;
  argv[9].value.pdb_pointer =
    text_xlfd_create (foundry,
		      family,
		      weight,
		      slant,
		      set_width,
		      spacing,
		      registry,
		      encoding);

  return text_fontname_invoker (argv);
}

static ProcArg text_inargs[] =
{
  {
    PDB_IMAGE,
    "image",
    "The image"
  },
  {
    PDB_DRAWABLE,
    "drawable",
    "The affected drawable: (-1 for a new text layer)"
  },
  {
    PDB_FLOAT,
    "x",
    "The x coordinate for the left of the text bounding box"
  },
  {
    PDB_FLOAT,
    "y",
    "The y coordinate for the top of the text bounding box"
  },
  {
    PDB_STRING,
    "text",
    "The text to generate"
  },
  {
    PDB_INT32,
    "border",
    "The size of the border: -1 <= border"
  },
  {
    PDB_INT32,
    "antialias",
    "Antialiasing (TRUE or FALSE)"
  },
  {
    PDB_FLOAT,
    "size",
    "The size of text in either pixels or points"
  },
  {
    PDB_INT32,
    "size_type",
    "The units of specified size: PIXELS (0) or POINTS (1)"
  },
  {
    PDB_STRING,
    "foundry",
    "The font foundry, \"*\" for any"
  },
  {
    PDB_STRING,
    "family",
    "The font family, \"*\" for any"
  },
  {
    PDB_STRING,
    "weight",
    "The font weight, \"*\" for any"
  },
  {
    PDB_STRING,
    "slant",
    "The font slant, \"*\" for any"
  },
  {
    PDB_STRING,
    "set_width",
    "The font set-width, \"*\" for any"
  },
  {
    PDB_STRING,
    "spacing",
    "The font spacing, \"*\" for any"
  },
  {
    PDB_STRING,
    "registry",
    "The font registry, \"*\" for any"
  },
  {
    PDB_STRING,
    "encoding",
    "The font encoding, \"*\" for any"
  }
};

static ProcArg text_outargs[] =
{
  {
    PDB_LAYER,
    "text_layer",
    "The new text layer"
  }
};

static ProcRecord text_proc =
{
  "gimp_text",
  "Add text at the specified location as a floating selection or a new layer.",
  "This tool requires font information in the form of nine parameters: size, foundry, family, weight, slant, set_width, spacing, registry, encoding. The font size can either be specified in units of pixels or points, and the appropriate metric is specified using the size_type argument. The x and y parameters together control the placement of the new text by specifying the upper left corner of the text bounding box. If the antialias parameter is non-zero, the generated text will blend more smoothly with underlying layers. This option requires more time and memory to compute than non-antialiased text; the resulting floating selection or layer, however, will require the same amount of memory with or without antialiasing. If the specified drawable parameter is valid, the text will be created as a floating selection attached to the drawable. If the drawable parameter is not valid (-1), the text will appear as a new layer. Finally, a border can be specified around the final rendered text."
  "The border is measured in pixels.",
  "Martin Edlman",
  "Spencer Kimball & Peter Mattis",
  "1998",
  PDB_INTERNAL,
  17,
  text_inargs,
  1,
  text_outargs,
  { { text_invoker } }
};

static Argument *
text_get_extents_invoker (Argument *args)
{
  gboolean success = TRUE;
  int i;
  Argument argv[4];
  gchar *foundry;
  gchar *family;
  gchar *weight;
  gchar *slant;
  gchar *set_width;
  gchar *spacing;
  gchar *registry;
  gchar *encoding;

  foundry = (gchar *) args[3].value.pdb_pointer;
  if (foundry == NULL)
    success = FALSE;

  family = (gchar *) args[4].value.pdb_pointer;
  if (family == NULL)
    success = FALSE;

  weight = (gchar *) args[5].value.pdb_pointer;
  if (weight == NULL)
    success = FALSE;

  slant = (gchar *) args[6].value.pdb_pointer;
  if (slant == NULL)
    success = FALSE;

  set_width = (gchar *) args[7].value.pdb_pointer;
  if (set_width == NULL)
    success = FALSE;

  spacing = (gchar *) args[8].value.pdb_pointer;
  if (spacing == NULL)
    success = FALSE;

  registry = (gchar *) args[9].value.pdb_pointer;
  if (registry == NULL)
    success = FALSE;

  encoding = (gchar *) args[10].value.pdb_pointer;
  if (encoding == NULL)
    success = FALSE;

  if (!success)
    return procedural_db_return_args (&text_get_extents_proc, FALSE);

  for (i = 0; i < 3; i++)
    argv[i] = args[i];

  argv[3].arg_type = PDB_STRING;
  argv[3].value.pdb_pointer =
    text_xlfd_create (foundry,
		      family,
		      weight,
		      slant,
		      set_width,
		      spacing,
		      registry,
		      encoding);

  return text_get_extents_fontname_invoker (argv);
}

static ProcArg text_get_extents_inargs[] =
{
  {
    PDB_STRING,
    "text",
    "The text to generate"
  },
  {
    PDB_FLOAT,
    "size",
    "The size of text in either pixels or points"
  },
  {
    PDB_INT32,
    "size_type",
    "The units of specified size: PIXELS (0) or POINTS (1)"
  },
  {
    PDB_STRING,
    "foundry",
    "The font foundry, \"*\" for any"
  },
  {
    PDB_STRING,
    "family",
    "The font family, \"*\" for any"
  },
  {
    PDB_STRING,
    "weight",
    "The font weight, \"*\" for any"
  },
  {
    PDB_STRING,
    "slant",
    "The font slant, \"*\" for any"
  },
  {
    PDB_STRING,
    "set_width",
    "The font set-width, \"*\" for any"
  },
  {
    PDB_STRING,
    "spacing",
    "The font spacing, \"*\" for any"
  },
  {
    PDB_STRING,
    "registry",
    "The font registry, \"*\" for any"
  },
  {
    PDB_STRING,
    "encoding",
    "The font encoding, \"*\" for any"
  }
};

static ProcArg text_get_extents_outargs[] =
{
  {
    PDB_INT32,
    "width",
    "The width of the specified font"
  },
  {
    PDB_INT32,
    "height",
    "The height of the specified font"
  },
  {
    PDB_INT32,
    "ascent",
    "The ascent of the specified font"
  },
  {
    PDB_INT32,
    "descent",
    "The descent of the specified font"
  }
};

static ProcRecord text_get_extents_proc =
{
  "gimp_text_get_extents",
  "Get extents of the bounding box for the specified text.",
  "This tool returns the width and height of a bounding box for the specified text string with the specified font information. Ascent and descent for the specified font are returned as well.",
  "Martin Edlman",
  "Spencer Kimball & Peter Mattis",
  "1998",
  PDB_INTERNAL,
  11,
  text_get_extents_inargs,
  4,
  text_get_extents_outargs,
  { { text_get_extents_invoker } }
};
