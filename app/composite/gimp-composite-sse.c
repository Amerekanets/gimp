/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * -*- mode: c tab-width: 2; -*-
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

/* Much of the content of this file are derivative works of David
 * Monniaux which are Copyright (C) 1999, 2001 David Monniaux
 * Tip-o-the-hat to David for pioneering this effort.
 *
 * All of these functions use the mmx and sse registers and expect
 * them to remain intact across multiple asm() constructs.  This may
 * not work in the future, if the compiler allocates mmx/sse registers
 * for it's own use. XXX
 */

#include "config.h"

#ifdef USE_MMX
#ifdef ARCH_X86
#if __GNUC__ >= 3

#include <stdio.h>

#include <glib-object.h>

#include "base/base-types.h"

#include "gimp-composite.h"
#include "gimp-composite-sse.h"


#ifdef USE_SSE
#define pminub(src,dst,tmp)  "pminub " "%%" #src ", %%" #dst
#define pmaxub(src,dst,tmp)  "pmaxub " "%%" #src ", %%" #dst
#else
#define pminub(src,dst,tmp)  "movq %%" #dst ", %%" #tmp ";" "psubusb %%" #src ", %%" #tmp ";" "psubb %%" #tmp ", %%" #dst

#define pmaxub(a,b,tmp)      "movq %%" #a ", %%" #tmp ";" "psubusb %%" #b ", %%" #tmp ";" "paddb %%" #tmp ", %%" #b
#endif


/*
 * Clobbers eax, ecx edx
 */
/*
 * Double-word divide.  Adjusted for subsequent unsigned packing
 * (high-order bit of each word is cleared)
 */
#define pdivwX(dividend,divisor,quotient) "movd %%" #dividend ",%%eax; " \
                                          "movd %%" #divisor  ",%%ecx; " \
                                          "xorl %%edx,%%edx; "           \
                                          "divw %%cx; "                  \
                                          "roll $16, %%eax; "            \
                                          "roll $16, %%ecx; "            \
                                          "xorl %%edx,%%edx; "           \
                                          "divw %%cx; "                  \
                                          "btr $15, %%eax; "             \
                                          "roll $16, %%eax; "            \
                                          "btr $15, %%eax; "             \
                                          "movd %%eax,%%" #quotient ";"

/*
 * Quadword divide.  No adjustment for subsequent unsigned packing
 * (high-order bit of each word is left alone)
 */
#define pdivwqX(dividend,divisor,quotient) "movd   %%" #dividend ",%%eax; " \
                                          "movd   %%" #divisor  ",%%ecx; " \
                                          "xorl   %%edx,%%edx; "           \
                                          "divw   %%cx; "                  \
                                          "roll   $16, %%eax; "            \
                                          "roll   $16, %%ecx; "            \
                                          "xorl   %%edx,%%edx; "           \
                                          "divw   %%cx; "                  \
                                          "roll   $16, %%eax; "            \
                                          "movd   %%eax,%%" #quotient "; " \
                                          "psrlq $32,%%" #dividend ";"     \
                                          "psrlq $32,%%" #divisor ";"      \
                                          "movd   %%" #dividend ",%%eax; " \
                                          "movd   %%" #divisor  ",%%ecx; " \
                                          "xorl   %%edx,%%edx; "           \
                                          "divw   %%cx; "                  \
                                          "roll   $16, %%eax; "            \
                                          "roll   $16, %%ecx; "            \
                                          "xorl   %%edx,%%edx; "           \
                                          "divw   %%cx; "                  \
                                          "roll   $16, %%eax; "            \
                                          "movd   %%eax,%%" #divisor ";"   \
                                          "psllq  $32,%%" #divisor ";"     \
                                          "por    %%" #divisor ",%%" #quotient ";"
   
/*
 * Quadword divide.  Adjusted for subsequent unsigned packing
 * (high-order bit of each word is cleared)
 */
#define pdivwuqX(dividend,divisor,quotient) \
                                          pdivwX(dividend,divisor,quotient) \
                                            "psrlq  $32,%%" #dividend ";"   \
                                            "psrlq  $32,%%" #divisor ";"    \
                                          pdivwX(dividend,divisor,quotient) \
                                          "movd   %%eax,%%" #divisor ";"    \
                                            "psllq  $32,%%" #divisor ";"    \
                                            "por    %%" #divisor ",%%" #quotient ";"

/* equivalent to INT_MULT() macro */
/*
 * opr2 = INT_MULT(opr1, opr2, t)
 *
 * Operates across quad-words
 * Result is left in opr2
 *
 * opr1 = opr1 * opr + w128
 */
#define pmulwX(opr1,opr2,w128) \
                  "\tpmullw    %%"#opr2", %%"#opr1"; " \
                  "\tpaddw     %%"#w128", %%"#opr1"; " \
                  "\tmovq      %%"#opr1", %%"#opr2"; " \
                  "\tpsrlw     $8,        %%"#opr2"; " \
                  "\tpaddw     %%"#opr1", %%"#opr2"; " \
                  "\tpsrlw     $8,        %%"#opr2"\n"
 

static unsigned long rgba8_alpha_mask[2] = { 0xFF000000, 0xFF000000 };
static unsigned long rgba8_b1[2] = { 0x01010101, 0x01010101 };
static unsigned long rgba8_b255[2] = { 0xFFFFFFFF, 0xFFFFFFFF };
static unsigned long rgba8_w1[2] = { 0x00010001, 0x00010001 };
static unsigned long rgba8_w128[2] = { 0x00800080, 0x00800080 };
static unsigned long rgba8_w256[2] = { 0x01000100, 0x01000100 };
static unsigned long rgba8_w255[2] = { 0X00FF00FF, 0X00FF00FF };

static unsigned long va8_alpha_mask[2] = { 0xFF00FF00, 0xFF00FF00 };
static unsigned long va8_b255[2] = { 0xFFFFFFFF, 0xFFFFFFFF };
static unsigned long va8_w1[2] = { 0x00010001, 0x00010001 };
static unsigned long va8_w255[2] = { 0X00FF00FF, 0X00FF00FF };
/*
 *
 */
