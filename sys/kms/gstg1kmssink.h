/* GStreamer
 *
 * Copyright (C) 2016 Igalia
 * Copyright (C) Microchip Technology Inc.
 *
 * Authors:
 *  Víctor Manuel Jáquez Leal <vjaquez@igalia.com>
 *  Javier Martin <javiermartin@by.com.es>
 *  Sandeep Sheriker M <sandeepsheriker.mallikarjun@microchip.com>
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
 *
 */

/**
 * g1kmsink is modified for Microchip(Atmel AT91) SAMA5D4 to implement
 * zerocopy and render video frames directly on drm/kms plane.
 * device.
 */

#ifndef __GST_G1KMS_SINK_H__
#define __GST_G1KMS_SINK_H__

#include <gst/video/gstvideosink.h>

G_BEGIN_DECLS
#define GST_TYPE_G1KMS_SINK \
  (gst_g1kms_sink_get_type())
#define GST_G1KMS_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_G1KMS_SINK, GstG1KMSSink))
#define GST_G1KMS_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_G1KMS_SINK, GstG1KMSSinkClass))
#define GST_IS_G1KMS_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_G1KMS_SINK))
#define GST_IS_G1KMS_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_G1KMS_SINK))
typedef struct _GstG1KMSSink GstG1KMSSink;
typedef struct _GstG1KMSSinkClass GstG1KMSSinkClass;

struct _GstG1KMSSink
{
  GstVideoSink videosink;

  /*< private > */
  gint fd;
  gint conn_id;
  gint crtc_id;
  gint plane_id;
  guint pipe;

  /* crtc data */
  guint16 hdisplay, vdisplay;
  guint32 buffer_id;

  /* capabilities */
  gboolean has_prime_import;
  gboolean has_async_page_flip;
  gboolean can_scale;

  gboolean modesetting_enabled;

  GstVideoInfo vinfo;
  GstCaps *allowed_caps;
  GstBufferPool *pool;
  GstAllocator *allocator;
  GstBuffer *last_buffer;
  GstMemory *tmp_kmsmem;

  gchar *devname;

  guint32 mm_width, mm_height;

  GstPoll *poll;
  GstPollFD pollfd;
};

struct _GstG1KMSSinkClass
{
  GstVideoSinkClass parent_class;
};

GType
gst_g1kms_sink_get_type (void)
    G_GNUC_CONST;

G_END_DECLS
#endif /* __GST_G1KMS_SINK_H__ */
