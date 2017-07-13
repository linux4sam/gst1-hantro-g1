/* GStreamer G1 plugin
 *
 * Copyright (C) 2017 Microchip Technology Inc.
 *              Sandeep Sheriker M <sandeepsheriker.mallikarjun@microchip.com>
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
#ifndef __GST_G1_MP4_DEC_H__
#define __GST_G1_MP4_DEC_H__

#include <gst/gst.h>
#include "gstg1basedec.h"
#include "mp4decapi.h"

G_BEGIN_DECLS
#define GST_TYPE_G1_MP4_DEC     (gst_g1_mp4_dec_get_type())
#define GST_G1_MP4_DEC(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_G1_MP4_DEC, GstG1MP4Dec))
#define GST_G1_MP4_DEC_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_G1_MP4_DEC, GstG1MP4DecClass))
#define GST_IS_G1_MP4_DEC(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_G1_MP4_DEC))
#define GST_IS_G1_MP4_DEC_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_G1_MP4_DEC))
typedef struct _GstG1MP4Dec GstG1MP4Dec;
typedef struct _GstG1MP4DecClass GstG1MP4DecClass;

struct _GstG1MP4Dec
{
  GstG1BaseDec parent;
  gboolean skip_non_reference;
  gboolean error_concealment;
  u32 numFrameBuffers;
  u32 picDecodeNumber;          /* decoded picture ID */
};

struct _GstG1MP4DecClass
{
  GstG1BaseDecClass parent_class;
};

GType gst_g1_mp4_dec_get_type (void);

G_END_DECLS
#endif /*__GST_G1_MP4_DEC_H__*/
