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

#include "config.h"

#include <gtk/gtk.h>

#include "display-types.h"
#include "tools/tools-types.h"

#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimpimage-projection.h"
#include "core/gimplist.h"

#include "widgets/gimpitemfactory.h"

#include "libgimptool/gimptool.h"

#include "tools/tool_manager.h"

#include "gimpdisplay.h"
#include "gimpdisplay-area.h"
#include "gimpdisplay-handlers.h"
#include "gimpdisplayshell.h"
#include "gimpdisplayshell-handlers.h"
#include "gimpdisplayshell-transform.h"

#include "libgimp/gimpintl.h"


enum
{
  PROP_0,
  PROP_IMAGE
};


/*  local function prototypes  */

static void       gimp_display_class_init            (GimpDisplayClass *klass);
static void       gimp_display_init                  (GimpDisplay      *gdisp);

static void       gimp_display_finalize              (GObject          *object);
static void       gimp_display_set_property          (GObject          *object,
                                                      guint             property_id,
                                                      const GValue     *value,
                                                      GParamSpec       *pspec);
static void       gimp_display_get_property          (GObject          *object,
                                                      guint             property_id,
                                                      GValue           *value,
                                                      GParamSpec       *pspec);

static void       gimp_display_flush_whenever        (GimpDisplay      *gdisp, 
                                                      gboolean          now);
static void       gimp_display_idlerender_init       (GimpDisplay      *gdisp);
static gboolean   gimp_display_idlerender_callback   (gpointer          data);
static gboolean   gimp_display_idle_render_next_area (GimpDisplay      *gdisp);
static void       gimp_display_paint_area            (GimpDisplay      *gdisp,
                                                      gint              x,
                                                      gint              y,
                                                      gint              w,
                                                      gint              h);


static GimpObjectClass *parent_class = NULL;