void
gimp_composite_addition_rgba8_rgba8_rgba8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm volatile ("movq    %0,%%mm0"    
																: /* empty */
																: "m" (*rgba8_alpha_mask)
																: "%mm0");

  for (; op.n_pixels >= 2; op.n_pixels -= 2) {
    asm ("  movq    (%0), %%mm2; addl $8, %0\n"
									"\tmovq    (%1), %%mm3; addl $8, %1\n"
									"\tmovq    %%mm2, %%mm4\n"
									"\tpaddusb %%mm3, %%mm4\n"
									"\tmovq    %%mm0, %%mm1\n"
									"\tpandn   %%mm4, %%mm1\n"
									"\t" pminub(mm3, mm2, mm4) "\n"
									"\tpand    %%mm0, %%mm2\n"
									"\tpor     %%mm2, %%mm1\n"
									"\tmovq    %%mm1, (%2); addl $8, %2\n"
									: "+r" (op.A), "+r" (op.B), "+r" (op.D)
									: /* empty */
									: "0", "1", "2", "%mm0", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5", "%mm6", "%mm7");
  }

  if (op.n_pixels) {
    asm volatile ("  movd    (%0), %%mm2;\n"
        "\tmovd    (%1), %%mm3;\n"
        "\tmovq    %%mm2, %%mm4\n"
        "\tpaddusb %%mm3, %%mm4\n"
        "\tmovq    %%mm0, %%mm1\n"
        "\tpandn   %%mm4, %%mm1\n"
        "\t" pminub(mm3, mm2, mm4) "\n"
        "\tpand    %%mm0, %%mm2\n"
        "\tpor     %%mm2, %%mm1\n"
        "\tmovd    %%mm1, (%2);\n"
        : /* empty */
        : "r" (op.A), "r" (op.B), "r" (op.D)
        : "0", "1", "2", "%mm0", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5", "%mm6", "%mm7");
  }

  asm("emms");
}

void gimp_composite_burn_rgba8_rgba8_rgba8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm volatile ("movq   %0,%%mm1"
      : /* empty */
      : "m" (*rgba8_alpha_mask)
      : "%mm1");

  for (; op.n_pixels >= 2; op.n_pixels -= 2) {
    asm ("  movq      (%0),%%mm0; addl $8,%0\n"
									"\tmovq      (%1),%%mm1; addl $8,%1\n"
									
									"\tmovq      %3,%%mm2\n"
									"\tpsubb     %%mm0,%%mm2\n" /* mm2 = 255 - A */
									"\tpxor      %%mm4,%%mm4\n"
									"\tpunpcklbw %%mm2,%%mm4\n" /* mm4 = (255- A) * 256  */

									"\tmovq      %%mm1,%%mm3\n"
									"\tpxor      %%mm5,%%mm5\n"
									"\tpunpcklbw %%mm5,%%mm3\n"
									"\tmovq      %4,%%mm5\n"
									"\tpaddusw   %%mm3,%%mm5\n" /* mm5 = B + 1 */

									"\t" pdivwqX(mm4,mm5,mm7) "\n"

									"\tmovq      %3,%%mm2\n"
									"\tpsubb   %%mm0,%%mm2\n" /* mm2 = 255 - A */
									"\tpxor      %%mm4,%%mm4\n"
									"\tpunpckhbw %%mm2,%%mm4\n" /* mm4 = (255- A) * 256  */

									"\tmovq      %%mm1,%%mm3\n"
									"\tpxor      %%mm5,%%mm5\n"
									"\tpunpckhbw %%mm5,%%mm3\n"
									"\tmovq      %4,%%mm5\n"
									"\tpaddusw   %%mm3,%%mm5\n" /* mm5 = B + 1 */
									"\t" pdivwqX(mm4,mm5,mm6) "\n"

									"\tmovq      %5,%%mm4\n"
									"\tmovq      %%mm4,%%mm5\n"
									"\tpsubusw     %%mm6,%%mm4\n"
									"\tpsubusw     %%mm7,%%mm5\n"
                  
									"\tpackuswb  %%mm4,%%mm5\n"

									"\t" pminub(mm0,mm1,mm3) "\n" /* mm1 = min(mm0,mm1) clobber mm3 */

									"\tmovq      %6,%%mm7\n"
									"\tpand      %%mm7,%%mm1\n" /* mm1 = mm7 & alpha_mask */

									"\tpandn     %%mm5,%%mm7\n" /* mm7 = ~mm7 & mm5 */
									"\tpor       %%mm1,%%mm7\n" /* mm7 = mm7 | mm1 */

									"\tmovq      %%mm7,(%2); addl $8,%2\n"
									: "+r" (op.A), "+r" (op.B), "+r" (op.D)
									: "m" (*rgba8_b255), "m" (*rgba8_w1), "m" (*rgba8_w255), "m" (*rgba8_alpha_mask)
									: "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4");
  }

  if (op.n_pixels) {
    asm volatile ("  movd      (%0),%%mm0\n"
                  "\tmovd      (%1),%%mm1\n"

                  "\tmovq      %3,%%mm2\n"
                  "\tpsubb     %%mm0,%%mm2\n" /* mm2 = 255 - A */
                  "\tpxor      %%mm4,%%mm4\n"
                  "\tpunpcklbw %%mm2,%%mm4\n" /* mm4 = (255- A) * 256  */

                  "\tmovq      %%mm1,%%mm3\n"
                  "\tpxor      %%mm5,%%mm5\n"
                  "\tpunpcklbw %%mm5,%%mm3\n"
                  "\tmovq      %4,%%mm5\n"
                  "\tpaddusw   %%mm3,%%mm5\n" /* mm5 = B + 1 */

                  "\t" pdivwqX(mm4,mm5,mm7) "\n"

                  "\tmovq      %3,%%mm2\n"
                  "\tpsubb   %%mm0,%%mm2\n" /* mm2 = 255 - A */
                  "\tpxor      %%mm4,%%mm4\n"
                  "\tpunpckhbw %%mm2,%%mm4\n" /* mm4 = (255- A) * 256  */

                  "\tmovq      %%mm1,%%mm3\n"
                  "\tpxor      %%mm5,%%mm5\n"
                  "\tpunpckhbw %%mm5,%%mm3\n"
                  "\tmovq      %4,%%mm5\n"
                  "\tpaddusw   %%mm3,%%mm5\n" /* mm5 = B + 1 */
                  "\t" pdivwqX(mm4,mm5,mm6) "\n"

                  "\tmovq      %5,%%mm4\n"
                  "\tmovq      %%mm4,%%mm5\n"
                  "\tpsubusw     %%mm6,%%mm4\n"
                  "\tpsubusw     %%mm7,%%mm5\n"
                  
                  "\tpackuswb  %%mm4,%%mm5\n"

                  "\t" pminub(mm0,mm1,mm3) "\n" /* mm1 = min(mm0,mm1) clobber mm3 */

                  "\tmovq      %6,%%mm7\n"
                  "\tpand      %%mm7,%%mm1\n" /* mm1 = mm7 & alpha_mask */

                  "\tpandn     %%mm5,%%mm7\n" /* mm7 = ~mm7 & mm5 */
                  "\tpor       %%mm1,%%mm7\n" /* mm7 = mm7 | mm1 */

                  "\tmovd      %%mm7,(%2)\n"
                  : /* empty */
                  : "r" (op.A), "r" (op.B), "r" (op.D), "m" (*rgba8_b255), "m" (*rgba8_w1), "m" (*rgba8_w255), "m" (*rgba8_alpha_mask)
                  : "0", "1", "2", "%mm0", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5", "%mm6", "%mm7");
  }

  asm("emms");
}

void
xxxgimp_composite_coloronly_rgba8_rgba8_rgba8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("movq    %0,%%mm0"     :  : "m" (*rgba8_alpha_mask) : "%mm0");

  for (; op.n_pixels >= 2; op.n_pixels -= 2) {
    asm volatile ("  movq    (%0), %%mm2; addl  $8, %0\n"
                  "\tmovq    (%1), %%mm3; addl  $8, %1\n"


                  "\tmovq    %%mm1, (%2); addl  $8, %2\n"
                  : "+r" (op.A), "+S" (op.B), "+D" (op.D)
                  : /* empty */
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4");
  }

  if (op.n_pixels) {
    asm volatile ("  movd    (%0), %%mm2;\n"
                  "\tmovd    (%1), %%mm3;\n"

                  "\tmovd    %%mm1, (%2);\n"
                  : /* empty */
                  : "r" (op.A), "r" (op.B), "r" (op.D)
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4");
  }

  asm("emms");

}

void
gimp_composite_darken_rgba8_rgba8_rgba8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  for (; op.n_pixels >= 2; op.n_pixels -= 2) {
    asm ("  movq    (%0), %%mm2; addl  $8, %0\n"
									"\tmovq    (%1), %%mm3; addl  $8, %1\n"
									"\t" pminub(mm3, mm2, mm4) "\n"
									"\tmovq    %%mm2, (%2); addl  $8, %2\n"
									: "+r" (op.A), "+S" (op.B), "+D" (op.D)
									: /* empty */
									: "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4");
  }

  if (op.n_pixels) {
    asm volatile ("  movd    (%0), %%mm2;\n"
                  "\tmovd    (%1), %%mm3;\n"
                  "\t" pminub(mm3, mm2, mm4) "\n"
                  "\tmovd    %%mm2, (%2);\n"
                  : /* empty */
                  : "r" (op.A), "r" (op.B), "r" (op.D)
                  : "0", "1", "2", "%mm2", "%mm3", "%mm4");
  }
        
  asm("emms");
}

void
gimp_composite_difference_rgba8_rgba8_rgba8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("movq    %0,%%mm0"     :  : "m" (*rgba8_alpha_mask) : "%mm0");

  for (; op.n_pixels >= 2; op.n_pixels -= 2) {
    asm ("  movq    (%0), %%mm2; addl $8, %0\n"
									"\tmovq    (%1), %%mm3; addl $8, %1\n"
									"\tmovq    %%mm2, %%mm4\n"
									"\tmovq    %%mm3, %%mm5\n"
									"\tpsubusb %%mm3, %%mm4\n"
									"\tpsubusb %%mm2, %%mm5\n"
									"\tpaddb   %%mm5, %%mm4\n"
									"\tmovq    %%mm0, %%mm1\n"
									"\tpandn   %%mm4, %%mm1\n"
									"\tpminub  %%mm3, %%mm2\n"
									"\tpand    %%mm0, %%mm2\n"
									"\tpor     %%mm2, %%mm1\n"
									"\tmovq    %%mm1, (%2); addl $8, %2\n"
									: "+r" (op.A), "+r" (op.B), "+r" (op.D)
									: /* empty */
									: "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4");
  }
  
  if (op.n_pixels) {
    asm volatile ("  movd    (%0), %%mm2;\n"
                  "\tmovd    (%1), %%mm3;\n"
                  "\tmovq    %%mm2, %%mm4\n"
                  "\tmovq    %%mm3, %%mm5\n"
                  "\tpsubusb %%mm3, %%mm4\n"
                  "\tpsubusb %%mm2, %%mm5\n"
                  "\tpaddb   %%mm5, %%mm4\n"
                  "\tmovq    %%mm0, %%mm1\n"
                  "\tpandn   %%mm4, %%mm1\n"
                  "\tpminub  %%mm3, %%mm2\n"
                  "\tpand    %%mm0, %%mm2\n"
                  "\tpor     %%mm2, %%mm1\n"
                  "\tmovd    %%mm1, (%2);\n"
                  : /* empty */
                  : "r" (op.A), "r" (op.B), "r" (op.D)
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4");
  }

  asm("emms");
}


void
xxxgimp_composite_dissolve_rgba8_rgba8_rgba8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("movq    %0,%%mm0"     :  : "m" (*rgba8_alpha_mask) : "%mm0");

  for (; op.n_pixels >= 2; op.n_pixels -= 2) {
    asm volatile ("\tmovq    (%0), %%mm2; addl $8, %0\n"
                  "\tmovq    (%1), %%mm3; addl $8, %1\n"

                  "\tmovq      %%mm1, (%2); addl $8, %2\n"
                  : "+r" (op.A), "+r" (op.B), "+r" (op.D)
                  : /* empty */
                  : "%eax", "%ecx", "%edx", "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5");
  }

  if (op.n_pixels) {
    asm volatile ("\tmovd    (%0), %%mm2;\n"
                  "\tmovd    (%1), %%mm3;\n"

                  "\tmovd      %%mm1, (%2);\n"
                  : /* empty */
                  : "r" (op.A), "r" (op.B), "r" (op.D)
                  : "%eax", "%ecx", "%edx", "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5");
  }

  asm("emms");
}

void
gimp_composite_divide_rgba8_rgba8_rgba8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("movq    %0, %%mm0\n"
      "\tmovq    %1, %%mm7\n"
      :
      : "m" (*rgba8_alpha_mask), "m" (*rgba8_w1)
      : "%mm0", "%mm7");
  
  for (; op.n_pixels >= 2; op.n_pixels -= 2) {
    asm volatile ("  movq    (%0), %%mm0; addl $8, %0\n"
                  "\tmovq    (%1), %%mm1; addl $8, %1\n"

                  "\tpxor      %%mm2,%%mm2\n"
                  "\tpunpcklbw %%mm0,%%mm2\n" /* mm2 = A*256 */

                  "\tmovq      %%mm1,%%mm3\n"
                  "\tpxor      %%mm5,%%mm5\n"
                  "\tpunpcklbw %%mm5,%%mm3\n"
                  "\tpaddw     %%mm7,%%mm3\n" /* mm3 = B+1 */

                  "\t" pdivwuqX(mm2,mm3,mm5) "\n" /* mm5 = (A*256)/(B+1) */

                  "\tpxor      %%mm2,%%mm2\n"
                  "\tpunpckhbw %%mm0,%%mm2\n" /* mm2 = A*256 */

                  "\tmovq      %%mm1,%%mm3\n"
                  "\tpxor      %%mm6,%%mm6\n"
                  "\tpunpckhbw %%mm6,%%mm3\n"
                  "\tpaddw     %%mm7,%%mm3\n" /* mm3 = B+1 */

                  "\t" pdivwuqX(mm2,mm3,mm4) "\n" /* mm4 = (A*256)/(B+1) */
                  
                  "\tpackuswb  %%mm4,%%mm5\n" /* expects mm4 and mm5 to be signed values */

                  "\t" pminub(mm0,mm1,mm3) "\n"
                  "\tmovq      %3,%%mm3\n"
                  "\tmovq      %%mm3,%%mm2\n"

                  "\tpandn     %%mm5,%%mm3\n"

                  "\tpand      %%mm2,%%mm1\n"
                  "\tpor       %%mm1,%%mm3\n"

                  "\tmovq      %%mm3,(%2); addl $8, %2\n"
                  : "+r" (op.A), "+r" (op.B), "+r" (op.D)
                  : "m" (*rgba8_alpha_mask)
                  : "%eax", "%ecx", "%edx", "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5");
  }

  if (op.n_pixels) {
    asm volatile ("  movd    (%0), %%mm0; addl $8, %0\n"
                  "\tmovd    (%1), %%mm1; addl $8, %1\n"

                  "\tpxor      %%mm2,%%mm2\n"
                  "\tpunpcklbw %%mm0,%%mm2\n" /* mm2 = A*256 */

                  "\tmovq      %%mm1,%%mm3\n"
                  "\tpxor      %%mm5,%%mm5\n"
                  "\tpunpcklbw %%mm5,%%mm3\n"
                  "\tpaddw     %%mm7,%%mm3\n" /* mm3 = B+1 */

                  "\t" pdivwuqX(mm2,mm3,mm5) "\n" /* mm5 = (A*256)/(B+1) */

                  "\tpxor      %%mm2,%%mm2\n"
                  "\tpunpckhbw %%mm0,%%mm2\n" /* mm2 = A*256 */

                  "\tmovq      %%mm1,%%mm3\n"
                  "\tpxor      %%mm6,%%mm6\n"
                  "\tpunpckhbw %%mm6,%%mm3\n"
                  "\tpaddw     %%mm7,%%mm3\n" /* mm3 = B+1 */

                  "\t" pdivwuqX(mm2,mm3,mm4) "\n" /* mm4 = (A*256)/(B+1) */
                  
                  "\tpackuswb  %%mm4,%%mm5\n" /* expects mm4 and mm5 to be signed values */

                  "\t" pminub(mm0,mm1,mm3) "\n"
                  "\tmovq      %3,%%mm3\n"
                  "\tmovq      %%mm3,%%mm2\n"

                  "\tpandn     %%mm5,%%mm3\n"

                  "\tpand      %%mm2,%%mm1\n"
                  "\tpor       %%mm1,%%mm3\n"

                  "\tmovd      %%mm3,(%2); addl $8, %2\n"
                  : /* empty */
                  : "r" (op.A), "r" (op.B), "r" (op.D), "m" (*rgba8_alpha_mask)
                  : "%eax", "%ecx", "%edx", "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5");
  }

  asm("emms");
}

