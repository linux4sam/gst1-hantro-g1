/* GStreamer G1 plugin
 *
 * Copyright (C) 2014-2015  Atmel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __GST_G1_BASE_DEC_H__
#define __GST_G1_BASE_DEC_H__

#include <gst/gst.h>
#include <gst/video/gstvideodecoder.h>
#include "gstdwlallocator.h"

#include <ppapi.h>

G_BEGIN_DECLS

#define GST_TYPE_G1_BASE_DEC \
  (gst_g1_base_dec_get_type())
#define GST_G1_BASE_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_G1_BASE_DEC,GstG1BaseDec))
#define GST_G1_BASE_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_G1_BASE_DEC,GstG1BaseDecClass))
#define GST_IS_G1_BASE_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_G1_BASE_DEC))
#define GST_IS_G1_BASE_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_G1_BASE_DEC))

typedef struct _GstG1BaseDec GstG1BaseDec;
typedef struct _GstG1BaseDecClass GstG1BaseDecClass;

struct _GstG1BaseDec {
  GstVideoDecoder parent;

  gpointer codec;
  guint32 dectype;
  PPInst pp;
  PPConfig ppconfig;

  gint rotation;

  gint brightness;
  gint contrast;
  gint saturation;

  guint crop_x;
  guint crop_y;
  guint crop_width;
  guint crop_height;

  guint mask1_x;
  guint mask1_y;
  guint mask1_width;
  guint mask1_height;
  gchar *mask1_location;
  gboolean use_drm;
  GstG1Memory *mask1_mem;
  
  /* TODO: move to a private */
  GstAllocator *allocator;
};

struct _GstG1BaseDecClass {
  GstVideoDecoderClass parent_class;

  gboolean (*open) (GstG1BaseDec *dec);
  gboolean (*close) (GstG1BaseDec *dec);
  GstFlowReturn (*decode) (GstG1BaseDec *dec, GstVideoCodecFrame *frame);
};

GType gst_g1_base_dec_get_type(void);

void gst_g1_base_dec_config_format (GstG1BaseDec * dec, GstVideoFormatInfo *fmt, 
    gint32 width, gint32 height);
GstFlowReturn gst_g1_base_dec_allocate_output (GstG1BaseDec * dec, 
    GstVideoCodecFrame * frame);
GstFlowReturn gst_g1_base_dec_push_data (GstG1BaseDec *dec, GstVideoCodecFrame *frame);

G_END_DECLS

#endif /*__GST_G1_BASE_DEC_H__*/
