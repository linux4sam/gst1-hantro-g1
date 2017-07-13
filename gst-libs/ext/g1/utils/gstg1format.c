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
#include "gstg1format.h"

GstVideoFormatInfo
gst_g1_format_mp4_to_gst (MP4DecOutFormat fmt)
{
  GstVideoFormatInfo finfo = (const GstVideoFormatInfo) { 0 };

  switch (fmt) {
    case MP4DEC_SEMIPLANAR_YUV420:
      finfo.name = "NV12";
      finfo.description = "raster semiplanar 4:2:0 YUV";
      finfo.format = GST_VIDEO_FORMAT_NV12;
      finfo.flags |= GST_VIDEO_FORMAT_FLAG_YUV;
      break;
    case MP4DEC_TILED_YUV420:
      finfo.name = "NV12";
      finfo.description = "tiled semiplanar 4:2:0 YUV";
      finfo.format = GST_VIDEO_FORMAT_NV12;
      finfo.flags |= GST_VIDEO_FORMAT_FLAG_YUV;
      /* This version of gstreamer doesn't have support for tiles yet */
      //finfo.flags |= GST_VIDEO_FORMAT_FLAG_TILED;
      g_return_val_if_reached (finfo);
      break;
    default:
    g_return_val_if_reached ((const GstVideoFormatInfo) {
        0});
  }

  return finfo;
}


GstVideoFormatInfo
gst_g1_format_h264_to_gst (H264DecOutFormat fmt)
{
  GstVideoFormatInfo finfo = (const GstVideoFormatInfo) { 0 };

  switch (fmt) {
    case H264DEC_SEMIPLANAR_YUV420:
      finfo.name = "NV12";
      finfo.description = "raster semiplanar 4:2:0 YUV";
      finfo.format = GST_VIDEO_FORMAT_NV12;
      finfo.flags |= GST_VIDEO_FORMAT_FLAG_YUV;
      break;
    case H264DEC_TILED_YUV420:
      finfo.name = "NV12";
      finfo.description = "tiled semiplanar 4:2:0 YUV";
      finfo.format = GST_VIDEO_FORMAT_NV12;
      finfo.flags |= GST_VIDEO_FORMAT_FLAG_YUV;
      /* This version of gstreamer doesn't have support for tiles yet */
      //finfo.flags |= GST_VIDEO_FORMAT_FLAG_TILED;
      g_return_val_if_reached (finfo);
      break;
    case H264DEC_YUV400:
      finfo.name = "GRAY8";
      finfo.description = "8-bit monochrome";
      finfo.format = GST_VIDEO_FORMAT_GRAY8;
      finfo.flags |= GST_VIDEO_FORMAT_FLAG_GRAY;
      break;
    default:
    g_return_val_if_reached ((const GstVideoFormatInfo) {
        0});
  }

  return finfo;
}

guint32
gst_g1_format_gst_to_pp (GstVideoFormatInfo * finfo)
{
  guint32 ret;
  GstVideoFormat fmt;

  fmt = GST_VIDEO_FORMAT_INFO_FORMAT (finfo);

  switch (fmt) {
    case GST_VIDEO_FORMAT_GRAY8:
      ret = PP_PIX_FMT_YCBCR_4_0_0;
      break;
    case GST_VIDEO_FORMAT_YUY2:
      ret = PP_PIX_FMT_YCBCR_4_2_2_INTERLEAVED;
      break;
    case GST_VIDEO_FORMAT_YVYU:
      ret = PP_PIX_FMT_YCRYCB_4_2_2_INTERLEAVED;
      break;
    case GST_VIDEO_FORMAT_UYVY:
      ret = PP_PIX_FMT_CBYCRY_4_2_2_INTERLEAVED;
      break;
//    case GST_VIDEO_FORMAT_XXXX:
//      ret = PP_PIX_FMT_CRYCBY_4_2_2_INTERLEAVED;
//      break;
    case GST_VIDEO_FORMAT_NV16:
      ret = PP_PIX_FMT_YCBCR_4_2_2_SEMIPLANAR;
      break;
//    case GST_VIDEO_FORMAT_XXXX:
//      ret = PP_PIX_FMT_YCBCR_4_2_2_TILED_4X4;
//      break;
//    case GST_VIDEO_FORMAT_XXXX:
//      ret = PP_PIX_FMT_YCRYCB_4_2_2_TILED_4X4;
//      break;
//    case GST_VIDEO_FORMAT_XXXX:
//      ret = PP_PIX_FMT_CBYCRY_4_2_2_TILED_4X4;
//      break;
//    case GST_VIDEO_FORMAT_XXXX:
//      ret = PP_PIX_FMT_CRYCBY_4_2_2_TILED_4X4;
//      break;
//    case GST_VIDEO_FORMAT_XXXX:
//      ret = PP_PIX_FMT_YCBCR_4_4_0;
//      break;
    case GST_VIDEO_FORMAT_I420:
      ret = PP_PIX_FMT_YCBCR_4_2_0_PLANAR;
      break;
    case GST_VIDEO_FORMAT_NV12:
      ret = PP_PIX_FMT_YCBCR_4_2_0_SEMIPLANAR;
      break;
//    case GST_VIDEO_FORMAT_XXXX:
//      ret = PP_PIX_FMT_YCBCR_4_2_0_TILED;
//      break;
//    case GST_VIDEO_FORMAT_XXXX:
//      ret = PP_PIX_FMT_YCBCR_4_1_1_SEMIPLANAR;
//      break;
//    case GST_VIDEO_FORMAT_XXXX:
//      ret = PP_PIX_FMT_YCBCR_4_4_4_SEMIPLANAR;
//      break;
//    case GST_VIDEO_FORMAT_XXXX:
//      ret = PP_PIX_FMT_RGB16_CUSTOM;
//      break;
    case GST_VIDEO_FORMAT_RGB15:
      ret = PP_PIX_FMT_RGB16_5_5_5;
      break;
    case GST_VIDEO_FORMAT_RGB16:
      ret = PP_PIX_FMT_RGB16_5_6_5;
      break;
    case GST_VIDEO_FORMAT_BGR15:
      ret = PP_PIX_FMT_BGR16_5_5_5;
      break;
    case GST_VIDEO_FORMAT_BGR16:
      ret = PP_PIX_FMT_BGR16_5_6_5;
      break;
//    case GST_VIDEO_FORMAT_XXXX:
//      ret = PP_PIX_FMT_RGB32_CUSTOM;
//      break;
    case GST_VIDEO_FORMAT_BGRx:
      ret = PP_PIX_FMT_RGB32;   //Inverted on purpose
      break;
    case GST_VIDEO_FORMAT_RGBx:
      ret = PP_PIX_FMT_BGR32;   //Inverted on purpose
      break;
    default:
      g_return_val_if_reached (-1);
  }

  return ret;
}
