/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimppreviewrendererbrush.c
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

#include <gtk/gtk.h>

#include "widgets-types.h"

#include "base/temp-buf.h"

#include "core/gimpbrush.h"
#include "core/gimpbrushpipe.h"

#include "gimppreviewrendererbrush.h"


static void   gimp_preview_renderer_brush_class_init (GimpPreviewRendererBrushClass *klass);
static void   gimp_preview_renderer_brush_init       (GimpPreviewRendererBrush      *preview);

static void     gimp_preview_renderer_brush_finalize       (GObject     *object);
static void     gimp_preview_renderer_brush_render         (GimpPreviewRenderer *renderer,
                                                            GtkWidget           *widget);

static gboolean gimp_preview_renderer_brush_render_timeout (gpointer     data);


static GimpPreviewRendererClass *parent_class = NULL;


GType
gimp_preview_renderer_brush_get_type (void)
{
  static GType renderer_type = 0;

  if (! renderer_type)
    {
      static const GTypeInfo renderer_info =
      {
        sizeof (GimpPreviewRendererBrushClass),
        NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) gimp_preview_renderer_brush_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GimpPreviewRendererBrush),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gimp_preview_renderer_brush_init,
      };

      renderer_type = g_type_register_static (GIMP_TYPE_PREVIEW_RENDERER,
                                              "GimpPreviewRendererBrush",
                                              &renderer_info, 0);
    }
  
  return renderer_type;
}

static void
gimp_preview_renderer_brush_class_init (GimpPreviewRendererBrushClass *klass)
{
  GObjectClass             *object_class;
  GimpPreviewRendererClass *renderer_class;

  object_class   = G_OBJECT_CLASS (klass);
  renderer_class = GIMP_PREVIEW_RENDERER_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gimp_preview_renderer_brush_finalize;

  renderer_class->render = gimp_preview_renderer_brush_render;
}

static void
gimp_preview_renderer_brush_init (GimpPreviewRendererBrush *renderer)
{
  renderer->pipe_timeout_id       = 0;
  renderer->pipe_animation_index  = 0;
  renderer->pipe_animation_widget = NULL;
}

