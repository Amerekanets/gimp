/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
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

/* Misc data definitions used by the convert.c code module.  Moved
   out here simply to unclutter convert.c, mostly. */

#ifndef __CONVERT_DATA_H__
#define __CONVERT_DATA_H__

#include <glib.h>

/* 'web safe' palette. */
static const guchar webpal[] =
{
  255,255,255,255,255,204,255,255,153,255,255,102,255,255,51,255,255,0,255,
  204,255,255,204,204,255,204,153,255,204,102,255,204,51,255,204,0,255,153,
  255,255,153,204,255,153,153,255,153,102,255,153,51,255,153,0,255,102,255,
  255,102,204,255,102,153,255,102,102,255,102,51,255,102,0,255,51,255,255,
  51,204,255,51,153,255,51,102,255,51,51,255,51,0,255,0,255,255,0,
  204,255,0,153,255,0,102,255,0,51,255,0,0,204,255,255,204,255,204,
  204,255,153,204,255,102,204,255,51,204,255,0,204,204,255,204,204,204,204,
  204,153,204,204,102,204,204,51,204,204,0,204,153,255,204,153,204,204,153,
  153,204,153,102,204,153,51,204,153,0,204,102,255,204,102,204,204,102,153,
  204,102,102,204,102,51,204,102,0,204,51,255,204,51,204,204,51,153,204,
  51,102,204,51,51,204,51,0,204,0,255,204,0,204,204,0,153,204,0,
  102,204,0,51,204,0,0,153,255,255,153,255,204,153,255,153,153,255,102,
  153,255,51,153,255,0,153,204,255,153,204,204,153,204,153,153,204,102,153,
  204,51,153,204,0,153,153,255,153,153,204,153,153,153,153,153,102,153,153,
  51,153,153,0,153,102,255,153,102,204,153,102,153,153,102,102,153,102,51,
  153,102,0,153,51,255,153,51,204,153,51,153,153,51,102,153,51,51,153,
  51,0,153,0,255,153,0,204,153,0,153,153,0,102,153,0,51,153,0,
  0,102,255,255,102,255,204,102,255,153,102,255,102,102,255,51,102,255,0,
  102,204,255,102,204,204,102,204,153,102,204,102,102,204,51,102,204,0,102,
  153,255,102,153,204,102,153,153,102,153,102,102,153,51,102,153,0,102,102,
  255,102,102,204,102,102,153,102,102,102,102,102,51,102,102,0,102,51,255,
  102,51,204,102,51,153,102,51,102,102,51,51,102,51,0,102,0,255,102,
  0,204,102,0,153,102,0,102,102,0,51,102,0,0,51,255,255,51,255,
  204,51,255,153,51,255,102,51,255,51,51,255,0,51,204,255,51,204,204,
  51,204,153,51,204,102,51,204,51,51,204,0,51,153,255,51,153,204,51,
  153,153,51,153,102,51,153,51,51,153,0,51,102,255,51,102,204,51,102,
  153,51,102,102,51,102,51,51,102,0,51,51,255,51,51,204,51,51,153,
  51,51,102,51,51,51,51,51,0,51,0,255,51,0,204,51,0,153,51,
  0,102,51,0,51,51,0,0,0,255,255,0,255,204,0,255,153,0,255,
  102,0,255,51,0,255,0,0,204,255,0,204,204,0,204,153,0,204,102,
  0,204,51,0,204,0,0,153,255,0,153,204,0,153,153,0,153,102,0,
  153,51,0,153,0,0,102,255,0,102,204,0,102,153,0,102,102,0,102,
  51,0,102,0,0,51,255,0,51,204,0,51,153,0,51,102,0,51,51,
  0,51,0,0,0,255,0,0,204,0,0,153,0,0,102,0,0,51,0,0,0
};