GType
gimp_display_get_type (void)
{
  static GType display_type = 0;

  if (! display_type)
    {
      static const GTypeInfo display_info =
      {
        sizeof (GimpDisplayClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_display_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data     */
	sizeof (GimpDisplay),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_display_init,
      };

      display_type = g_type_register_static (GIMP_TYPE_OBJECT,
                                             "GimpDisplay",
                                             &display_info, 0);
    }

  return display_type;
}

static void
gimp_display_class_init (GimpDisplayClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize     = gimp_display_finalize;
  object_class->set_property = gimp_display_set_property;
  object_class->get_property = gimp_display_get_property;

  g_object_class_install_property (object_class, PROP_IMAGE,
				   g_param_spec_object ("image",
							NULL, NULL,
							GIMP_TYPE_IMAGE,
							G_PARAM_READABLE));
}

static void
gimp_display_init (GimpDisplay *gdisp)
{
  gdisp->ID                       = 0;

  gdisp->gimage                   = NULL;
  gdisp->instance                 = 0;

  gdisp->shell                    = NULL;

  gdisp->update_areas             = NULL;

  gdisp->idle_render.idle_id      = 0;
  gdisp->idle_render.update_areas = NULL;
}

static void
gimp_display_finalize (GObject *object)
{
  GimpDisplay *gdisp;

  gdisp = GIMP_DISPLAY (object);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_display_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GimpDisplay *gdisp;

  gdisp = GIMP_DISPLAY (object);

  switch (property_id)
    {
    case PROP_IMAGE:
      g_assert_not_reached ();
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_display_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GimpDisplay *gdisp;

  gdisp = GIMP_DISPLAY (object);

  switch (property_id)
    {
    case PROP_IMAGE:
      g_value_set_object (value, gdisp->gimage);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

GimpDisplay *
gimp_display_new (GimpImage       *gimage,
                  guint            scale,
                  GimpMenuFactory *menu_factory,
                  GimpItemFactory *popup_factory)
{
  GimpDisplay *gdisp;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  /*  If there isn't an interface, never create a gdisplay  */
  if (gimage->gimp->no_interface)
    return NULL;

  gdisp = g_object_new (GIMP_TYPE_DISPLAY, NULL);

  gdisp->ID = gimage->gimp->next_display_ID++;

  /*  refs the image  */
  gimp_display_connect (gdisp, gimage);

  /*  create the shell for the image  */
  gdisp->shell = gimp_display_shell_new (gdisp, scale,
                                         menu_factory, popup_factory);

  gtk_widget_show (gdisp->shell);

  return gdisp;
}

void
gimp_display_delete (GimpDisplay *gdisp)
{
  GimpTool *active_tool;

  g_return_if_fail (GIMP_IS_DISPLAY (gdisp));

  /* remove the display from the list */
  gimp_container_remove (gdisp->gimage->gimp->displays,
                         GIMP_OBJECT (gdisp));

  /*  stop any active tool  */
  tool_manager_control_active (gdisp->gimage->gimp, HALT, gdisp);

  active_tool = tool_manager_get_active (gdisp->gimage->gimp);

  /*  clear out the pointer to this gdisp from the active tool  */
  if (active_tool && active_tool->gdisp == gdisp)
    {
      active_tool->drawable = NULL;
      active_tool->gdisp    = NULL;
    }

  /* If this gdisplay was idlerendering at the time when it was deleted,
   * deactivate the idlerendering thread before deletion!
   */
  if (gdisp->idle_render.idle_id)
    {
      g_source_remove (gdisp->idle_render.idle_id);
      gdisp->idle_render.idle_id = 0;
    }

  /*  free the update area lists  */
  gimp_display_area_list_free (gdisp->update_areas);
  gimp_display_area_list_free (gdisp->idle_render.update_areas);

  if (gdisp->shell)
    {
      gtk_widget_destroy (gdisp->shell);
      gdisp->shell = NULL;
    }

  /*  unrefs the gimage  */
  gimp_display_disconnect (gdisp);

  g_object_unref (gdisp);
}

gint
gimp_display_get_ID (GimpDisplay *gdisp)
{
  g_return_val_if_fail (GIMP_IS_DISPLAY (gdisp), -1);

  return gdisp->ID;
}

GimpDisplay *
gimp_display_get_by_ID (Gimp *gimp,
                        gint  ID)
{
  GimpDisplay *gdisp;
  GList       *list;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);

  for (list = GIMP_LIST (gimp->displays)->list;
       list;
       list = g_list_next (list))
    {
      gdisp = (GimpDisplay *) list->data;

      if (gdisp->ID == ID)
	return gdisp;
    }

  return NULL;
}

void
gimp_display_reconnect (GimpDisplay *gdisp, 
                        GimpImage   *gimage)
{
  g_return_if_fail (GIMP_IS_DISPLAY (gdisp));
  g_return_if_fail (GIMP_IS_IMAGE (gimage));

  if (gdisp->idle_render.idle_id)
    {
      g_source_remove (gdisp->idle_render.idle_id);
      gdisp->idle_render.idle_id = 0;
    }

  gimp_display_shell_disconnect (GIMP_DISPLAY_SHELL (gdisp->shell));

  gimp_display_disconnect (gdisp);

  gimp_display_connect (gdisp, gimage);

  gimp_display_add_update_area (gdisp,
                                0, 0,
                                gdisp->gimage->width,
                                gdisp->gimage->height);

  gimp_display_shell_reconnect (GIMP_DISPLAY_SHELL (gdisp->shell));
}

void
gimp_display_add_update_area (GimpDisplay *gdisp,
                              gint         x,
                              gint         y,
                              gint         w,
                              gint         h)
{
  GimpArea *area;

  g_return_if_fail (GIMP_IS_DISPLAY (gdisp));

  area = gimp_area_new (CLAMP (x, 0, gdisp->gimage->width),
                        CLAMP (y, 0, gdisp->gimage->height),
                        CLAMP (x + w, 0, gdisp->gimage->width),
                        CLAMP (y + h, 0, gdisp->gimage->height));

  gdisp->update_areas = gimp_display_area_list_process (gdisp->update_areas,
                                                        area);
}

void
gimp_display_flush (GimpDisplay *gdisp)
{
  g_return_if_fail (GIMP_IS_DISPLAY (gdisp));

  /* Redraw on idle time */
  gimp_display_flush_whenever (gdisp, FALSE);
}

void
gimp_display_flush_now (GimpDisplay *gdisp)
{
  g_return_if_fail (GIMP_IS_DISPLAY (gdisp));

  /* Redraw NOW */
  gimp_display_flush_whenever (gdisp, TRUE);
}

/* Force all gdisplays to finish their idlerender projection */
void
gimp_display_finish_draw (GimpDisplay *gdisp)
{
  g_return_if_fail (GIMP_IS_DISPLAY (gdisp));

  if (gdisp->idle_render.idle_id)
    {
      g_source_remove (gdisp->idle_render.idle_id);
      gdisp->idle_render.idle_id = 0;

      while (gimp_display_idlerender_callback (gdisp));
    }
}

/* utility function to check if the cursor is inside the active drawable */
gboolean
gimp_display_coords_in_active_drawable (GimpDisplay      *gdisp,
                                        const GimpCoords *coords)
{
  GimpDrawable *drawable;
  gint          x, y;

  g_return_val_if_fail (GIMP_IS_DISPLAY (gdisp), FALSE);

  if (!gdisp->gimage)
    return FALSE;

  if (!(drawable = gimp_image_active_drawable (gdisp->gimage)))
    return FALSE;

  gimp_drawable_offsets (drawable, &x, &y);

  x = ROUND (coords->x) - x;
  if (x < 0 || x > gimp_drawable_width (drawable))
    return FALSE;

  y = ROUND (coords->y) - y;
  if (y < 0 || y > gimp_drawable_height (drawable))
    return FALSE;

  return TRUE;
}

/*  private functions  */

static void
gimp_display_flush_whenever (GimpDisplay *gdisp, 
                             gboolean     now)
{
  GimpDisplayShell *shell;

  shell = GIMP_DISPLAY_SHELL (gdisp->shell);

  /*  Flush the items in the displays and updates lists -
   *  but only if gdisplay has been mapped and exposed
   */
  if (! shell->select)
    {
      g_warning ("gimp_display_flush_whenever(): called unrealized");
      return;
    }

  /*  First the updates...  */
  if (now)
    {
      /* Synchronous */

      GSList   *list;
      GimpArea *area;

      for (list = gdisp->update_areas; list; list = g_slist_next (list))
	{
	  /*  Paint the area specified by the GimpArea  */
	  area = (GimpArea *) list->data;

	  if ((area->x1 != area->x2) && (area->y1 != area->y2))
	    {
	      gimp_display_paint_area (gdisp,
                                       area->x1,
                                       area->y1,
                                       (area->x2 - area->x1),
                                       (area->y2 - area->y1));
	    }
	}
    }
  else
    {
      /* Asynchronous */

      if (gdisp->update_areas)
	gimp_display_idlerender_init (gdisp);
    }

  /*  Free the update lists  */
  gdisp->update_areas = gimp_display_area_list_free (gdisp->update_areas);

  /*  Next the displays...  */
  gimp_display_shell_flush (shell);

  /*  ensure the consistency of the menus  */
  if (! now)
    {
      GimpContext *context;

      gimp_item_factory_update (shell->menubar_factory, shell);

      context = gimp_get_current_context (gdisp->gimage->gimp);

      if (gdisp == gimp_context_get_display (context))
        gimp_item_factory_update (shell->popup_factory, shell);
    }
}

static void
gimp_display_idlerender_init (GimpDisplay *gdisp)
{
  GSList    *list;
  GimpArea  *area, *new_area;

  /* We need to merge the IdleRender's and the GimpDisplay's update_areas list
   * to keep track of which of the updates have been flushed and hence need
   * to be drawn. 
   */
  for (list = gdisp->update_areas; list; list = g_slist_next (list))
    {
      area = (GimpArea *) list->data;

      new_area = g_memdup (area, sizeof (GimpArea));

      gdisp->idle_render.update_areas =
	gimp_display_area_list_process (gdisp->idle_render.update_areas,
                                        new_area);
    }

  /* If an idlerender was already running, merge the remainder of its
   * unrendered area with the update_areas list, and make it start work
   * on the next unrendered area in the list.
   */
  if (gdisp->idle_render.idle_id)
    {
      new_area =
        gimp_area_new (gdisp->idle_render.basex,
                       gdisp->idle_render.y,
                       gdisp->idle_render.basex + gdisp->idle_render.width,
                       gdisp->idle_render.y + (gdisp->idle_render.height -
                                               (gdisp->idle_render.y -
                                                gdisp->idle_render.basey)));

      gdisp->idle_render.update_areas =
	gimp_display_area_list_process (gdisp->idle_render.update_areas,
                                        new_area);

      gimp_display_idle_render_next_area (gdisp);
    }
  else
    {
      if (gdisp->idle_render.update_areas == NULL)
	{
	  g_warning ("Wanted to start idlerender thread with no update_areas. (+memleak)");
	  return;
	}

      gimp_display_idle_render_next_area (gdisp);

      gdisp->idle_render.idle_id = g_idle_add_full (G_PRIORITY_LOW,
                                                    gimp_display_idlerender_callback,
                                                    gdisp,
                                                    NULL);
    }

  /* Caller frees gdisp->update_areas */
}

/* Unless specified otherwise, display re-rendering is organised by
 * IdleRender, which amalgamates areas to be re-rendered and breaks
 * them into bite-sized chunks which are chewed on in a low- priority
 * idle thread.  This greatly improves responsiveness for many GIMP
 * operations.  -- Adam
 */
static gboolean
gimp_display_idlerender_callback (gpointer data)
{
  const gint   CHUNK_WIDTH  = 256;
  const gint   CHUNK_HEIGHT = 128;
  gint         workx, worky, workw, workh;
  GimpDisplay *gdisp = data;

  workw = CHUNK_WIDTH;
  workh = CHUNK_HEIGHT;
  workx = gdisp->idle_render.x;
  worky = gdisp->idle_render.y;

  if (workx + workw > gdisp->idle_render.basex + gdisp->idle_render.width)
    {
      workw = gdisp->idle_render.basex + gdisp->idle_render.width - workx;
    }

  if (worky + workh > gdisp->idle_render.basey + gdisp->idle_render.height)
    {
      workh = gdisp->idle_render.basey + gdisp->idle_render.height - worky;
    }  

  gimp_display_paint_area (gdisp, workx, worky, workw, workh);

  gdisp->idle_render.x += CHUNK_WIDTH;

  if (gdisp->idle_render.x >=
      gdisp->idle_render.basex + gdisp->idle_render.width)
    {
      gdisp->idle_render.x = gdisp->idle_render.basex;
      gdisp->idle_render.y += CHUNK_HEIGHT;

      if (gdisp->idle_render.y >=
	  gdisp->idle_render.basey + gdisp->idle_render.height)
	{
	  if (! gimp_display_idle_render_next_area (gdisp))
	    {
	      /* FINISHED */
              gdisp->idle_render.idle_id = 0;

	      return FALSE;
	    }
	}
    }

  /* Still work to do. */
  return TRUE;
}

static gboolean
gimp_display_idle_render_next_area (GimpDisplay *gdisp)
{
  GimpArea *area;

  if (! gdisp->idle_render.update_areas)
    return FALSE;

  area = (GimpArea *) gdisp->idle_render.update_areas->data;

  gdisp->idle_render.update_areas =
    g_slist_remove (gdisp->idle_render.update_areas, area);

  gdisp->idle_render.x      = gdisp->idle_render.basex = area->x1;
  gdisp->idle_render.y      = gdisp->idle_render.basey = area->y1;
  gdisp->idle_render.width  = area->x2 - area->x1;
  gdisp->idle_render.height = area->y2 - area->y1;

  g_free (area);

  return TRUE;
}

static void
gimp_display_paint_area (GimpDisplay *gdisp,
                         gint         x,
                         gint         y,
                         gint         w,
                         gint         h)
{
  GimpDisplayShell *shell;
  gint              x1, y1, x2, y2;

  shell = GIMP_DISPLAY_SHELL (gdisp->shell);

  /*  Bounds check  */
  x1 = CLAMP (x,     0, gdisp->gimage->width);
  y1 = CLAMP (y,     0, gdisp->gimage->height);
  x2 = CLAMP (x + w, 0, gdisp->gimage->width);
  y2 = CLAMP (y + h, 0, gdisp->gimage->height);
  x = x1;
  y = y1;
  w = (x2 - x1);
  h = (y2 - y1);

  /*  calculate the extents of the update as limited by what's visible  */
  gimp_display_shell_untransform_xy (shell,
                                     0, 0,
                                     &x1, &y1,
                                     FALSE, FALSE);
  gimp_display_shell_untransform_xy (shell,
                                     shell->disp_width, shell->disp_height,
                                     &x2, &y2,
                                     FALSE, FALSE);

  gimp_image_invalidate (gdisp->gimage, x, y, w, h, x1, y1, x2, y2);

  /*  display the area  */
  gimp_display_shell_transform_xy (shell, x, y, &x1, &y1, FALSE);
  gimp_display_shell_transform_xy (shell, x + w, y + h, &x2, &y2, FALSE);

  gimp_display_shell_expose_area (shell, x1, y1, (x2 - x1), (y2 - y1));
}
