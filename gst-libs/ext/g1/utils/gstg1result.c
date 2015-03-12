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
#include "gstg1result.h"

const gchar *
gst_g1_result_pp (PPResult r)
{
  const gchar *ret;

  switch (r) {
    case PP_OK:
      ret = "ok";
      break;
    case PP_PARAM_ERROR:
      ret = "parameter error";
      break;
    case PP_MEMFAIL:
      ret = "memory fail";
      break;
    case PP_SET_IN_SIZE_INVALID:
      ret = "invalid input size";
      break;
    case PP_SET_IN_ADDRESS_INVALID:
      ret = "invalid input address";
      break;
    case PP_SET_IN_FORMAT_INVALID:
      ret = "invalid input format";
      break;
    case PP_SET_CROP_INVALID:
      ret = "invalid crop";
      break;
    case PP_SET_ROTATION_INVALID:
      ret = "invalid rotation";
      break;
    case PP_SET_OUT_SIZE_INVALID:
      ret = "invalid output size";
      break;
    case PP_SET_OUT_ADDRESS_INVALID:
      ret = "invalid output address";
      break;
    case PP_SET_OUT_FORMAT_INVALID:
      ret = "invalid output format";
      break;
    case PP_SET_VIDEO_ADJUST_INVALID:
      ret = "invalid video adjust";
      break;
    case PP_SET_RGB_BITMASK_INVALID:
      ret = "invalid RGB bitmask";
      break;
    case PP_SET_FRAMEBUFFER_INVALID:
      ret = "invalid framebuffer";
      break;
    case PP_SET_MASK1_INVALID:
      ret = "invalid mask 1";
      break;
    case PP_SET_MASK2_INVALID:
      ret = "invalid mask 2";
      break;
    case PP_SET_DEINTERLACE_INVALID:
      ret = "invalid deinterlace";
      break;
    case PP_SET_IN_STRUCT_INVALID:
      ret = "invalid input struct";
      break;
    case PP_SET_IN_RANGE_MAP_INVALID:
      ret = "invalid input range map";
      break;
    case PP_SET_ABLEND_UNSUPPORTED:
      ret = "alpha blend unsupported";
      break;
    case PP_SET_DEINTERLACING_UNSUPPORTED:
      ret = "deinterlacing unsupported";
      break;
    case PP_SET_DITHERING_UNSUPPORTED:
      ret = "dithering unsupported";
      break;
    case PP_SET_SCALING_UNSUPPORTED:
      ret = "scaling unsupported";
      break;
    case PP_BUSY:
      ret = "busy";
      break;
    case PP_HW_BUS_ERROR:
      ret = "bus error";
      break;
    case PP_HW_TIMEOUT:
      ret = "hw timeout";
      break;
    case PP_DWL_ERROR:
      ret = "dwl error";
      break;
    case PP_SYSTEM_ERROR:
      ret = "system error";
      break;
    case PP_DEC_COMBINED_MODE_ERROR:
      ret = "combined mode error";
      break;
    case PP_DEC_RUNTIME_ERROR:
      ret = "runtime error";
      break;
    default:
      g_return_val_if_reached ("(Invalid code)");
  }
  return ret;
}

const gchar *
gst_g1_result_h264 (H264DecRet r)
{
  const gchar *ret;

  switch (r) {
    case H264DEC_OK:
      ret = "ok";
      break;
    case H264DEC_STRM_PROCESSED:
      ret = "stream processed";
      break;
    case H264DEC_PIC_RDY:
      ret = "picture available for output";
      break;
    case H264DEC_PIC_DECODED:
      ret = "picture decoded";
      break;
    case H264DEC_HDRS_RDY:
      ret = "headers decoded";
      break;
    case H264DEC_ADVANCED_TOOLS:
      ret = "advanced coding tools detected";
      break;
    case H264DEC_PENDING_FLUSH:
      ret = "output pictures must be retrieved before continuing decode";
      break;
    case H264DEC_NONREF_PIC_SKIPPED:
      ret = "skipped non-reference picture";
      break;
    case H264DEC_END_OF_STREAM:
      ret = "end of stream";
      break;
    case H264DEC_PARAM_ERROR:
      ret = "parameter error";
      break;
    case H264DEC_STRM_ERROR:
      ret = "stream error";
      break;
    case H264DEC_NOT_INITIALIZED:
      ret = "not initialized";
      break;
    case H264DEC_MEMFAIL:
      ret = "memory fail";
      break;
    case H264DEC_INITFAIL:
      ret = "init fail";
      break;
    case H264DEC_HDRS_NOT_RDY:
      ret = "headers not ready";
      break;
    case H264DEC_STREAM_NOT_SUPPORTED:
      ret = "stream not supported";
      break;
    case H264DEC_HW_RESERVED:
      ret = "hardware reserved";
      break;
    case H264DEC_HW_TIMEOUT:
      ret = "hardware timeout";
      break;
    case H264DEC_HW_BUS_ERROR:
      ret = "hardware bus error";
      break;
    case H264DEC_SYSTEM_ERROR:
      ret = "system error";
      break;
    case H264DEC_DWL_ERROR:
      ret = "dwl error";
      break;
    case H264DEC_EVALUATION_LIMIT_EXCEEDED:
      ret = "evaluation limit exceeded";
      break;
    case H264DEC_FORMAT_NOT_SUPPORTED:
      ret = "format not supported";
      break;
    default:
      g_return_val_if_reached ("(Invalid code)");
  }
  return ret;
}