/* the dither matrix, used for alpha dither and 'positional' dither */
#define DM_WIDTH 32
#define DM_WIDTHMASK ((DM_WIDTH)-1)
#define DM_HEIGHT 32
#define DM_HEIGHTMASK ((DM_HEIGHT)-1)
/* matrix values should be scaled/biased to 1..255 range */
/* this array is not const because it may be overwritten. */
static guchar DM[32][32] = {
  {  1,191, 48,239, 12,203, 60,251,  3,194, 51,242, 15,206, 63,254,  1,192, 49,240, 13,204, 61,252,  4,195, 52,243, 16,207, 64,255},
  {128, 64,175,112,140, 76,187,124,131, 67,178,115,143, 79,190,127,128, 65,176,112,140, 77,188,124,131, 68,179,115,143, 80,191,127},
  { 32,223, 16,207, 44,235, 28,219, 35,226, 19,210, 47,238, 31,222, 33,224, 17,208, 45,236, 29,220, 36,227, 20,211, 48,239, 32,223},
  {159, 96,144, 80,171,108,155, 92,162, 99,146, 83,174,111,158, 95,160, 97,144, 81,172,109,156, 93,163,100,147, 84,175,111,159, 96},
  {  8,199, 56,247,  4,195, 52,243, 11,202, 59,250,  7,198, 55,246,  9,200, 57,248,  5,196, 53,244, 12,203, 60,251,  8,199, 56,247},
  {136, 72,183,120,132, 68,179,116,139, 75,186,123,135, 71,182,119,136, 73,184,120,132, 69,180,116,139, 76,187,123,135, 72,183,119},
  { 40,231, 24,215, 36,227, 20,211, 43,234, 27,218, 39,230, 23,214, 41,232, 25,216, 37,228, 21,212, 44,235, 28,219, 40,231, 24,215},
  {167,104,151, 88,163,100,147, 84,170,107,154, 91,166,103,150, 87,168,105,152, 89,164,101,148, 85,171,108,155, 92,167,104,151, 88},
  {  2,193, 50,241, 14,205, 62,253,  1,192, 49,240, 13,204, 61,252,  3,194, 51,242, 15,206, 63,254,  2,193, 50,241, 14,205, 62,253},
  {130, 66,177,114,142, 78,189,126,129, 65,176,113,141, 77,188,125,130, 67,178,114,142, 79,190,126,129, 66,177,113,141, 78,189,125},
  { 34,225, 18,209, 46,237, 30,221, 33,224, 17,208, 45,236, 29,220, 35,226, 19,210, 47,238, 31,222, 34,225, 18,209, 46,237, 30,221},
  {161, 98,146, 82,173,110,157, 94,160, 97,145, 81,172,109,156, 93,162, 99,146, 83,174,110,158, 95,161, 98,145, 82,173,109,157, 94},
  { 10,201, 58,249,  6,197, 54,245,  9,200, 57,248,  5,196, 53,244, 11,202, 59,250,  7,198, 55,246, 10,201, 58,249,  6,197, 54,245},
  {138, 74,185,122,134, 70,181,118,137, 73,184,121,133, 69,180,117,138, 75,186,122,134, 71,182,118,137, 74,185,121,133, 70,181,117},
  { 42,233, 26,217, 38,229, 22,213, 41,232, 25,216, 37,228, 21,212, 43,234, 27,218, 39,230, 23,214, 42,233, 26,217, 38,229, 22,213},
  {169,106,153, 90,165,102,149, 86,168,105,152, 89,164,101,148, 85,170,107,154, 91,166,103,150, 87,169,106,153, 90,165,102,149, 86},
  {  1,192, 49,239, 13,204, 61,251,  4,195, 52,242, 16,207, 64,254,  1,191, 48,239, 13,203, 60,251,  4,194, 51,242, 16,206, 63,254},
  {128, 65,176,112,140, 76,188,124,131, 68,179,115,143, 79,191,127,128, 64,176,112,140, 76,187,124,131, 67,179,115,143, 79,190,127},
  { 33,223, 17,208, 45,235, 29,219, 36,226, 20,211, 48,238, 32,222, 33,223, 17,207, 44,235, 29,219, 36,226, 20,210, 47,238, 32,222},
  {160, 96,144, 80,172,108,156, 92,163, 99,147, 83,175,111,159, 95,160, 96,144, 80,172,108,156, 92,163, 99,147, 83,175,111,159, 95},
  {  9,200, 57,247,  5,196, 53,243, 12,203, 60,250,  8,199, 56,246,  9,199, 56,247,  5,195, 52,243, 12,202, 59,250,  8,198, 55,246},
  {136, 73,184,120,132, 69,180,116,139, 75,187,123,135, 72,183,119,136, 72,183,120,132, 68,180,116,139, 75,186,123,135, 71,182,119},
  { 41,231, 25,216, 37,227, 21,212, 44,234, 28,218, 40,230, 24,215, 40,231, 25,215, 37,227, 21,211, 43,234, 28,218, 39,230, 24,214},
  {168,104,152, 88,164,100,148, 84,171,107,155, 91,167,103,151, 87,168,104,152, 88,164,100,148, 84,171,107,155, 91,167,103,151, 87},
  {  3,194, 51,241, 15,206, 63,253,  2,193, 50,240, 14,205, 62,252,  3,193, 50,241, 15,205, 62,253,  2,192, 49,240, 14,204, 61,252},
  {130, 67,178,114,142, 78,190,126,129, 66,177,113,141, 77,189,125,130, 66,178,114,142, 78,189,126,129, 65,177,113,141, 77,188,125},
  { 35,225, 19,210, 47,237, 31,221, 34,224, 18,209, 46,236, 30,220, 35,225, 19,209, 46,237, 31,221, 34,224, 18,208, 45,236, 30,220},
  {162, 98,146, 82,174,110,158, 94,161, 97,145, 81,173,109,157, 93,162, 98,146, 82,174,110,158, 94,161, 97,145, 81,173,109,157, 93},
  { 11,202, 59,249,  7,198, 55,245, 10,201, 58,248,  6,197, 54,244, 11,201, 58,249,  7,197, 54,245, 10,200, 57,248,  6,196, 53,244},
  {138, 74,186,122,134, 71,182,118,137, 73,185,121,133, 70,181,117,138, 74,185,122,134, 70,182,118,137, 73,184,121,133, 69,181,117},
  { 43,233, 27,218, 39,229, 23,214, 42,232, 26,217, 38,228, 22,213, 42,233, 27,217, 38,229, 23,213, 41,232, 26,216, 37,228, 22,212},
  {170,106,154, 90,166,102,150, 86,169,105,153, 89,165,101,149, 85,170,106,154, 90,166,102,150, 86,169,105,153, 89,165,101,149, 85}
};

