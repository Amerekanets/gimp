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

/* NOTE: This file is autogenerated by enumcode.pl. */

#include "siod.h"

void
init_generated_constants (void)
{
  setvar (cintern ("ADD-WHITE-MASK"), flocons (0), NIL);
  setvar (cintern ("ADD-BLACK-MASK"), flocons (1), NIL);
  setvar (cintern ("ADD-ALPHA-MASK"), flocons (2), NIL);
  setvar (cintern ("ADD-SELECTION-MASK"), flocons (3), NIL);
  setvar (cintern ("ADD-INVERSE-SELECTION-MASK"), flocons (4), NIL);
  setvar (cintern ("ADD-COPY-MASK"), flocons (5), NIL);
  setvar (cintern ("ADD-INVERSE-COPY-MASK"), flocons (6), NIL);

  setvar (cintern ("FG-BG-RGB-MODE"), flocons (0), NIL);
  setvar (cintern ("FG-BG-HSV-MODE"), flocons (1), NIL);
  setvar (cintern ("FG-TRANSPARENT-MODE"), flocons (2), NIL);
  setvar (cintern ("CUSTOM-MODE"), flocons (3), NIL);

  setvar (cintern ("BRUSH-HARD"), flocons (0), NIL);
  setvar (cintern ("BRUSH-SOFT"), flocons (1), NIL);

  setvar (cintern ("FG-BUCKET-FILL"), flocons (0), NIL);
  setvar (cintern ("BG-BUCKET-FILL"), flocons (1), NIL);
  setvar (cintern ("PATTERN-BUCKET-FILL"), flocons (2), NIL);

  setvar (cintern ("VALUE-LUT"), flocons (0), NIL);
  setvar (cintern ("RED-LUT"), flocons (1), NIL);
  setvar (cintern ("GREEN-LUT"), flocons (2), NIL);
  setvar (cintern ("BLUE-LUT"), flocons (3), NIL);
  setvar (cintern ("ALPHA-LUT"), flocons (4), NIL);

  setvar (cintern ("CHANNEL-OP-ADD"), flocons (0), NIL);
  setvar (cintern ("CHANNEL-OP-SUBTRACT"), flocons (1), NIL);
  setvar (cintern ("CHANNEL-OP-REPLACE"), flocons (2), NIL);
  setvar (cintern ("CHANNEL-OP-INTERSECT"), flocons (3), NIL);

  setvar (cintern ("RED-CHANNEL"), flocons (0), NIL);
  setvar (cintern ("GREEN-CHANNEL"), flocons (1), NIL);
  setvar (cintern ("BLUE-CHANNEL"), flocons (2), NIL);
  setvar (cintern ("GRAY-CHANNEL"), flocons (3), NIL);
  setvar (cintern ("INDEXED-CHANNEL"), flocons (4), NIL);
  setvar (cintern ("ALPHA-CHANNEL"), flocons (5), NIL);

  setvar (cintern ("IMAGE-CLONE"), flocons (0), NIL);
  setvar (cintern ("PATTERN-CLONE"), flocons (1), NIL);

  setvar (cintern ("NO-DITHER"), flocons (0), NIL);
  setvar (cintern ("FS-DITHER"), flocons (1), NIL);
  setvar (cintern ("FSLOWBLEED-DITHER"), flocons (2), NIL);
  setvar (cintern ("FIXED-DITHER"), flocons (3), NIL);

  setvar (cintern ("MAKE-PALETTE"), flocons (0), NIL);
  setvar (cintern ("REUSE-PALETTE"), flocons (1), NIL);
  setvar (cintern ("WEB-PALETTE"), flocons (2), NIL);
  setvar (cintern ("MONO-PALETTE"), flocons (3), NIL);
  setvar (cintern ("CUSTOM-PALETTE"), flocons (4), NIL);

  setvar (cintern ("NORMAL-CONVOL"), flocons (0), NIL);
  setvar (cintern ("ABSOLUTE-CONVOL"), flocons (1), NIL);
  setvar (cintern ("NEGATIVE-CONVOL"), flocons (2), NIL);

  setvar (cintern ("BLUR-CONVOLVE"), flocons (0), NIL);
  setvar (cintern ("SHARPEN-CONVOLVE"), flocons (1), NIL);

  setvar (cintern ("DODGE"), flocons (0), NIL);
  setvar (cintern ("BURN"), flocons (1), NIL);

  setvar (cintern ("FOREGROUND-FILL"), flocons (0), NIL);
  setvar (cintern ("BACKGROUND-FILL"), flocons (1), NIL);
  setvar (cintern ("WHITE-FILL"), flocons (2), NIL);
  setvar (cintern ("TRANSPARENT-FILL"), flocons (3), NIL);
  setvar (cintern ("NO-FILL"), flocons (4), NIL);

  setvar (cintern ("GRADIENT-ONCE-FORWARD"), flocons (0), NIL);
  setvar (cintern ("GRADIENT-ONCE-BACKWARD"), flocons (1), NIL);
  setvar (cintern ("GRADIENT-LOOP-SAWTOOTH"), flocons (2), NIL);
  setvar (cintern ("GRADIENT-LOOP-TRIANGLE"), flocons (3), NIL);

  setvar (cintern ("LINEAR"), flocons (0), NIL);
  setvar (cintern ("BILINEAR"), flocons (1), NIL);
  setvar (cintern ("RADIAL"), flocons (2), NIL);
  setvar (cintern ("SQUARE"), flocons (3), NIL);
  setvar (cintern ("CONICAL-SYMMETRIC"), flocons (4), NIL);
  setvar (cintern ("CONICAL-ASYMMETRIC"), flocons (5), NIL);
  setvar (cintern ("SHAPEBURST-ANGULAR"), flocons (6), NIL);
  setvar (cintern ("SHAPEBURST-SPHERICAL"), flocons (7), NIL);
  setvar (cintern ("SHAPEBURST-DIMPLED"), flocons (8), NIL);
  setvar (cintern ("SPIRAL-CLOCKWISE"), flocons (9), NIL);
  setvar (cintern ("SPIRAL-ANTICLOCKWISE"), flocons (10), NIL);

  setvar (cintern ("RGB"), flocons (0), NIL);
  setvar (cintern ("GRAY"), flocons (1), NIL);
  setvar (cintern ("INDEXED"), flocons (2), NIL);

  setvar (cintern ("RGB-IMAGE"), flocons (0), NIL);
  setvar (cintern ("RGBA-IMAGE"), flocons (1), NIL);
  setvar (cintern ("GRAY-IMAGE"), flocons (2), NIL);
  setvar (cintern ("GRAYA-IMAGE"), flocons (3), NIL);
  setvar (cintern ("INDEXED-IMAGE"), flocons (4), NIL);
  setvar (cintern ("INDEXEDA-IMAGE"), flocons (5), NIL);

  setvar (cintern ("NORMAL-MODE"), flocons (0), NIL);
  setvar (cintern ("DISSOLVE-MODE"), flocons (1), NIL);
  setvar (cintern ("BEHIND-MODE"), flocons (2), NIL);
  setvar (cintern ("MULTIPLY-MODE"), flocons (3), NIL);
  setvar (cintern ("SCREEN-MODE"), flocons (4), NIL);
  setvar (cintern ("OVERLAY-MODE"), flocons (5), NIL);
  setvar (cintern ("DIFFERENCE-MODE"), flocons (6), NIL);
  setvar (cintern ("ADDITION-MODE"), flocons (7), NIL);
  setvar (cintern ("SUBTRACT-MODE"), flocons (8), NIL);
  setvar (cintern ("DARKEN-ONLY-MODE"), flocons (9), NIL);
  setvar (cintern ("LIGHTEN-ONLY-MODE"), flocons (10), NIL);
  setvar (cintern ("HUE-MODE"), flocons (11), NIL);
  setvar (cintern ("SATURATION-MODE"), flocons (12), NIL);
  setvar (cintern ("COLOR-MODE"), flocons (13), NIL);
  setvar (cintern ("VALUE-MODE"), flocons (14), NIL);
  setvar (cintern ("DIVIDE-MODE"), flocons (15), NIL);
  setvar (cintern ("DODGE-MODE"), flocons (16), NIL);
  setvar (cintern ("BURN-MODE"), flocons (17), NIL);
  setvar (cintern ("HARDLIGHT-MODE"), flocons (18), NIL);
  setvar (cintern ("COLOR-ERASE-MODE"), flocons (19), NIL);

  setvar (cintern ("MASK-APPLY"), flocons (0), NIL);
  setvar (cintern ("MASK-DISCARD"), flocons (1), NIL);

  setvar (cintern ("EXPAND-AS-NECESSARY"), flocons (0), NIL);
  setvar (cintern ("CLIP-TO-IMAGE"), flocons (1), NIL);
  setvar (cintern ("CLIP-TO-BOTTOM-LAYER"), flocons (2), NIL);
  setvar (cintern ("FLATTEN-IMAGE"), flocons (3), NIL);

  setvar (cintern ("MESSAGE-BOX"), flocons (0), NIL);
  setvar (cintern ("CONSOLE"), flocons (1), NIL);
  setvar (cintern ("ERROR-CONSOLE"), flocons (2), NIL);

  setvar (cintern ("OFFSET-BACKGROUND"), flocons (0), NIL);
  setvar (cintern ("OFFSET-TRANSPARENT"), flocons (1), NIL);

  setvar (cintern ("PDB-INT32"), flocons (0), NIL);
  setvar (cintern ("PDB-INT16"), flocons (1), NIL);
  setvar (cintern ("PDB-INT8"), flocons (2), NIL);
  setvar (cintern ("PDB-FLOAT"), flocons (3), NIL);
  setvar (cintern ("PDB-STRING"), flocons (4), NIL);
  setvar (cintern ("PDB-INT32ARRAY"), flocons (5), NIL);
  setvar (cintern ("PDB-INT16ARRAY"), flocons (6), NIL);
  setvar (cintern ("PDB-INT8ARRAY"), flocons (7), NIL);
  setvar (cintern ("PDB-FLOATARRAY"), flocons (8), NIL);
  setvar (cintern ("PDB-STRINGARRAY"), flocons (9), NIL);
  setvar (cintern ("PDB-COLOR"), flocons (10), NIL);
  setvar (cintern ("PDB-REGION"), flocons (11), NIL);
  setvar (cintern ("PDB-DISPLAY"), flocons (12), NIL);
  setvar (cintern ("PDB-IMAGE"), flocons (13), NIL);
  setvar (cintern ("PDB-LAYER"), flocons (14), NIL);
  setvar (cintern ("PDB-CHANNEL"), flocons (15), NIL);
  setvar (cintern ("PDB-DRAWABLE"), flocons (16), NIL);
  setvar (cintern ("PDB-SELECTION"), flocons (17), NIL);
  setvar (cintern ("PDB-BOUNDARY"), flocons (18), NIL);
  setvar (cintern ("PDB-PATH"), flocons (19), NIL);
  setvar (cintern ("PDB-PARASITE"), flocons (20), NIL);
  setvar (cintern ("PDB-STATUS"), flocons (21), NIL);
  setvar (cintern ("PDB-END"), flocons (22), NIL);

  setvar (cintern ("INTERNAL"), flocons (0), NIL);
  setvar (cintern ("PLUGIN"), flocons (1), NIL);
  setvar (cintern ("EXTENSION"), flocons (2), NIL);
  setvar (cintern ("TEMPORARY"), flocons (3), NIL);

  setvar (cintern ("PDB-EXECUTION-ERROR"), flocons (0), NIL);
  setvar (cintern ("PDB-CALLING-ERROR"), flocons (1), NIL);
  setvar (cintern ("PDB-PASS-THROUGH"), flocons (2), NIL);
  setvar (cintern ("PDB-SUCCESS"), flocons (3), NIL);
  setvar (cintern ("PDB-CANCEL"), flocons (4), NIL);

  setvar (cintern ("PAINT-CONSTANT"), flocons (0), NIL);
  setvar (cintern ("PAINT-INCREMENTAL"), flocons (1), NIL);

  setvar (cintern ("REPEAT-NONE"), flocons (0), NIL);
  setvar (cintern ("REPEAT-SAWTOOTH"), flocons (1), NIL);
  setvar (cintern ("REPEAT-TRIANGULAR"), flocons (2), NIL);

  setvar (cintern ("RUN-INTERACTIVE"), flocons (0), NIL);
  setvar (cintern ("RUN-NONINTERACTIVE"), flocons (1), NIL);
  setvar (cintern ("RUN-WITH-LAST-VALS"), flocons (2), NIL);

  setvar (cintern ("STACK-TRACE-NEVER"), flocons (0), NIL);
  setvar (cintern ("STACK-TRACE-QUERY"), flocons (1), NIL);
  setvar (cintern ("STACK-TRACE-ALWAYS"), flocons (2), NIL);

  setvar (cintern ("SHADOWS"), flocons (0), NIL);
  setvar (cintern ("MIDTONES"), flocons (1), NIL);
  setvar (cintern ("HIGHLIGHTS"), flocons (2), NIL);

  setvar (cintern ("UNIT-PIXEL"), flocons (0), NIL);
  setvar (cintern ("UNIT-INCH"), flocons (1), NIL);
  setvar (cintern ("UNIT-MM"), flocons (2), NIL);
  setvar (cintern ("UNIT-POINT"), flocons (3), NIL);
  setvar (cintern ("UNIT-PICA"), flocons (4), NIL);
  setvar (cintern ("UNIT-END"), flocons (5), NIL);

  setvar (cintern ("ALL-HUES"), flocons (0), NIL);
  setvar (cintern ("RED-HUES"), flocons (1), NIL);
  setvar (cintern ("YELLOW-HUES"), flocons (2), NIL);
  setvar (cintern ("GREEN-HUES"), flocons (3), NIL);
  setvar (cintern ("CYAN-HUES"), flocons (4), NIL);
  setvar (cintern ("BLUE-HUES"), flocons (5), NIL);
  setvar (cintern ("MAGENTA-HUES"), flocons (6), NIL);

  setvar (cintern ("HORIZONTAL"), flocons (0), NIL);
  setvar (cintern ("VERTICAL"), flocons (1), NIL);
  setvar (cintern ("UNKNOWN"), flocons (2), NIL);

  setvar (cintern ("PIXELS"), flocons (0), NIL);
  setvar (cintern ("POINTS"), flocons (1), NIL);

  return;
}