static void
gimp_preview_renderer_brush_finalize (GObject *object)
{
  GimpPreviewRendererBrush *renderer;

  renderer = GIMP_PREVIEW_RENDERER_BRUSH (object);

  if (renderer->pipe_timeout_id)
    {
      g_source_remove (renderer->pipe_timeout_id);
      renderer->pipe_timeout_id = 0;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_preview_renderer_brush_render (GimpPreviewRenderer *renderer,
                                    GtkWidget           *widget)
{
  GimpPreviewRendererBrush *renderbrush;
  GimpBrush                *brush;
  TempBuf                  *temp_buf;
  gint                      brush_width;
  gint                      brush_height;

  renderbrush = GIMP_PREVIEW_RENDERER_BRUSH (renderer);

  if (renderbrush->pipe_timeout_id)
    {
      g_source_remove (renderbrush->pipe_timeout_id);
      renderbrush->pipe_timeout_id = 0;
    }

  brush        = GIMP_BRUSH (renderer->viewable);
  brush_width  = brush->mask->width;
  brush_height = brush->mask->height;

  temp_buf = gimp_viewable_get_new_preview (renderer->viewable,
                                            renderer->width,
                                            renderer->height);

  if (temp_buf->width < renderer->width)
    temp_buf->x = (renderer->width - temp_buf->width) / 2;

  if (temp_buf->height < renderer->height)
    temp_buf->y = (renderer->height - temp_buf->height) / 2;

  if (renderer->is_popup)
    {
      gimp_preview_renderer_render_preview (renderer, temp_buf, -1,
                                            GIMP_PREVIEW_BG_WHITE,
                                            GIMP_PREVIEW_BG_WHITE);

      temp_buf_free (temp_buf);

      if (GIMP_IS_BRUSH_PIPE (brush))
	{
	  if (renderer->width  != brush_width ||
              renderer->height != brush_height)
	    {
	      g_warning ("%s(): non-fullsize pipe popups are not supported yet.",
			 G_GNUC_FUNCTION);
	      return;
	    }

          renderbrush->pipe_animation_widget = widget;
	  renderbrush->pipe_animation_index  = 0;
	  renderbrush->pipe_timeout_id       =
            g_timeout_add (300, gimp_preview_renderer_brush_render_timeout,
                           renderbrush);
	}

      return;
    }

#define INDICATOR_WIDTH  7
#define INDICATOR_HEIGHT 7

  if (temp_buf->width  >= INDICATOR_WIDTH  &&
      temp_buf->height >= INDICATOR_HEIGHT &&
      (renderer->width  < brush_width  ||
       renderer->height < brush_height ||
       GIMP_IS_BRUSH_PIPE (brush)))
    {
#define WHT { 255, 255, 255 }
#define BLK {   0,   0,   0 }
#define RED { 255, 127, 127 }

      static const guchar scale_indicator_bits[7][7][3] = 
      {
        { WHT, WHT, WHT, WHT, WHT, WHT, WHT },
        { WHT, WHT, WHT, BLK, WHT, WHT, WHT },
        { WHT, WHT, WHT, BLK, WHT, WHT, WHT },
        { WHT, BLK, BLK, BLK, BLK, BLK, WHT },
        { WHT, WHT, WHT, BLK, WHT, WHT, WHT },
        { WHT, WHT, WHT, BLK, WHT, WHT, WHT },
        { WHT, WHT, WHT, WHT, WHT, WHT, WHT }
      };

      static const guchar scale_pipe_indicator_bits[7][7][3] = 
      {
        { WHT, WHT, WHT, WHT, WHT, WHT, WHT },
        { WHT, WHT, WHT, BLK, WHT, WHT, RED },
        { WHT, WHT, WHT, BLK, WHT, RED, RED },
        { WHT, BLK, BLK, BLK, BLK, BLK, RED },
        { WHT, WHT, WHT, BLK, RED, RED, RED },
        { WHT, WHT, RED, BLK, RED, RED, RED },
        { WHT, RED, RED, RED, RED, RED, RED }
      };

      static const guchar pipe_indicator_bits[7][7][3] = 
      {
        { WHT, WHT, WHT, WHT, WHT, WHT, WHT },
        { WHT, WHT, WHT, WHT, WHT, WHT, RED },
        { WHT, WHT, WHT, WHT, WHT, RED, RED },
        { WHT, WHT, WHT, WHT, RED, RED, RED },
        { WHT, WHT, WHT, RED, RED, RED, RED },
        { WHT, WHT, RED, RED, RED, RED, RED },
        { WHT, RED, RED, RED, RED, RED, RED }
      };

#undef WHT
#undef BLK
#undef RED

      guchar   *buf;
      gint      x, y;
      gint      offset_x;
      gint      offset_y;
      gboolean  alpha;
      gboolean  pipe;
      gboolean  scale;

      buf = temp_buf_data (temp_buf);

      offset_x = temp_buf->width  - INDICATOR_WIDTH;
      offset_y = temp_buf->height - INDICATOR_HEIGHT;

      buf += (offset_y * temp_buf->width + offset_x) * temp_buf->bytes;

      pipe  = GIMP_IS_BRUSH_PIPE (brush);
      scale = (renderer->width  < brush_width ||
               renderer->height < brush_height);
      alpha = (temp_buf->bytes == 4);

      for (y = 0; y < INDICATOR_HEIGHT; y++)
        {
          for (x = 0; x < INDICATOR_WIDTH; x++)
            {
              if (scale)
                {
                  if (pipe)
                    {
                      *buf++ = scale_pipe_indicator_bits[y][x][0];
                      *buf++ = scale_pipe_indicator_bits[y][x][1];
                      *buf++ = scale_pipe_indicator_bits[y][x][2];
                    }
                  else
                    {
                      *buf++ = scale_indicator_bits[y][x][0];
                      *buf++ = scale_indicator_bits[y][x][1];
                      *buf++ = scale_indicator_bits[y][x][2];
                    }
                }
              else if (pipe)
                {
                  *buf++ = pipe_indicator_bits[y][x][0];
                  *buf++ = pipe_indicator_bits[y][x][1];
                  *buf++ = pipe_indicator_bits[y][x][2];
                }

              if (alpha)
                *buf++ = 255;
            }

          buf += (temp_buf->width - INDICATOR_WIDTH) * temp_buf->bytes;
        }
    }

#undef INDICATOR_WIDTH
#undef INDICATOR_HEIGHT

  gimp_preview_renderer_render_preview (renderer, temp_buf, -1,
                                        GIMP_PREVIEW_BG_WHITE,
                                        GIMP_PREVIEW_BG_WHITE);

  temp_buf_free (temp_buf);
}

static gboolean
gimp_preview_renderer_brush_render_timeout (gpointer data)
{
  GimpPreviewRendererBrush *renderbrush;
  GimpPreviewRenderer      *renderer;
  GimpBrushPipe            *brush_pipe;
  GimpBrush                *brush;
  TempBuf                  *temp_buf;

  renderbrush = GIMP_PREVIEW_RENDERER_BRUSH (data);
  renderer    = GIMP_PREVIEW_RENDERER (data);

  if (! renderer->viewable)
    {
      renderbrush->pipe_timeout_id       = 0;
      renderbrush->pipe_animation_index  = 0;
      renderbrush->pipe_animation_widget = NULL;

      return FALSE;
    }

  brush_pipe = GIMP_BRUSH_PIPE (renderer->viewable);

  renderbrush->pipe_animation_index++;

  if (renderbrush->pipe_animation_index >= brush_pipe->nbrushes)
    renderbrush->pipe_animation_index = 0;

  brush =
    GIMP_BRUSH (brush_pipe->brushes[renderbrush->pipe_animation_index]);

  temp_buf = gimp_viewable_get_new_preview (GIMP_VIEWABLE (brush),
					    renderer->width,
					    renderer->height);

  gimp_preview_renderer_render_preview (renderer, temp_buf, -1,
                                        GIMP_PREVIEW_BG_WHITE,
                                        GIMP_PREVIEW_BG_WHITE);

  temp_buf_free (temp_buf);

  gtk_widget_queue_draw (renderbrush->pipe_animation_widget);

  return TRUE;
}
