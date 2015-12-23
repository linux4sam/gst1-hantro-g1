/* GStreamer drmsink plugin
 * Copyright (C) 2013 Harm Hanemaaijer <fgenfb@yahoo.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_DRMSINK_H_
#define _GST_DRMSINK_H_

#include <stdint.h>
#include <linux/fb.h>
#include "gstframebuffersink.h"

G_BEGIN_DECLS

/* Main class. */

#define GST_TYPE_DRMSINK (gst_drmsink_get_type ())
#define GST_DRMSINK(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
    GST_TYPE_DRMSINK, GstDrmsink))
#define GST_DRMSINK_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
    GST_TYPE_DRMSINK,GstDrmsinkClass))
#define GST_IS_DRMSINK(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
    GST_TYPE_DRMSINK))
#define GST_IS_DRMSINK_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
    GST_TYPE_DRMSINK))

typedef struct _GstDrmsink GstDrmsink;
typedef struct _GstDrmsinkClass GstDrmsinkClass;

struct _GstDrmsink
{
  GstFramebufferSink framebuffersink;

  /* DRM */
  gint fd;
  uint32_t connector_id;
  int32_t crtc_id;
  drmModeRes *resources;
  drmModePlaneRes *plane_resources;
  drmModePlane *plane;
  drmModeModeInfo mode; 
  drmEventContext *event_context;
  drmModeCrtc *saved_crtc;
  gboolean crtc_mode_initialized;
  gboolean set_plane_initialized;
  gboolean vblank_occurred;
  gboolean page_flip_pending;
  gboolean page_flip_occurred;
  int cx;
  int cy;
  int cw;
  int ch;
  gboolean zero_memcpy;
  gboolean lcd;

  /* GST */
  GstVideoRectangle screen_rect;

  /* Properties */
  gint preferred_connector_id;
};

struct _GstDrmsinkClass
{
  GstFramebufferSinkClass framebuffersink_parent_class;
};

GType gst_drmsink_get_type (void);

G_END_DECLS

#endif