/*
 * (src1[b] << 8) / (256 - src2[b]);
 */
void
gimp_composite_dodge_rgba8_rgba8_rgba8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  for (; op.n_pixels >= 2; op.n_pixels -= 2) {
    asm volatile ("  movq     (%0), %%mm0; addl $8, %0\n"
                  "\tmovq     (%1), %%mm1; addl $8, %1\n"
                  "\tmovq      %%mm1, %%mm3\n"
                  "\tpxor      %%mm2, %%mm2\n"
                  "\tpunpcklbw %%mm2, %%mm3\n"
                  "\tpunpcklbw %%mm0, %%mm2\n"

                  "\tmovq      rgba8_w256, %%mm4\n"
                  "\tpsubw     %%mm3, %%mm4\n"

                  "\t" pdivwuqX(mm2,mm4,mm5) "\n"

                  "\tmovq      %%mm1, %%mm3\n"
                  "\tpxor      %%mm2, %%mm2\n"
                  "\tpunpckhbw %%mm2, %%mm3\n"
                  "\tpunpckhbw %%mm0, %%mm2\n"

                  "\tmovq      rgba8_w256, %%mm4\n"
                  "\tpsubw     %%mm3, %%mm4\n"

                  "\t" pdivwuqX(mm2,mm4,mm6) "\n"

                  "\tpackuswb  %%mm6, %%mm5\n"

                  "\tmovq      rgba8_alpha_mask, %%mm6\n"
                  "\tmovq      %%mm1,%%mm7\n"
                  "\t" pminub(mm0,mm7,mm2) "\n"
                  "\tpand      %%mm6, %%mm7\n"
                  "\tpandn     %%mm5, %%mm6\n"

                  "\tpor       %%mm6, %%mm7\n"

                  "\tmovq    %%mm7, (%2); addl $8, %2\n"
                  : "+r" (op.A), "+r" (op.B), "+r" (op.D)
                  : /* empty */
                  : "0", "1", "2", "%mm0", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5", "%mm6", "%mm7");
  }

  if (op.n_pixels) {
    asm volatile ("  movd     (%0), %%mm0;\n"
                  "\tmovq     (%1), %%mm1;\n"
                  "\tmovq      %%mm1, %%mm3\n"
                  "\tpxor      %%mm2, %%mm2\n"
                  "\tpunpcklbw %%mm2, %%mm3\n"
                  "\tpunpcklbw %%mm0, %%mm2\n"

                  "\tmovq      rgba8_w256, %%mm4\n"
                  "\tpsubw     %%mm3, %%mm4\n"

                  "\t" pdivwuqX(mm2,mm4,mm5) "\n"

                  "\tmovq      %%mm1, %%mm3\n"
                  "\tpxor      %%mm2, %%mm2\n"
                  "\tpunpckhbw %%mm2, %%mm3\n"
                  "\tpunpckhbw %%mm0, %%mm2\n"

                  "\tmovq      rgba8_w256, %%mm4\n"
                  "\tpsubw     %%mm3, %%mm4\n"

                  "\t" pdivwuqX(mm2,mm4,mm6) "\n"

                  "\tpackuswb  %%mm6, %%mm5\n"

                  "\tmovq      rgba8_alpha_mask, %%mm6\n"
                  "\tmovq      %%mm1,%%mm7\n"
                  "\t" pminub(mm0,mm7,mm2) "\n"
                  "\tpand      %%mm6, %%mm7\n"
                  "\tpandn     %%mm5, %%mm6\n"

                  "\tpor       %%mm6, %%mm7\n"

                  "\tmovd    %%mm7, (%2);\n"
                  : /* empty */
                  : "r" (op.A), "r" (op.B), "r" (op.D)
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5");
  }

  asm("emms");
}

void
gimp_composite_grain_extract_rgba8_rgba8_rgba8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("movq    %0,%%mm0"     :  : "m" (*rgba8_alpha_mask) : "%mm0");
  asm("pxor    %%mm6,%%mm6"  :  :                        : "%mm6");
  asm("movq    %0,%%mm7"     :  : "m" (*rgba8_w128)       : "%mm7");

  for (; op.n_pixels >= 2; op.n_pixels -= 2) {
    asm volatile ("  movq    (%0), %%mm2; addl $8, %0\n"
                  "\tmovq    (%1), %%mm3; addl $8, %1\n"

                  "\tmovq      %%mm2, %%mm4\n"
                  "\tpunpcklbw %%mm6, %%mm4\n"
                  "\tmovq      %%mm3, %%mm5\n"
                  "\tpunpcklbw %%mm6, %%mm5\n"

                  "\tpsubw     %%mm5, %%mm4\n"
                  "\tpaddw     %%mm7, %%mm4\n"
                  "\tmovq      %%mm4, %%mm1\n"

                  "\tmovq      %%mm2, %%mm4\n"
                  "\tpunpckhbw %%mm6, %%mm4\n"
                  "\tmovq      %%mm3, %%mm5\n"
                  "\tpunpckhbw %%mm6, %%mm5\n"

                  "\tpsubw     %%mm5, %%mm4\n"
                  "\tpaddw     %%mm7, %%mm4\n"

                  "\tpackuswb  %%mm4, %%mm1\n"
                  "\tmovq      %%mm1, %%mm4\n"

                  "\tmovq      %%mm0, %%mm1; pandn     %%mm4, %%mm1\n"

                  "\t" pminub(mm3,mm2,mm4) "\n"
                  "\tpand      %%mm0, %%mm2\n"

                  "\tpor       %%mm2, %%mm1\n"
                  "\tmovq      %%mm1, (%2); addl $8, %2\n"
                  : "+r" (op.A), "+r" (op.B), "+r" (op.D)
                  : /* empty */
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4");
  }

  if (op.n_pixels) {
    asm volatile ("  movd    (%0), %%mm2;\n"
                  "\tmovd    (%1), %%mm3;\n"

                  "\tmovq      %%mm2, %%mm4\n"
                  "\tpunpcklbw %%mm6, %%mm4\n"
                  "\tmovq      %%mm3, %%mm5\n"
                  "\tpunpcklbw %%mm6, %%mm5\n"

                  "\tpsubw     %%mm5, %%mm4\n"
                  "\tpaddw     %%mm7, %%mm4\n"
                  "\tmovq      %%mm4, %%mm1\n"

                  "\tmovq      %%mm2, %%mm4\n"
                  "\tpunpckhbw %%mm6, %%mm4\n"
                  "\tmovq      %%mm3, %%mm5\n"
                  "\tpunpckhbw %%mm6, %%mm5\n"

                  "\tpsubw     %%mm5, %%mm4\n"
                  "\tpaddw     %%mm7, %%mm4\n"

                  "\tpackuswb  %%mm4, %%mm1\n"
                  "\tmovq      %%mm1, %%mm4\n"

                  "\tmovq      %%mm0, %%mm1; pandn     %%mm4, %%mm1\n"

                  "\t" pminub(mm3,mm2,mm4) "\n"
                  "\tpand      %%mm0, %%mm2\n"

                  "\tpor       %%mm2, %%mm1\n"
                  "\tmovd      %%mm1, (%2);\n"
                  : /* empty */
                  : "r" (op.A), "r" (op.B), "r" (op.D)
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4");
  }

  asm("emms");

}

void
gimp_composite_grain_merge_rgba8_rgba8_rgba8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("movq    %0, %%mm0"     :  : "m" (*rgba8_alpha_mask) : "%mm0");
  asm("pxor    %%mm6, %%mm6"  :  :                        : "%mm6");
  asm("movq    %0, %%mm7"     :  : "m" (*rgba8_w128)       : "%mm7");

  for (; op.n_pixels >= 2; op.n_pixels -= 2) {
    asm volatile ("  movq    (%0), %%mm2; addl $8, %0\n"
                  "\tmovq    (%1), %%mm3; addl $8, %1\n"

                  "\tmovq      %%mm2, %%mm4\n"
                  "\tpunpcklbw %%mm6, %%mm4\n"
                  "\tmovq      %%mm3, %%mm5\n"
                  "\tpunpcklbw %%mm6, %%mm5\n"

                  "\tpaddw     %%mm5, %%mm4\n"
                  "\tpsubw     %%mm7, %%mm4\n"
                  "\tmovq      %%mm4, %%mm1\n"

                  "\tmovq      %%mm2, %%mm4\n"
                  "\tpunpckhbw %%mm6, %%mm4\n"
                  "\tmovq      %%mm3, %%mm5\n"
                  "\tpunpckhbw %%mm6, %%mm5\n"

                  "\tpaddw     %%mm5, %%mm4\n"
                  "\tpsubw     %%mm7, %%mm4\n"

                  "\tpackuswb  %%mm4, %%mm1\n"
                  "\tmovq      %%mm1, %%mm4\n"

                  "\tmovq      %%mm0, %%mm1; pandn     %%mm4, %%mm1\n"

                  "\t" pminub(mm3,mm2,mm4) "\n"
                  "\tpand      %%mm0, %%mm2\n"

                  "\tpor       %%mm2, %%mm1\n"
                  "\tmovq      %%mm1, (%2); addl $8, %2\n"
                  : "+r" (op.A), "+r" (op.B), "+r" (op.D)
                  : /* empty */
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4");
  }

  if (op.n_pixels) {
    asm volatile ("  movd    (%0), %%mm2;\n"
                  "\tmovd    (%1), %%mm3;\n"

                  "\tmovq      %%mm2, %%mm4\n"
                  "\tpunpcklbw %%mm6, %%mm4\n"
                  "\tmovq      %%mm3, %%mm5\n"
                  "\tpunpcklbw %%mm6, %%mm5\n"

                  "\tpaddw     %%mm5, %%mm4\n"
                  "\tpsubw     %%mm7, %%mm4\n"
                  "\tmovq      %%mm4, %%mm1\n"

                  "\tmovq      %%mm2, %%mm4\n"
                  "\tpunpckhbw %%mm6, %%mm4\n"
                  "\tmovq      %%mm3, %%mm5\n"
                  "\tpunpckhbw %%mm6, %%mm5\n"

                  "\tpaddw     %%mm5, %%mm4\n"
                  "\tpsubw     %%mm7, %%mm4\n"

                  "\tpackuswb  %%mm4, %%mm1\n"
                  "\tmovq      %%mm1, %%mm4\n"

                  "\tmovq      %%mm0, %%mm1; pandn     %%mm4, %%mm1\n"

                  "\t" pminub(mm3,mm2,mm4) "\n"
                  "\tpand      %%mm0, %%mm2\n"

                  "\tpor       %%mm2, %%mm1\n"
                  "\tmovd      %%mm1, (%2);\n"
                  : /* empty */
                  : "r" (op.A), "r" (op.B), "r" (op.D)
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4");
  }

  asm("emms");

}

void
xxxgimp_composite_hardlight_rgba8_rgba8_rgba8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("movq    %0,%%mm0"     :  : "m" (*rgba8_alpha_mask) : "%mm0");

  for (; op.n_pixels >= 2; op.n_pixels -= 2) {

  }

  if (op.n_pixels) {

  }

  asm("emms");

}

void
xxxgimp_composite_hueonly_rgba8_rgba8_rgba8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("movq    %0,%%mm0"     :  : "m" (*rgba8_alpha_mask) : "%mm0");

  for (; op.n_pixels >= 2; op.n_pixels -= 2) {

  }

  if (op.n_pixels) {

  }

  asm("emms");
}

void
gimp_composite_lighten_rgba8_rgba8_rgba8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("movq    %0,%%mm0"     :  : "m" (*rgba8_alpha_mask) : "%mm0");

  for (; op.n_pixels >= 2; op.n_pixels -= 2) {
    asm volatile ("  movq     (%0), %%mm2; addl $8, %0\n"
                  "\tmovq     (%1), %%mm3; addl $8, %1\n"
                  "\tmovq    %%mm2, %%mm4\n"
                  "\t" pmaxub(mm3,mm4,mm5) "\n"
                  "\tmovq    %%mm0, %%mm1\n"
                  "\tpandn   %%mm4, %%mm1\n"
                  "\t" pminub(mm2,mm3,mm4) "\n"
                  "\tpand    %%mm0, %%mm3\n"
                  "\tpor     %%mm3, %%mm1\n"
                  "\tmovq    %%mm1, (%2); addl $8, %2\n"
                  : "+r" (op.A), "+r" (op.B), "+r" (op.D)
                  : /* empty */
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5");
  }

  if (op.n_pixels) {
    asm volatile ("  movd    (%0), %%mm2\n"
                  "\tmovd    (%1), %%mm3\n"
                  "\tmovq    %%mm2, %%mm4\n"
                  "\t" pmaxub(mm3,mm4,mm5) "\n"

                  "\tmovq    %%mm0, %%mm1\n"
                  "\tpandn   %%mm4, %%mm1\n"

                  "\t" pminub(mm2,mm3,mm4) "\n"

                  "\tpand    %%mm0, %%mm3\n"
                  "\tpor     %%mm3, %%mm1\n"
                  "\tmovd    %%mm1, (%2)\n"
                  : /* empty */
                  : "r" (op.A), "r" (op.B), "r" (op.D)
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5");
  }

  asm("emms");
}

void
gimp_composite_multiply_rgba8_rgba8_rgba8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("movq    %0,%%mm0"     :  : "m" (*rgba8_alpha_mask) : "%mm0");
  asm("movq    %0,%%mm7"     :  : "m" (*rgba8_w128) : "%mm7");
  asm("pxor    %%mm6,%%mm6"  :  :  : "%mm6");

  for (; op.n_pixels >= 2; op.n_pixels -= 2) {
    asm volatile ("  movq     (%0), %%mm2; addl $8, %0\n"
                  "\tmovq     (%1), %%mm3; addl $8, %1\n"

                  "\tmovq      %%mm2, %%mm1\n"
                  "\tpunpcklbw %%mm6, %%mm1\n"
                  "\tmovq      %%mm3, %%mm5\n"
                  "\tpunpcklbw %%mm6, %%mm5\n"
                  
                  "\t" pmulwX(mm5,mm1,mm7) "\n"

                  "\tmovq      %%mm2, %%mm4\n"
                  "\tpunpckhbw %%mm6, %%mm4\n"
                  "\tmovq      %%mm3, %%mm5\n"
                  "\tpunpckhbw %%mm6, %%mm5\n"

                  "\t" pmulwX(mm5,mm4,mm7) "\n"

                  "\tpackuswb  %%mm4, %%mm1\n"

                  "\tmovq      %%mm0, %%mm4\n"
                  "\tpandn     %%mm1, %%mm4\n"
                  "\tmovq      %%mm4, %%mm1\n"
                  "\t" pminub(mm3,mm2,mm4) "\n"
                  "\tpand      %%mm0, %%mm2\n"
                  "\tpor       %%mm2, %%mm1\n"
                  
                  "\tmovq    %%mm1, (%2); addl $8, %2\n"
                  : "+r" (op.A), "+r" (op.B), "+r" (op.D)
                  : /* empty */
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5");
  }

  if (op.n_pixels) {
    asm volatile ("  movd     (%0), %%mm2\n"
                  "\tmovd     (%1), %%mm3\n"

                  "\tmovq      %%mm2, %%mm1\n"
                  "\tpunpcklbw %%mm6, %%mm1\n"
                  "\tmovq      %%mm3, %%mm5\n"
                  "\tpunpcklbw %%mm6, %%mm5\n"

                  "\t" pmulwX(mm5,mm1,mm7) "\n"

                  "\tmovq      %%mm2, %%mm4\n"
                  "\tpunpckhbw %%mm6, %%mm4\n"
                  "\tmovq      %%mm3, %%mm5\n"
                  "\tpunpckhbw %%mm6, %%mm5\n"

                  "\t" pmulwX(mm5,mm4,mm7) "\n"

                  "\tpackuswb  %%mm4, %%mm1\n"

                  "\tmovq      %%mm0, %%mm4\n"
                  "\tpandn     %%mm1, %%mm4\n"
                  "\tmovq      %%mm4, %%mm1\n"
                  "\t" pminub(mm3,mm2,mm4) "\n"
                  "\tpand      %%mm0, %%mm2\n"
                  "\tpor       %%mm2, %%mm1\n"
                  
                  "\tmovd    %%mm1, (%2)\n"
                  : /* empty */
                  : "r" (op.A), "r" (op.B), "r" (op.D)
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5");
  }

  asm("emms");
}

const static unsigned long rgba8_lower_ff[2] = {  0x00FF00FF, 0x00FF00FF };

static void
op_overlay(void)
{
  asm("movq      %mm2, %mm1");
  asm("punpcklbw %mm6, %mm1");
  asm("movq      %mm3, %mm5");
  asm("punpcklbw %mm6, %mm5");
  asm("pmullw    %mm5, %mm1");
  asm("paddw     %mm7, %mm1");
  asm("movq      %mm1, %mm5");
  asm("psrlw     $8, %mm5");
  asm("paddw     %mm5, %mm1");
  asm("psrlw     $8, %mm1");

  asm("pcmpeqb   %mm4, %mm4");
  asm("psubb     %mm2, %mm4");
  asm("punpcklbw %mm6, %mm4");
  asm("pcmpeqb   %mm5, %mm5");
  asm("psubb     %mm3, %mm5");
  asm("punpcklbw %mm6, %mm5");
  asm("pmullw    %mm5, %mm4");
  asm("paddw     %mm7, %mm4");
  asm("movq      %mm4, %mm5");
  asm("psrlw     $8, %mm5");
  asm("paddw     %mm5, %mm4");
  asm("psrlw     $8, %mm4");

  asm("movq      rgba8_lower_ff, %mm5");
  asm("psubw     %mm4, %mm5");

  asm("psubw     %mm1, %mm5");
  asm("movq      %mm2, %mm4");
  asm("punpcklbw %mm6, %mm4");
  asm("pmullw    %mm4, %mm5");
  asm("paddw     %mm7, %mm5");
  asm("movq      %mm5, %mm4");
  asm("psrlw     $8, %mm4");
  asm("paddw     %mm4, %mm5");
  asm("psrlw     $8, %mm5");
  asm("paddw     %mm1, %mm5");

  asm("subl      $8, %esp");
  asm("movq      %mm5, (%esp)");

  asm("movq      %mm2, %mm1");
  asm("punpckhbw %mm6, %mm1");
  asm("movq      %mm3, %mm5");
  asm("punpckhbw %mm6, %mm5");
  asm("pmullw    %mm5, %mm1");
  asm("paddw     %mm7, %mm1");
  asm("movq      %mm1, %mm5");
  asm("psrlw     $8, %mm5");
  asm("paddw     %mm5, %mm1");
  asm("psrlw     $8, %mm1");

  asm("pcmpeqb   %mm4, %mm4");
  asm("psubb     %mm2, %mm4");
  asm("punpckhbw %mm6, %mm4");
  asm("pcmpeqb   %mm5, %mm5");
  asm("psubb     %mm3, %mm5");
  asm("punpckhbw %mm6, %mm5");
  asm("pmullw    %mm5, %mm4");
  asm("paddw     %mm7, %mm4");
  asm("movq      %mm4, %mm5");
  asm("psrlw     $8, %mm5");
  asm("paddw     %mm5, %mm4");
  asm("psrlw     $8, %mm4");

  asm("movq      rgba8_lower_ff, %mm5");
  asm("psubw     %mm4, %mm5");

  asm("psubw     %mm1, %mm5");
  asm("movq      %mm2, %mm4");
  asm("punpckhbw %mm6, %mm4");
  asm("pmullw    %mm4, %mm5");
  asm("paddw     %mm7, %mm5");
  asm("movq      %mm5, %mm4");
  asm("psrlw     $8, %mm4");
  asm("paddw     %mm4, %mm5");
  asm("psrlw     $8, %mm5");
  asm("paddw     %mm1, %mm5");

  asm("movq      (%esp), %mm4");
  asm("addl      $8, %esp");

  asm("packuswb  %mm5, %mm4");
  asm("movq      %mm0, %mm1");
  asm("pandn     %mm4, %mm1");

  asm("movq      %mm2, %mm4");
  asm("psubusb   %mm3, %mm4");
  asm("psubb     %mm4, %mm2");
  asm("pand      %mm0, %mm2");
  asm("por       %mm2, %mm1");
}

void
gimp_composite_overlay_rgba8_rgba8_rgba8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("movq    %0,%%mm0"     :  : "m" (*rgba8_alpha_mask) : "%mm0");

  for (; op.n_pixels >= 2; op.n_pixels -= 2) {
    asm volatile ("  movq    (%0), %%mm2; addl  $8, %0\n"
                  "\tmovq    (%1), %%mm3; addl  $8, %1\n"

                  "\tcall op_overlay\n"

                  "\tmovq    %%mm1, (%2); addl  $8, %2\n"
                  : "+r" (op.A), "+S" (op.B), "+D" (op.D)
                  : /* empty */
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4");
  }

  if (op.n_pixels) {
    asm volatile ("  movd    (%0), %%mm2;\n"
                  "\tmovd    (%1), %%mm3;\n"

                  "\tcall op_overlay\n"

                  "\tmovd    %%mm1, (%2);\n"
                  : /* empty */
                  : "r" (op.A), "r" (op.B), "r" (op.D)
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4");
  }

  asm("emms");
}

void
xxxgimp_composite_saturationonly_rgba8_rgba8_rgba8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("movq    %0,%%mm0"     :  : "m" (*rgba8_alpha_mask) : "%mm0");

  for (; op.n_pixels >= 2; op.n_pixels -= 2) {
    asm volatile ("  movq    (%0), %%mm2; addl  $8, %0\n"
                  "\tmovq    (%1), %%mm3; addl  $8, %1\n"


                  "\tmovq    %%mm1, (%2); addl  $8, %2\n"
                  : "+r" (op.A), "+S" (op.B), "+D" (op.D)
                  : /* empty */
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4");
  }

  if (op.n_pixels) {
    asm volatile ("  movd    (%0), %%mm2;\n"
                  "\tmovd    (%1), %%mm3;\n"

                  "\tmovd    %%mm1, (%2);\n"
                  : /* empty */
                  : "r" (op.A), "r" (op.B), "r" (op.D)
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4");
  }

  asm("emms");
}

void
gimp_composite_scale_rgba8_rgba8_rgba8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm volatile ("pxor    %%mm0,%%mm0\n"
                "\tmovl  %0,%%eax\n"
                "\tmovl  %%eax,%%ebx\n"
                "\tshl   $16,%%ebx\n"
                "\torl   %%ebx,%%eax\n"
                "\tmovd  %%eax,%%mm5\n"
                "\tmovd  %%eax,%%mm3\n"
                "\tpsllq $32,%%mm5\n"
                "\tpor   %%mm5,%%mm3\n"
                "\tmovq  %1,%%mm7\n"
                : /* empty */
                : "m" (op.scale.scale), "m" (*rgba8_w128)
                : "%eax", "%mm0", "%mm5", "%mm6", "%mm7");

  for (; op.n_pixels >= 2; op.n_pixels -= 2) {
    asm volatile ("movq      (%0),%%mm2; addl $8,%0\n"
                  "\tmovq      %%mm2,%%mm1\n"
                  "\tpunpcklbw %%mm0,%%mm1\n"
                  "\tmovq      %%mm3,%%mm5\n"

                  "\t" pmulwX(mm5,mm1,mm7) "\n"

                  "\tmovq      %%mm2,%%mm4\n"
                  "\tpunpckhbw %%mm0,%%mm4\n"
                  "\tmovq      %%mm3,%%mm5\n"

                  "\t" pmulwX(mm5,mm4,mm7) "\n"

                  "\tpackuswb  %%mm4,%%mm1\n"

                  "\tmovq    %%mm1,(%1);  addl $8,%1\n"
                  : "+r" (op.A), "+r" (op.D)
                  : /* empty */
                  : "0", "1", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5", "%mm6", "%mm7");

  }

  if (op.n_pixels) {
    asm volatile ("movd      (%0), %%mm2\n"
                  "\tmovq      %%mm2,%%mm1\n"
                  "\tpunpcklbw %%mm0,%%mm1\n"
                  "\tmovq      %%mm3,%%mm5\n"

                  "\t" pmulwX(mm5,mm1,mm7) "\n"

                  "\tpackuswb  %%mm0,%%mm1\n"
                  "\tmovd    %%mm1,(%1)\n"
                  : /* empty */
                  : "r" (op.A), "r" (op.D)
                  : "0", "1", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5", "%mm6", "%mm7");
  }

  asm("emms");
}

void
gimp_composite_screen_rgba8_rgba8_rgba8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("movq    %0,%%mm0"     :  : "m" (*rgba8_alpha_mask) : "%mm0");
  asm("movq    %0,%%mm7"     :  : "m" (*rgba8_w128)  : "%mm7");
  asm("pxor    %mm6, %mm6");

  for (; op.n_pixels >= 2; op.n_pixels -= 2) {
    asm volatile ("  movq     (%0), %%mm2; addl $8, %0\n"
                  "\tmovq     (%1), %%mm3; addl $8, %1\n"

                  "\tpcmpeqb   %%mm4, %%mm4\n"
                  "\tpsubb     %%mm2, %%mm4\n"
                  "\tpcmpeqb   %%mm5, %%mm5\n"
                  "\tpsubb     %%mm3, %%mm5\n"

                  "\tpunpcklbw %%mm6, %%mm4\n"
                  "\tpunpcklbw %%mm6, %%mm5\n"
                  "\tpmullw    %%mm4, %%mm5\n"
                  "\tpaddw     %%mm7, %%mm5\n"
                  "\tmovq      %%mm5, %%mm1\n"
                  "\tpsrlw     $ 8, %%mm1\n"
                  "\tpaddw     %%mm5, %%mm1\n"
                  "\tpsrlw     $ 8, %%mm1\n"

                  "\tpcmpeqb   %%mm4, %%mm4\n"
                  "\tpsubb     %%mm2, %%mm4\n"
                  "\tpcmpeqb   %%mm5, %%mm5\n"
                  "\tpsubb     %%mm3, %%mm5\n"

                  "\tpunpckhbw %%mm6, %%mm4\n"
                  "\tpunpckhbw %%mm6, %%mm5\n"
                  "\tpmullw    %%mm4, %%mm5\n"
                  "\tpaddw     %%mm7, %%mm5\n"
                  "\tmovq      %%mm5, %%mm4\n"
                  "\tpsrlw     $ 8, %%mm4\n"
                  "\tpaddw     %%mm5, %%mm4\n"
                  "\tpsrlw     $ 8, %%mm4\n"

                  "\tpackuswb  %%mm4, %%mm1\n"

                  "\tpcmpeqb   %%mm4, %%mm4\n"
                  "\tpsubb     %%mm1, %%mm4\n"

                  "\tmovq      %%mm0, %%mm1\n"
                  "\tpandn     %%mm4, %%mm1\n"

                  "\t" pminub(mm2,mm3,mm4) "\n"
                  "\tpand      %%mm0, %%mm3\n"

                  "\tpor       %%mm3, %%mm1\n"

                  "\tmovq    %%mm1, (%2); addl $8, %2\n"
                  : "+r" (op.A), "+r" (op.B), "+r" (op.D)
                  : /* empty */
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5");
  }

  if (op.n_pixels) {
    asm volatile ("  movd     (%0), %%mm2;\n"
                  "\tmovd     (%1), %%mm3;\n"

                  "\tpcmpeqb   %%mm4, %%mm4\n"
                  "\tpsubb     %%mm2, %%mm4\n"
                  "\tpcmpeqb   %%mm5, %%mm5\n"
                  "\tpsubb     %%mm3, %%mm5\n"

                  "\tpunpcklbw %%mm6, %%mm4\n"
                  "\tpunpcklbw %%mm6, %%mm5\n"
                  "\tpmullw    %%mm4, %%mm5\n"
                  "\tpaddw     %%mm7, %%mm5\n"
                  "\tmovq      %%mm5, %%mm1\n"
                  "\tpsrlw     $ 8, %%mm1\n"
                  "\tpaddw     %%mm5, %%mm1\n"
                  "\tpsrlw     $ 8, %%mm1\n"

                  "\tpcmpeqb   %%mm4, %%mm4\n"
                  "\tpsubb     %%mm2, %%mm4\n"
                  "\tpcmpeqb   %%mm5, %%mm5\n"
                  "\tpsubb     %%mm3, %%mm5\n"

                  "\tpunpckhbw %%mm6, %%mm4\n"
                  "\tpunpckhbw %%mm6, %%mm5\n"
                  "\tpmullw    %%mm4, %%mm5\n"
                  "\tpaddw     %%mm7, %%mm5\n"
                  "\tmovq      %%mm5, %%mm4\n"
                  "\tpsrlw     $ 8, %%mm4\n"
                  "\tpaddw     %%mm5, %%mm4\n"
                  "\tpsrlw     $ 8, %%mm4\n"

                  "\tpackuswb  %%mm4, %%mm1\n"

                  "\tpcmpeqb   %%mm4, %%mm4\n"
                  "\tpsubb     %%mm1, %%mm4\n"

                  "\tmovq      %%mm0, %%mm1\n"
                  "\tpandn     %%mm4, %%mm1\n"

                  "\t" pminub(mm2,mm3,mm4) "\n"
                  "\tpand      %%mm0, %%mm3\n"

                  "\tpor       %%mm3, %%mm1\n"

                  "\tmovd    %%mm1, (%2);\n"
                  : /* empty */
                  : "r" (op.A), "r" (op.B), "r" (op.D)
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5");
  }

  asm("emms");
}

void
xxxgimp_composite_softlight_rgba8_rgba8_rgba8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("movq    %0,%%mm0"     :  : "m" (*rgba8_alpha_mask) : "%mm0");

  for (; op.n_pixels >= 2; op.n_pixels -= 2) {
    asm volatile ("  movq    (%0), %%mm2; addl  $8, %0\n"
                  "\tmovq    (%1), %%mm3; addl  $8, %1\n"
                  

                  "\tmovq    %%mm1, (%2); addl  $8, %2\n"
                  : "+r" (op.A), "+S" (op.B), "+D" (op.D)
                  : /* empty */
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4");
  }

  if (op.n_pixels) {
    asm volatile ("  movd    (%0), %%mm2;\n"
                  "\tmovd    (%1), %%mm3;\n"
                  
                  "\tmovd    %%mm1, (%2);\n"
                  : /* empty */
                  : "r" (op.A), "r" (op.B), "r" (op.D)
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4");
  }
  
  asm("emms");
}

void
gimp_composite_subtract_rgba8_rgba8_rgba8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("movq    %0,%%mm0"     :  : "m" (*rgba8_alpha_mask) : "%mm0");

  for (; op.n_pixels >= 2; op.n_pixels -= 2) {
    asm volatile ("  movq     (%0), %%mm2; addl $8, %0\n"
                  "\tmovq     (%1), %%mm3; addl $8, %1\n"

                  "\tmovq    %%mm2, %%mm4\n"
                  "\tpsubusb %%mm3, %%mm4\n"
                  
                  "\tmovq    %%mm0, %%mm1\n"
                  "\tpandn   %%mm4, %%mm1\n"
                  
                  "\t" pminub(mm3,mm2,mm4) "\n"

                  "\tpand    %%mm0, %%mm2\n"
                  "\tpor     %%mm2, %%mm1\n"
                  "\tmovq    %%mm1, (%2); addl $8, %2\n"
                  : "+r" (op.A), "+r" (op.B), "+r" (op.D)
                  : /* empty */
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5");
  }

  if (op.n_pixels) {
    asm volatile ("  movd     (%0), %%mm2;\n"
                  "\tmovd     (%1), %%mm3;\n"

                  "\tmovq    %%mm2, %%mm4\n"
                  "\tpsubusb %%mm3, %%mm4\n"
                  
                  "\tmovq    %%mm0, %%mm1\n"
                  "\tpandn   %%mm4, %%mm1\n"
                  
                  "\t" pminub(mm3,mm2,mm4) "\n"

                  "\tpand    %%mm0, %%mm2\n"
                  "\tpor     %%mm2, %%mm1\n"
                  "\tmovd    %%mm1, (%2); addl $8, %2\n"
                  : /* empty */
                  : "r" (op.A), "r" (op.B), "r" (op.D)
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5");
  }

  asm("emms");
}

void
gimp_composite_swap_rgba8_rgba8_rgba8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  for (; op.n_pixels >= 2; op.n_pixels -= 2) {
    asm volatile ("  movq    (%0), %%mm2\n"
                  "\tmovq    (%1), %%mm3\n"
                  "\tmovq    %%mm3, (%0)\n"
                  "\tmovq    %%mm2, (%1)\n"
                  "\taddl    $8, %0\n"
                  "\taddl    $8, %1\n"
                  : "+r" (op.A), "+r" (op.B)
                  : /* empty */
                  : "0", "1", "%mm1", "%mm2", "%mm3", "%mm4");
  }

  if (op.n_pixels) {
    asm volatile ("  movd    (%0), %%mm2\n"
                  "\tmovd    (%1), %%mm3\n"
                  "\tmovd    %%mm3, (%0)\n"
                  "\tmovd    %%mm2, (%1)\n"                  
                  : /* empty */
                  : "r" (op.A), "r" (op.B)
                  : "0", "1", "%mm1", "%mm2", "%mm3", "%mm4");
  }
  
  asm("emms");
}

void
gimp_composite_valueonly_rgba8_rgba8_rgba8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("movq    %0,%%mm0"     :  : "m" (*rgba8_alpha_mask) : "%mm0");

  for (; op.n_pixels >= 2; op.n_pixels -= 2) {
    asm volatile ("  movq    (%0), %%mm2; addl  $8, %0\n"
                  "\tmovq    (%1), %%mm3; addl  $8, %1\n"
                  

                  "\tmovq    %%mm1, (%2); addl  $8, %2\n"
                  : "+r" (op.A), "+S" (op.B), "+D" (op.D)
                  : /* empty */
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4");
  }

  if (op.n_pixels) {
    asm volatile ("  movd    (%0), %%mm2;\n"
                  "\tmovd    (%1), %%mm3;\n"
                  
                  "\tmovd    %%mm1, (%2);\n"
                  : /* empty */
                  : "r" (op.A), "r" (op.B), "r" (op.D)
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4");
  }
  
  asm("emms");
}


const static unsigned long v8_alpha_mask[2] = { 0xFF00FF00, 0xFF00FF00};
const static unsigned long v8_mul_shift[2] = { 0x00800080, 0x00800080 };

#if 0
void
gimp_composite_addition_va8_va8_va8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("pushl %edi");
  asm("pushl %ebx");
  asm("movl 12(%esp), %edi");
  asm("movq v8_alpha_mask, %mm0");

  asm("subl $ 4, %ecx");
  asm("jl .add_pixels_1a_1a_last3");
  asm("movl $ 8, %ebx");
  asm(".add_pixels_1a_1a_loop:");

  asm("movq (%eax), %mm2");
  asm("movq (%edx), %mm3");

  asm("movq %mm2, %mm4");
  asm("paddusb %mm3, %mm4");
  asm("movq %mm0, %mm1");
  asm("pandn %mm4, %mm1");
  asm("movq %mm2, %mm4");
  asm("psubusb %mm3, %mm4");
  asm("psubb %mm4, %mm2");
  asm("pand %mm0, %mm2");
  asm("por %mm2, %mm1");
  asm("movq %mm1, (%edi)");
  asm("addl %ebx, %eax");
  asm("addl %ebx, %edx");
  asm("addl %ebx, %edi");
  asm("subl $ 4, %ecx");
  asm("jge .add_pixels_1a_1a_loop");

  asm(".add_pixels_1a_1a_last3:");
  asm("test $ 2, %ecx");
  asm("jz .add_pixels_1a_1a_last1");
  asm("movd (%eax), %mm2");
  asm("movd (%edx), %mm3");

  asm("movq %mm2, %mm4");
  asm("paddusb %mm3, %mm4");
  asm("movq %mm0, %mm1");
  asm("pandn %mm4, %mm1");
  asm("movq %mm2, %mm4");
  asm("psubusb %mm3, %mm4");
  asm("psubb %mm4, %mm2");
  asm("pand %mm0, %mm2");
  asm("por %mm2, %mm1");
  asm("addl $ 4, %eax");
  asm("addl $ 4, %edx");
  asm("addl $ 4, %edi");

  asm(".add_pixels_1a_1a_last1:");
  asm("test $ 1, %ecx");
  asm("jz .add_pixels_1a_1a_end");

  asm("movw (%eax), %bx");
  asm("movd %ebx, %mm2");
  asm("movw (%edx), %bx");
  asm("movd %ebx, %mm3");

  asm("movq %mm2, %mm4");
  asm("paddusb %mm3, %mm4");
  asm("movq %mm0, %mm1");
  asm("pandn %mm4, %mm1");
  asm("movq %mm2, %mm4");
  asm("psubusb %mm3, %mm4");
  asm("psubb %mm4, %mm2");
  asm("pand %mm0, %mm2");
  asm("por %mm2, %mm1");
  asm("movd %mm1, %ebx");
  asm("movw %bx, (%edi)");

  asm(".add_pixels_1a_1a_end:");

  asm("emms");
  asm("popl %ebx");
  asm("popl %edi");
}

void
gimp_composite_burn_va8_va8_va8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("movq   %0,%%mm1"
      :
      : "m" (va8_alpha_mask)
      : "%mm1");

  for (; op.n_pixels >= 4; op.n_pixels -= 4) {
    asm volatile ("  movq      (%0),%%mm0; addl $8,%0\n"
                  "\tmovq      (%1),%%mm1; addl $8,%1\n"

                  "\tmovq      %3,%%mm2\n"
                  "\tpsubb     %%mm0,%%mm2\n" /* mm2 = 255 - A */
                  "\tpxor      %%mm4,%%mm4\n"
                  "\tpunpcklbw %%mm2,%%mm4\n" /* mm4 = (255- A) * 256  */

                  "\tmovq      %%mm1,%%mm3\n"
                  "\tpxor      %%mm5,%%mm5\n"
                  "\tpunpcklbw %%mm5,%%mm3\n"
                  "\tmovq      %4,%%mm5\n"
                  "\tpaddusw   %%mm3,%%mm5\n" /* mm5 = B + 1 */

                  "\t" pdivwqX(mm4,mm5,mm7) "\n"

                  "\tmovq      %3,%%mm2\n"
                  "\tpsubb     %%mm0,%%mm2\n" /* mm2 = 255 - A */
                  "\tpxor      %%mm4,%%mm4\n"
                  "\tpunpckhbw %%mm2,%%mm4\n" /* mm4 = (255- A) * 256  */

                  "\tmovq      %%mm1,%%mm3\n"
                  "\tpxor      %%mm5,%%mm5\n"
                  "\tpunpckhbw %%mm5,%%mm3\n"
                  "\tmovq      %4,%%mm5\n"
                  "\tpaddusw   %%mm3,%%mm5\n" /* mm5 = B + 1 */
                  "\t" pdivwqX(mm4,mm5,mm6) "\n"

                  "\tmovq      %5,%%mm4\n"
                  "\tmovq      %%mm4,%%mm5\n"
                  "\tpsubusw     %%mm6,%%mm4\n"
                  "\tpsubusw     %%mm7,%%mm5\n"
                  
                  "\tpackuswb  %%mm4,%%mm5\n"

                  "\t" pminub(mm0,mm1,mm3) "\n" /* mm1 = min(mm0,mm1) clobber mm3 */

                  "\tmovq      %6,%%mm7\n"
                  "\tpand      %%mm7,%%mm1\n" /* mm1 = mm7 & alpha_mask */

                  "\tpandn     %%mm5,%%mm7\n" /* mm7 = ~mm7 & mm5 */
                  "\tpor       %%mm1,%%mm7\n" /* mm7 = mm7 | mm1 */

                  "\tmovq      %%mm7,(%2); addl $8,%2\n"
                  : "+r" (op.A), "+r" (op.B), "+r" (op.D)
                  : "m" (va8_b255), "m" (va8_w1), "m" (va8_w255), "m" (va8_alpha_mask)
                  : "0", "1", "2", "%mm1", "%mm2", "%mm3", "%mm4");
  }

  if (op.n_pixels) {
    asm volatile ("  movd      (%0),%%mm0\n"
                  "\tmovd      (%1),%%mm1\n"

                  "\tmovq      %3,%%mm2\n"
                  "\tpsubb     %%mm0,%%mm2\n" /* mm2 = 255 - A */
                  "\tpxor      %%mm4,%%mm4\n"
                  "\tpunpcklbw %%mm2,%%mm4\n" /* mm4 = (255- A) * 256  */

                  "\tmovq      %%mm1,%%mm3\n"
                  "\tpxor      %%mm5,%%mm5\n"
                  "\tpunpcklbw %%mm5,%%mm3\n"
                  "\tmovq      %4,%%mm5\n"
                  "\tpaddusw   %%mm3,%%mm5\n" /* mm5 = B + 1 */

                  "\t" pdivwqX(mm4,mm5,mm7) "\n"

                  "\tmovq      %3,%%mm2\n"
                  "\tpsubb   %%mm0,%%mm2\n" /* mm2 = 255 - A */
                  "\tpxor      %%mm4,%%mm4\n"
                  "\tpunpckhbw %%mm2,%%mm4\n" /* mm4 = (255- A) * 256  */

                  "\tmovq      %%mm1,%%mm3\n"
                  "\tpxor      %%mm5,%%mm5\n"
                  "\tpunpckhbw %%mm5,%%mm3\n"
                  "\tmovq      %4,%%mm5\n"
                  "\tpaddusw   %%mm3,%%mm5\n" /* mm5 = B + 1 */
                  "\t" pdivwqX(mm4,mm5,mm6) "\n"

                  "\tmovq      %5,%%mm4\n"
                  "\tmovq      %%mm4,%%mm5\n"
                  "\tpsubusw     %%mm6,%%mm4\n"
                  "\tpsubusw     %%mm7,%%mm5\n"
                  
                  "\tpackuswb  %%mm4,%%mm5\n"

                  "\t" pminub(mm0,mm1,mm3) "\n" /* mm1 = min(mm0,mm1) clobber mm3 */

                  "\tmovq      %6,%%mm7\n"
                  "\tpand      %%mm7,%%mm1\n" /* mm1 = mm7 & alpha_mask */

                  "\tpandn     %%mm5,%%mm7\n" /* mm7 = ~mm7 & mm5 */
                  "\tpor       %%mm1,%%mm7\n" /* mm7 = mm7 | mm1 */

                  "\tmovd      %%mm7,(%2)\n"
                  : /* empty */
                  : "r" (op.A), "r" (op.B), "r" (op.D), "m" (va8_b255), "m" (va8_w1), "m" (va8_w255), "m" (va8_alpha_mask)
                  : "0", "1", "2", "%mm0", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5", "%mm6", "%mm7");
  }

  asm("emms");
}

void
xxxgimp_composite_coloronly_va8_va8_va8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

}

void
gimp_composite_darken_va8_va8_va8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("pushl %edi");
  asm("pushl %ebx");
  asm("movl 12(%esp), %edi");
  asm("movq v8_alpha_mask, %mm0");
  asm("subl $ 4, %ecx");
  asm("jl .darken_pixels_1a_1a_last3");
  asm("movl $ 8, %ebx");
  asm(".darken_pixels_1a_1a_loop:");
  asm("movq (%eax), %mm2");
  asm("movq (%edx), %mm3");

  asm("movq %mm2, %mm4");
  asm("psubusb %mm3, %mm4");
  asm("psubb %mm4, %mm2");
  asm("movq %mm2, %mm1");
  asm("movq %mm1, (%edi)");
  asm("addl %ebx, %eax");
  asm("addl %ebx, %edx");
  asm("addl %ebx, %edi");
  asm("subl $ 4, %ecx");
  asm("jge .darken_pixels_1a_1a_loop");

  asm(".darken_pixels_1a_1a_last3:");
  asm("test $ 2, %ecx");
  asm("jz .darken_pixels_1a_1a_last1");
  asm("movd (%eax), %mm2");
  asm("movd (%edx), %mm3");

  asm("movq %mm2, %mm4");
  asm("psubusb %mm3, %mm4");
  asm("psubb %mm4, %mm2");
  asm("movq %mm2, %mm1");
  asm("addl $ 4, %eax");
  asm("addl $ 4, %edx");
  asm("addl $ 4, %edi");

  asm(".darken_pixels_1a_1a_last1:");
  asm("test $ 1, %ecx");
  asm("jz .darken_pixels_1a_1a_end");

  asm("movw (%eax), %bx");
  asm("movd %ebx, %mm2");
  asm("movw (%edx), %bx");
  asm("movd %ebx, %mm3");

  asm("movq %mm2, %mm4");
  asm("psubusb %mm3, %mm4");
  asm("psubb %mm4, %mm2");
  asm("movq %mm2, %mm1");
  asm("movd %mm1, %ebx");
  asm("movw %bx, (%edi)");

  asm(".darken_pixels_1a_1a_end:");

  asm("emms");
  asm("popl %ebx");
  asm("popl %edi");
}

void
gimp_composite_difference_va8_va8_va8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("pushl %edi");
  asm("pushl %ebx");
  asm("movl 12(%esp), %edi");
  asm("movq v8_alpha_mask, %mm0");
  asm("subl $ 4, %ecx");
  asm("jl .difference_pixels_1a_1a_last3");
  asm("movl $ 8, %ebx");
  asm(".difference_pixels_1a_1a_loop:");
  asm("movq (%eax), %mm2");
  asm("movq (%edx), %mm3");

  asm("movq %mm2, %mm4");
  asm("movq %mm3, %mm5");
  asm("psubusb %mm3, %mm4");
  asm("psubusb %mm2, %mm5");
  asm("movq %mm0, %mm1");
  asm("paddb %mm5, %mm4");
  asm("pandn %mm4, %mm1");
  asm("psubb %mm4, %mm2");
  asm("pand %mm0, %mm2");
  asm("por %mm2, %mm1");
  asm("movq %mm1, (%edi)");
  asm("addl %ebx, %eax");
  asm("addl %ebx, %edx");
  asm("addl %ebx, %edi");
  asm("subl $ 4, %ecx");
  asm("jge .difference_pixels_1a_1a_loop");

  asm(".difference_pixels_1a_1a_last3:");
  asm("test $ 2, %ecx");
  asm("jz .difference_pixels_1a_1a_last1");
  asm("movd (%eax), %mm2");
  asm("movd (%edx), %mm3");

  asm("movq %mm2, %mm4");
  asm("movq %mm3, %mm5");
  asm("psubusb %mm3, %mm4");
  asm("psubusb %mm2, %mm5");
  asm("movq %mm0, %mm1");
  asm("paddb %mm5, %mm4");
  asm("pandn %mm4, %mm1");
  asm("psubb %mm4, %mm2");
  asm("pand %mm0, %mm2");
  asm("por %mm2, %mm1");
  asm("addl $ 4, %eax");
  asm("addl $ 4, %edx");
  asm("addl $ 4, %edi");

  asm(".difference_pixels_1a_1a_last1:");
  asm("test $ 1, %ecx");
  asm("jz .difference_pixels_1a_1a_end");

  asm("movw (%eax), %bx");
  asm("movd %ebx, %mm2");
  asm("movw (%edx), %bx");
  asm("movd %ebx, %mm3");

  asm("movq %mm2, %mm4");
  asm("movq %mm3, %mm5");
  asm("psubusb %mm3, %mm4");
  asm("psubusb %mm2, %mm5");
  asm("movq %mm0, %mm1");
  asm("paddb %mm5, %mm4");
  asm("pandn %mm4, %mm1");
  asm("psubb %mm4, %mm2");
  asm("pand %mm0, %mm2");
  asm("por %mm2, %mm1");
  asm("movd %mm1, %ebx");
  asm("movw %bx, (%edi)");

  asm(".difference_pixels_1a_1a_end:");

  asm("emms");
  asm("popl %ebx");
  asm("popl %edi");
}

void
xxxgimp_composite_dissolve_va8_va8_va8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

}

void
xxxgimp_composite_divide_va8_va8_va8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

}

void
xxxgimp_composite_dodge_va8_va8_va8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

}

void
xxxgimp_composite_grainextract_va8_va8_va8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

}

void
xxxgimp_composite_grainmerge_va8_va8_va8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

}