static const guchar DM_ORIGINAL[32][32] = {
  {  1,191, 48,239, 12,203, 60,251,  3,194, 51,242, 15,206, 63,254,  1,192, 49,240, 13,204, 61,252,  4,195, 52,243, 16,207, 64,255},
  {128, 64,175,112,140, 76,187,124,131, 67,178,115,143, 79,190,127,128, 65,176,112,140, 77,188,124,131, 68,179,115,143, 80,191,127},
  { 32,223, 16,207, 44,235, 28,219, 35,226, 19,210, 47,238, 31,222, 33,224, 17,208, 45,236, 29,220, 36,227, 20,211, 48,239, 32,223},
  {159, 96,144, 80,171,108,155, 92,162, 99,146, 83,174,111,158, 95,160, 97,144, 81,172,109,156, 93,163,100,147, 84,175,111,159, 96},
  {  8,199, 56,247,  4,195, 52,243, 11,202, 59,250,  7,198, 55,246,  9,200, 57,248,  5,196, 53,244, 12,203, 60,251,  8,199, 56,247},
  {136, 72,183,120,132, 68,179,116,139, 75,186,123,135, 71,182,119,136, 73,184,120,132, 69,180,116,139, 76,187,123,135, 72,183,119},
  { 40,231, 24,215, 36,227, 20,211, 43,234, 27,218, 39,230, 23,214, 41,232, 25,216, 37,228, 21,212, 44,235, 28,219, 40,231, 24,215},
  {167,104,151, 88,163,100,147, 84,170,107,154, 91,166,103,150, 87,168,105,152, 89,164,101,148, 85,171,108,155, 92,167,104,151, 88},
  {  2,193, 50,241, 14,205, 62,253,  1,192, 49,240, 13,204, 61,252,  3,194, 51,242, 15,206, 63,254,  2,193, 50,241, 14,205, 62,253},
  {130, 66,177,114,142, 78,189,126,129, 65,176,113,141, 77,188,125,130, 67,178,114,142, 79,190,126,129, 66,177,113,141, 78,189,125},
  { 34,225, 18,209, 46,237, 30,221, 33,224, 17,208, 45,236, 29,220, 35,226, 19,210, 47,238, 31,222, 34,225, 18,209, 46,237, 30,221},
  {161, 98,146, 82,173,110,157, 94,160, 97,145, 81,172,109,156, 93,162, 99,146, 83,174,110,158, 95,161, 98,145, 82,173,109,157, 94},
  { 10,201, 58,249,  6,197, 54,245,  9,200, 57,248,  5,196, 53,244, 11,202, 59,250,  7,198, 55,246, 10,201, 58,249,  6,197, 54,245},
  {138, 74,185,122,134, 70,181,118,137, 73,184,121,133, 69,180,117,138, 75,186,122,134, 71,182,118,137, 74,185,121,133, 70,181,117},
  { 42,233, 26,217, 38,229, 22,213, 41,232, 25,216, 37,228, 21,212, 43,234, 27,218, 39,230, 23,214, 42,233, 26,217, 38,229, 22,213},
  {169,106,153, 90,165,102,149, 86,168,105,152, 89,164,101,148, 85,170,107,154, 91,166,103,150, 87,169,106,153, 90,165,102,149, 86},
  {  1,192, 49,239, 13,204, 61,251,  4,195, 52,242, 16,207, 64,254,  1,191, 48,239, 13,203, 60,251,  4,194, 51,242, 16,206, 63,254},
  {128, 65,176,112,140, 76,188,124,131, 68,179,115,143, 79,191,127,128, 64,176,112,140, 76,187,124,131, 67,179,115,143, 79,190,127},
  { 33,223, 17,208, 45,235, 29,219, 36,226, 20,211, 48,238, 32,222, 33,223, 17,207, 44,235, 29,219, 36,226, 20,210, 47,238, 32,222},
  {160, 96,144, 80,172,108,156, 92,163, 99,147, 83,175,111,159, 95,160, 96,144, 80,172,108,156, 92,163, 99,147, 83,175,111,159, 95},
  {  9,200, 57,247,  5,196, 53,243, 12,203, 60,250,  8,199, 56,246,  9,199, 56,247,  5,195, 52,243, 12,202, 59,250,  8,198, 55,246},
  {136, 73,184,120,132, 69,180,116,139, 75,187,123,135, 72,183,119,136, 72,183,120,132, 68,180,116,139, 75,186,123,135, 71,182,119},
  { 41,231, 25,216, 37,227, 21,212, 44,234, 28,218, 40,230, 24,215, 40,231, 25,215, 37,227, 21,211, 43,234, 28,218, 39,230, 24,214},
  {168,104,152, 88,164,100,148, 84,171,107,155, 91,167,103,151, 87,168,104,152, 88,164,100,148, 84,171,107,155, 91,167,103,151, 87},
  {  3,194, 51,241, 15,206, 63,253,  2,193, 50,240, 14,205, 62,252,  3,193, 50,241, 15,205, 62,253,  2,192, 49,240, 14,204, 61,252},
  {130, 67,178,114,142, 78,190,126,129, 66,177,113,141, 77,189,125,130, 66,178,114,142, 78,189,126,129, 65,177,113,141, 77,188,125},
  { 35,225, 19,210, 47,237, 31,221, 34,224, 18,209, 46,236, 30,220, 35,225, 19,209, 46,237, 31,221, 34,224, 18,208, 45,236, 30,220},
  {162, 98,146, 82,174,110,158, 94,161, 97,145, 81,173,109,157, 93,162, 98,146, 82,174,110,158, 94,161, 97,145, 81,173,109,157, 93},
  { 11,202, 59,249,  7,198, 55,245, 10,201, 58,248,  6,197, 54,244, 11,201, 58,249,  7,197, 54,245, 10,200, 57,248,  6,196, 53,244},
  {138, 74,186,122,134, 71,182,118,137, 73,185,121,133, 70,181,117,138, 74,185,122,134, 70,182,118,137, 73,184,121,133, 69,181,117},
  { 43,233, 27,218, 39,229, 23,214, 42,232, 26,217, 38,228, 22,213, 42,233, 27,217, 38,229, 23,213, 41,232, 26,216, 37,228, 22,212},
  {170,106,154, 90,166,102,150, 86,169,105,153, 89,165,101,149, 85,170,106,154, 90,166,102,150, 86,169,105,153, 89,165,101,149, 85}
};

#endif /* __CONVERT_DATA_H__ */
