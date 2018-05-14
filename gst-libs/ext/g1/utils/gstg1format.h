/* GStreamer G1 plugin
 *
 * Copyright (C) 2014-2015  Atmel Corporation.
 *                    2017 Microchip Technology Inc.
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
#ifndef __GST_G1_FORMAT_H__
#define __GST_G1_FORMAT_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <g1decoder/h264decapi.h>
#include <g1decoder/mp4decapi.h>
#include <g1decoder/ppapi.h>

G_BEGIN_DECLS

GstVideoFormatInfo gst_g1_format_h264_to_gst (H264DecOutFormat fmt);

GstVideoFormatInfo gst_g1_format_mp4_to_gst (MP4DecOutFormat fmt);

guint32 gst_g1_format_gst_to_pp (GstVideoFormatInfo * finfo);

G_END_DECLS
#endif //__GST_G1_FORMAT_H__