void
xxxgimp_composite_hardlight_va8_va8_va8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

}

void
xxxgimp_composite_hueonly_va8_va8_va8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

}

void
xxxgimp_composite_lighten_va8_va8_va8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("pushl %edi");
  asm("pushl %ebx");
  asm("movl 12(%esp), %edi");
  asm("movq v8_alpha_mask, %mm0");
  asm("subl $ 4, %ecx");
  asm("jl .lighten_pixels_1a_1a_last3");
  asm("movl $ 8, %ebx");
  asm(".lighten_pixels_1a_1a_loop:");
  asm("movq (%eax), %mm2");
  asm("movq (%edx), %mm3");

  asm("movq %mm2, %mm4");
  asm("psubusb %mm3, %mm4");
  asm("paddb %mm4, %mm3");
  asm("movq %mm0, %mm1");
  asm("pandn %mm3, %mm1");

  asm("psubb %mm4, %mm2");
  asm("pand %mm0, %mm2");
  asm("por %mm2, %mm1");
  asm("movq %mm1, (%edi)");
  asm("addl %ebx, %eax");
  asm("addl %ebx, %edx");
  asm("addl %ebx, %edi");
  asm("subl $ 4, %ecx");
  asm("jge .lighten_pixels_1a_1a_loop");

  asm(".lighten_pixels_1a_1a_last3:");
  asm("test $ 2, %ecx");
  asm("jz .lighten_pixels_1a_1a_last1");
  asm("movd (%eax), %mm2");
  asm("movd (%edx), %mm3");

  asm("movq %mm2, %mm4");
  asm("psubusb %mm3, %mm4");
  asm("paddb %mm4, %mm3");
  asm("movq %mm0, %mm1");
  asm("pandn %mm3, %mm1");

  asm("psubb %mm4, %mm2");
  asm("pand %mm0, %mm2");
  asm("por %mm2, %mm1");
  asm("addl $ 4, %eax");
  asm("addl $ 4, %edx");
  asm("addl $ 4, %edi");

  asm(".lighten_pixels_1a_1a_last1:");
  asm("test $ 1, %ecx");
  asm("jz .lighten_pixels_1a_1a_end");

  asm("movw (%eax), %bx");
  asm("movd %ebx, %mm2");
  asm("movw (%edx), %bx");
  asm("movd %ebx, %mm3");

  asm("movq %mm2, %mm4");
  asm("psubusb %mm3, %mm4");
  asm("paddb %mm4, %mm3");
  asm("movq %mm0, %mm1");
  asm("pandn %mm3, %mm1");

  asm("psubb %mm4, %mm2");
  asm("pand %mm0, %mm2");
  asm("por %mm2, %mm1");
  asm("movd %mm1, %ebx");
  asm("movw %bx, (%edi)");

  asm(".lighten_pixels_1a_1a_end:");

  asm("emms");
  asm("popl %ebx");
  asm("popl %edi");
}

