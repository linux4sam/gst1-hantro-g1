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
#ifndef __GST_G1_H264_DEC_H__
#define __GST_G1_H264_DEC_H__

#include <gst/gst.h>
#include "gstg1basedec.h"
#include "h264decapi.h"

G_BEGIN_DECLS
#define GST_TYPE_G1_H264_DEC \
  (gst_g1_h264_dec_get_type())
#define GST_G1_H264_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_G1_H264_DEC,GstG1H264Dec))
#define GST_G1_H264_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_G1_H264_DEC,GstG1H264DecClass))
#define GST_IS_G1_H264_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_G1_H264_DEC))
#define GST_IS_G1_H264_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_G1_H264_DEC))
typedef struct _GstG1H264Dec GstG1H264Dec;
typedef struct _GstG1H264DecClass GstG1H264DecClass;

struct _GstG1H264Dec
{
  GstG1BaseDec parent;

  gboolean skip_non_reference;
  gboolean disable_output_reordering;
  gboolean intra_freeze_concealment;
  gboolean use_display_smoothing;
};

struct _GstG1H264DecClass
{
  GstG1BaseDecClass parent_class;
};

GType gst_g1_h264_dec_get_type (void);

G_END_DECLS
#endif /*__GST_G1_H264_DEC_H__*/
