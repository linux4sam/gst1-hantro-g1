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
#ifndef __GST_G1_RESULT_H__
#define __GST_G1_RESULT_H__

#include <gst/gst.h>
#include <ppapi.h>
#include <h264decapi.h>
#include <mp4decapi.h>
#include <vp8decapi.h>

G_BEGIN_DECLS
/**
 * Returns a string representation for a given PPResult.
 *
 * \r The PPResult to print out
 *
 * \return A constant string description. Do not free!
 */
const gchar *gst_g1_result_pp (PPResult r);

/**
 * Returns a string representation for a given H264DecRet
 *
 * \r The H264DecRet to print out
 *
 * \return A constant string description. Do not free!
 */
const gchar *gst_g1_result_h264 (H264DecRet r);

/**
 * Returns a string representation for a given MP4DecRet
 *
 * \r The MP4DecRet to print out
 *
 * \return A constant string description. Do not free!
 */
const gchar *gst_g1_result_mp4 (MP4DecRet r);

/**
 * Returns a string representation for a given VP8DecRet
 *
 * \r The VP8DecRet to print out
 *
 * \return A constant string description. Do not free!
 */

const gchar *gst_g1_result_vp8 (VP8DecRet r);

G_END_DECLS
#endif //__GST_G1_RESULT_H__