void
xxxgimp_composite_multiply_va8_va8_va8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("pushl %edi");
  asm("pushl %ebx");
  asm("movl 12(%esp), %edi");
  asm("movq v8_alpha_mask, %mm0");
  asm("subl $ 4, %ecx");
  asm("jl .multiply_pixels_1a_1a_last3");
  asm("movl $ 8, %ebx");
  asm(".multiply_pixels_1a_1a_loop:");
  asm("movq (%eax), %mm2");
  asm("movq (%edx), %mm3");


  asm("movq %mm2, %mm1");
  asm("punpcklbw %mm6, %mm1");
  asm("movq %mm3, %mm5");
  asm("punpcklbw %mm6, %mm5");
  asm("pmullw %mm5, %mm1");
  asm("paddw %mm7, %mm1");
  asm("movq %mm1, %mm5");
  asm("psrlw $ 8, %mm5");
  asm("paddw %mm5, %mm1");
  asm("psrlw $ 8, %mm1");

  asm("movq %mm2, %mm4");
  asm("punpckhbw %mm6, %mm4");
  asm("movq %mm3, %mm5");
  asm("punpckhbw %mm6, %mm5");
  asm("pmullw %mm5, %mm4");
  asm("paddw %mm7, %mm4");
  asm("movq %mm4, %mm5");
  asm("psrlw $ 8, %mm5");
  asm("paddw %mm5, %mm4");
  asm("psrlw $ 8, %mm4");

  asm("packuswb %mm4, %mm1");

  asm("movq %mm0, %mm4");
  asm("pandn %mm1, %mm4");
  asm("movq %mm4, %mm1");

  asm("movq %mm2, %mm4");
  asm("psubusb %mm3, %mm4");
  asm("psubb %mm4, %mm2");
  asm("pand %mm0, %mm2");
  asm("por %mm2, %mm1");
  asm("movq %mm1, (%edi)");
  asm("addl %ebx, %eax");
  asm("addl %ebx, %edx");
  asm("addl %ebx, %edi");
  asm("subl $ 4, %ecx");
  asm("jge .multiply_pixels_1a_1a_loop");

  asm(".multiply_pixels_1a_1a_last3:");
  asm("test $ 2, %ecx");
  asm("jz .multiply_pixels_1a_1a_last1");
  asm("movd (%eax), %mm2");
  asm("movd (%edx), %mm3");


  asm("movq %mm2, %mm1");
  asm("punpcklbw %mm6, %mm1");
  asm("movq %mm3, %mm5");
  asm("punpcklbw %mm6, %mm5");
  asm("pmullw %mm5, %mm1");
  asm("paddw %mm7, %mm1");
  asm("movq %mm1, %mm5");
  asm("psrlw $ 8, %mm5");
  asm("paddw %mm5, %mm1");
  asm("psrlw $ 8, %mm1");

  asm("movq %mm2, %mm4");
  asm("punpckhbw %mm6, %mm4");
  asm("movq %mm3, %mm5");
  asm("punpckhbw %mm6, %mm5");
  asm("pmullw %mm5, %mm4");
  asm("paddw %mm7, %mm4");
  asm("movq %mm4, %mm5");
  asm("psrlw $ 8, %mm5");
  asm("paddw %mm5, %mm4");
  asm("psrlw $ 8, %mm4");

  asm("packuswb %mm4, %mm1");

  asm("movq %mm0, %mm4");
  asm("pandn %mm1, %mm4");
  asm("movq %mm4, %mm1");

  asm("movq %mm2, %mm4");
  asm("psubusb %mm3, %mm4");
  asm("psubb %mm4, %mm2");
  asm("pand %mm0, %mm2");
  asm("por %mm2, %mm1");
  asm("addl $ 4, %eax");
  asm("addl $ 4, %edx");
  asm("addl $ 4, %edi");

  asm(".multiply_pixels_1a_1a_last1:");
  asm("test $ 1, %ecx");
  asm("jz .multiply_pixels_1a_1a_end");

  asm("movw (%eax), %bx");
  asm("movd %ebx, %mm2");
  asm("movw (%edx), %bx");
  asm("movd %ebx, %mm3");


  asm("movq %mm2, %mm1");
  asm("punpcklbw %mm6, %mm1");
  asm("movq %mm3, %mm5");
  asm("punpcklbw %mm6, %mm5");
  asm("pmullw %mm5, %mm1");
  asm("paddw %mm7, %mm1");
  asm("movq %mm1, %mm5");
  asm("psrlw $ 8, %mm5");
  asm("paddw %mm5, %mm1");
  asm("psrlw $ 8, %mm1");

  asm("movq %mm2, %mm4");
  asm("punpckhbw %mm6, %mm4");
  asm("movq %mm3, %mm5");
  asm("punpckhbw %mm6, %mm5");
  asm("pmullw %mm5, %mm4");
  asm("paddw %mm7, %mm4");
  asm("movq %mm4, %mm5");
  asm("psrlw $ 8, %mm5");
  asm("paddw %mm5, %mm4");
  asm("psrlw $ 8, %mm4");

  asm("packuswb %mm4, %mm1");

  asm("movq %mm0, %mm4");
  asm("pandn %mm1, %mm4");
  asm("movq %mm4, %mm1");

  asm("movq %mm2, %mm4");
  asm("psubusb %mm3, %mm4");
  asm("psubb %mm4, %mm2");
  asm("pand %mm0, %mm2");
  asm("por %mm2, %mm1");
  asm("movd %mm1, %ebx");
  asm("movw %bx, (%edi)");

  asm(".multiply_pixels_1a_1a_end:");

  asm("emms");
  asm("popl %ebx");
  asm("popl %edi");
}

void
gimp_composite_overlay_va8_va8_va8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("pushl %edi");
  asm("pushl %ebx");
  asm("movl 12(%esp), %edi");
  asm("movq v8_alpha_mask, %mm0");
  asm("subl $ 4, %ecx");
  asm("jl .overlay_pixels_1a_1a_last3");
  asm("movl $ 8, %ebx");
  asm(".overlay_pixels_1a_1a_loop:");
  asm("movq (%eax), %mm2");
  asm("movq (%edx), %mm3");
  asm("call op_overlay");
  asm("movq %mm1, (%edi)");
  asm("addl %ebx, %eax");
  asm("addl %ebx, %edx");
  asm("addl %ebx, %edi");
  asm("subl $ 4, %ecx");
  asm("jge .overlay_pixels_1a_1a_loop");

  asm(".overlay_pixels_1a_1a_last3:");
  asm("test $ 2, %ecx");
  asm("jz .overlay_pixels_1a_1a_last1");
  asm("movd (%eax), %mm2");
  asm("movd (%edx), %mm3");
  asm("call op_overlay");
  asm("addl $ 4, %eax");
  asm("addl $ 4, %edx");
  asm("addl $ 4, %edi");

  asm(".overlay_pixels_1a_1a_last1:");
  asm("test $ 1, %ecx");
  asm("jz .overlay_pixels_1a_1a_end");

  asm("movw (%eax), %bx");
  asm("movd %ebx, %mm2");
  asm("movw (%edx), %bx");
  asm("movd %ebx, %mm3");
  asm("call op_overlay");
  asm("movd %mm1, %ebx");
  asm("movw %bx, (%edi)");

  asm(".overlay_pixels_1a_1a_end:");

  asm("emms");
  asm("popl %ebx");
  asm("popl %edi");
}

void
xxxgimp_composite_replace_va8_va8_va8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

}

void
xxxgimp_composite_saturationonly_va8_va8_va8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

}

void
xxxgimp_composite_screen_va8_va8_va8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("pushl %edi");
  asm("pushl %ebx");
  asm("movl 12(%esp), %edi");
  asm("movq v8_alpha_mask, %mm0");
  asm("subl $ 4, %ecx");
  asm("jl .screen_pixels_1a_1a_last3");
  asm("movl $ 8, %ebx");
  asm(".screen_pixels_1a_1a_loop:");
  asm("movq (%eax), %mm2");
  asm("movq (%edx), %mm3");


  asm("pcmpeqb %mm4, %mm4");
  asm("psubb %mm2, %mm4");
  asm("pcmpeqb %mm5, %mm5");
  asm("psubb %mm3, %mm5");

  asm("movq %mm4, %mm1");
  asm("punpcklbw %mm6, %mm1");
  asm("movq %mm5, %mm3");
  asm("punpcklbw %mm6, %mm3");
  asm("pmullw %mm3, %mm1");
  asm("paddw %mm7, %mm1");
  asm("movq %mm1, %mm3");
  asm("psrlw $ 8, %mm3");
  asm("paddw %mm3, %mm1");
  asm("psrlw $ 8, %mm1");

  asm("movq %mm4, %mm2");
  asm("punpckhbw %mm6, %mm2");
  asm("movq %mm5, %mm3");
  asm("punpckhbw %mm6, %mm3");
  asm("pmullw %mm3, %mm2");
  asm("paddw %mm7, %mm2");
  asm("movq %mm2, %mm3");
  asm("psrlw $ 8, %mm3");
  asm("paddw %mm3, %mm2");
  asm("psrlw $ 8, %mm2");

  asm("packuswb %mm2, %mm1");

  asm("pcmpeqb %mm3, %mm3");
  asm("psubb %mm1, %mm3");

  asm("movq %mm0, %mm1");
  asm("pandn %mm3, %mm1");

  asm("movq %mm2, %mm4");
  asm("psubusb %mm5, %mm2");
  asm("paddb %mm2, %mm5");
  asm("pcmpeqb %mm3, %mm3");
  asm("psubb %mm5, %mm3");

  asm("pand %mm0, %mm3");
  asm("por %mm3, %mm1");
  asm("movq %mm1, (%edi)");
  asm("addl %ebx, %eax");
  asm("addl %ebx, %edx");
  asm("addl %ebx, %edi");
  asm("subl $ 4, %ecx");
  asm("jge .screen_pixels_1a_1a_loop");

  asm(".screen_pixels_1a_1a_last3:");
  asm("test $ 2, %ecx");
  asm("jz .screen_pixels_1a_1a_last1");
  asm("movd (%eax), %mm2");
  asm("movd (%edx), %mm3");


  asm("pcmpeqb %mm4, %mm4");
  asm("psubb %mm2, %mm4");
  asm("pcmpeqb %mm5, %mm5");
  asm("psubb %mm3, %mm5");

  asm("movq %mm4, %mm1");
  asm("punpcklbw %mm6, %mm1");
  asm("movq %mm5, %mm3");
  asm("punpcklbw %mm6, %mm3");
  asm("pmullw %mm3, %mm1");
  asm("paddw %mm7, %mm1");
  asm("movq %mm1, %mm3");
  asm("psrlw $ 8, %mm3");
  asm("paddw %mm3, %mm1");
  asm("psrlw $ 8, %mm1");

  asm("movq %mm4, %mm2");
  asm("punpckhbw %mm6, %mm2");
  asm("movq %mm5, %mm3");
  asm("punpckhbw %mm6, %mm3");
  asm("pmullw %mm3, %mm2");
  asm("paddw %mm7, %mm2");
  asm("movq %mm2, %mm3");
  asm("psrlw $ 8, %mm3");
  asm("paddw %mm3, %mm2");
  asm("psrlw $ 8, %mm2");

  asm("packuswb %mm2, %mm1");

  asm("pcmpeqb %mm3, %mm3");
  asm("psubb %mm1, %mm3");

  asm("movq %mm0, %mm1");
  asm("pandn %mm3, %mm1");

  asm("movq %mm2, %mm4");
  asm("psubusb %mm5, %mm2");
  asm("paddb %mm2, %mm5");
  asm("pcmpeqb %mm3, %mm3");
  asm("psubb %mm5, %mm3");

  asm("pand %mm0, %mm3");
  asm("por %mm3, %mm1");
  asm("addl $ 4, %eax");
  asm("addl $ 4, %edx");
  asm("addl $ 4, %edi");

  asm(".screen_pixels_1a_1a_last1:");
  asm("test $ 1, %ecx");
  asm("jz .screen_pixels_1a_1a_end");

  asm("movw (%eax), %bx");
  asm("movd %ebx, %mm2");
  asm("movw (%edx), %bx");
  asm("movd %ebx, %mm3");


  asm("pcmpeqb %mm4, %mm4");
  asm("psubb %mm2, %mm4");
  asm("pcmpeqb %mm5, %mm5");
  asm("psubb %mm3, %mm5");

  asm("movq %mm4, %mm1");
  asm("punpcklbw %mm6, %mm1");
  asm("movq %mm5, %mm3");
  asm("punpcklbw %mm6, %mm3");
  asm("pmullw %mm3, %mm1");
  asm("paddw %mm7, %mm1");
  asm("movq %mm1, %mm3");
  asm("psrlw $ 8, %mm3");
  asm("paddw %mm3, %mm1");
  asm("psrlw $ 8, %mm1");

  asm("movq %mm4, %mm2");
  asm("punpckhbw %mm6, %mm2");
  asm("movq %mm5, %mm3");
  asm("punpckhbw %mm6, %mm3");
  asm("pmullw %mm3, %mm2");
  asm("paddw %mm7, %mm2");
  asm("movq %mm2, %mm3");
  asm("psrlw $ 8, %mm3");
  asm("paddw %mm3, %mm2");
  asm("psrlw $ 8, %mm2");

  asm("packuswb %mm2, %mm1");

  asm("pcmpeqb %mm3, %mm3");
  asm("psubb %mm1, %mm3");

  asm("movq %mm0, %mm1");
  asm("pandn %mm3, %mm1");

  asm("movq %mm2, %mm4");
  asm("psubusb %mm5, %mm2");
  asm("paddb %mm2, %mm5");
  asm("pcmpeqb %mm3, %mm3");
  asm("psubb %mm5, %mm3");

  asm("pand %mm0, %mm3");
  asm("por %mm3, %mm1");
  asm("movd %mm1, %ebx");
  asm("movw %bx, (%edi)");

  asm(".screen_pixels_1a_1a_end:");

  asm("emms");
  asm("popl %ebx");
  asm("popl %edi");
}

void
xxxgimp_composite_softlight_va8_va8_va8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

}

void
xxxgimp_composite_subtract_va8_va8_va8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  asm("pushl %edi");
  asm("pushl %ebx");
  asm("movl 12(%esp), %edi");
  asm("movq v8_alpha_mask, %mm0");
  asm("subl $ 4, %ecx");
  asm("jl .substract_pixels_1a_1a_last3");
  asm("movl $ 8, %ebx");
  asm(".substract_pixels_1a_1a_loop:");
  asm("movq (%eax), %mm2");
  asm("movq (%edx), %mm3");

  asm("movq %mm2, %mm4");
  asm("psubusb %mm3, %mm4");
  asm("movq %mm0, %mm1");
  asm("pandn %mm4, %mm1");
  asm("psubb %mm4, %mm2");
  asm("pand %mm0, %mm2");
  asm("por %mm2, %mm1");
  asm("movq %mm1, (%edi)");
  asm("addl %ebx, %eax");
  asm("addl %ebx, %edx");
  asm("addl %ebx, %edi");
  asm("subl $ 4, %ecx");
  asm("jge .substract_pixels_1a_1a_loop");

  asm(".substract_pixels_1a_1a_last3:");
  asm("test $ 2, %ecx");
  asm("jz .substract_pixels_1a_1a_last1");
  asm("movd (%eax), %mm2");
  asm("movd (%edx), %mm3");

  asm("movq %mm2, %mm4");
  asm("psubusb %mm3, %mm4");
  asm("movq %mm0, %mm1");
  asm("pandn %mm4, %mm1");
  asm("psubb %mm4, %mm2");
  asm("pand %mm0, %mm2");
  asm("por %mm2, %mm1");
  asm("addl $ 4, %eax");
  asm("addl $ 4, %edx");
  asm("addl $ 4, %edi");

  asm(".substract_pixels_1a_1a_last1:");
  asm("test $ 1, %ecx");
  asm("jz .substract_pixels_1a_1a_end");

  asm("movw (%eax), %bx");
  asm("movd %ebx, %mm2");
  asm("movw (%edx), %bx");
  asm("movd %ebx, %mm3");

  asm("movq %mm2, %mm4");
  asm("psubusb %mm3, %mm4");
  asm("movq %mm0, %mm1");
  asm("pandn %mm4, %mm1");
  asm("psubb %mm4, %mm2");
  asm("pand %mm0, %mm2");
  asm("por %mm2, %mm1");
  asm("movd %mm1, %ebx");
  asm("movw %bx, (%edi)");

  asm(".substract_pixels_1a_1a_end:");
  asm("emms");
  asm("popl %ebx");
  asm("popl %edi");
}

void
xxxgimp_composite_swap_va8_va8_va8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

}

void
xxxgimp_composite_valueonly_va8_va8_va8_sse(GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

}
#endif

#endif /* __GNUC__ > 3 */
#endif /* ARCH_X86 */
#endif /* USE_SSE */

void
gimp_composite_sse_init(void)
{

}
