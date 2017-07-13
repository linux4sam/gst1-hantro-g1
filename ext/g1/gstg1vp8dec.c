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
/**
 * SECTION:element-g1vp8dec
 *
 * Hantro G1 HW accelerated VP8 decoder
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 filesrc location=<File.webm>  ! matroskaparse ! queue ! 
 *                  g1vp8dec ! drmsink full-screen=true 
 * ]| 
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif
#include "gstg1vp8dec.h"
#include "gstg1allocator.h"
#include "gstg1format.h"
#include "gstg1result.h"
#include "fifo.h"

#include <vp8decapi.h>
#include <dwl.h>

enum
{
  PROP_0,
  PROP_ERROR_CONCEALMENT,
  PROP_NUM_FRAMEBUFFER,
};

#define PROP_DEFAULT_ERROR_CONCEALMENT      FALSE
#define PROP_DEFAULT_NUM_FRAMEBUFFER        6

static GstStaticPadTemplate gst_g1_vp8_dec_sink_pad_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-vp8"));

GST_DEBUG_CATEGORY_STATIC (g1_vp8_dec_debug);
#define GST_CAT_DEFAULT g1_vp8_dec_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_PERFORMANCE);

#define gst_g1_vp8_dec_parent_class parent_class
G_DEFINE_TYPE (GstG1VP8Dec, gst_g1_vp8_dec, GST_TYPE_G1_BASE_DEC);

#define GST_G1_VP8_FAILED(ret) (VP8DEC_OK != (ret))

static void gst_g1_vp8_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_g1_vp8_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static gboolean gst_g1_vp8_dec_open (GstG1BaseDec * dec);
static gboolean gst_g1_vp8_dec_close (GstG1BaseDec * dec);
static GstFlowReturn gst_g1_vp8_dec_decode_headers (GstG1BaseDec * g1dec,
    GstBuffer * streamheader);
static GstFlowReturn gst_g1_vp8_dec_decode (GstG1BaseDec * decoder,
    GstVideoCodecFrame * frame);

static void gst_g1_vp8_dec_dwl_to_vp8 (GstG1VP8Dec * dec,
    DWLLinearMem_t * linearmem, VP8DecInput * input, gsize size);

static void
gst_g1_vp8_dec_class_init (GstG1VP8DecClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstG1BaseDecClass *g1dec_class;

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;
  g1dec_class = (GstG1BaseDecClass *) klass;

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_g1_vp8_dec_sink_pad_template));

  GST_DEBUG_CATEGORY_INIT (g1_vp8_dec_debug, "g1vp8dec", 0,
      "Hantro G1 VP8 decoder");
  GST_DEBUG_CATEGORY_GET (GST_CAT_PERFORMANCE, "GST_PERFORMANCE");

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->set_property = gst_g1_vp8_dec_set_property;
  gobject_class->get_property = gst_g1_vp8_dec_get_property;

  g_object_class_install_property (gobject_class, PROP_ERROR_CONCEALMENT,
      g_param_spec_boolean ("video-freeze-concealment",
          "Video Freeze concealment", "When set to non-zero value the decoder"
          " will conceal every frame after an error has been detected in the"
          "bitstream, until the next key frame is decoded. When set to zero,"
          " decoder will conceal only the frames having errors in bitstream",
          PROP_DEFAULT_ERROR_CONCEALMENT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_NUM_FRAMEBUFFER,
      g_param_spec_uint ("numFrameBuffers", "numFrameBuffers",
          "Number of frame buffers the decoder should allocate. Maximum value"
          " is 16, minimum value is 2 or 3depending on the stream contents. "
          "Extra buffers allow for application-specific post processing by "
          "guaranteeing that the output frame is not immediately overwritten "
          "by the next decoded frame",
          2, 16, PROP_DEFAULT_NUM_FRAMEBUFFER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g1dec_class->open = GST_DEBUG_FUNCPTR (gst_g1_vp8_dec_open);
  g1dec_class->close = GST_DEBUG_FUNCPTR (gst_g1_vp8_dec_close);
  g1dec_class->decode = GST_DEBUG_FUNCPTR (gst_g1_vp8_dec_decode);
  g1dec_class->decode_header =
      GST_DEBUG_FUNCPTR (gst_g1_vp8_dec_decode_headers);

  gst_element_class_set_static_metadata (element_class,
      "Hantro G1 VP8 decoder", "Codec/Decoder/Video", "Decode an VP8 stream",
      "Sandeep Sheriker <sandeepsheriker.mallikarjun@microchip.com>");
}

static void
gst_g1_vp8_dec_init (GstG1VP8Dec * dec)
{
  GstG1BaseDec *g1dec = GST_G1_BASE_DEC (dec);

  GST_LOG_OBJECT (dec, "initializing");
  g1dec->dectype = PP_PIPELINED_DEC_TYPE_VP8;
  dec->error_concealment = PROP_DEFAULT_ERROR_CONCEALMENT;
  dec->numFrameBuffers = PROP_DEFAULT_NUM_FRAMEBUFFER;
  dec->picDecodeNumber = 0;
}

static gboolean
gst_g1_vp8_dec_open (GstG1BaseDec * g1dec)
{
  GstG1VP8Dec *dec = GST_G1_VP8_DEC (g1dec);
  VP8DecRet decret;
  gboolean ret;

  GST_LOG_OBJECT (dec, "opening VP8 decoder");

  decret = VP8DecInit ((VP8DecInst *) & g1dec->codec, VP8DEC_VP8,
      dec->error_concealment, dec->numFrameBuffers, DEC_REF_FRM_RASTER_SCAN);
  if (GST_G1_VP8_FAILED (decret)) {
    GST_ERROR_OBJECT (dec, gst_g1_result_vp8 (decret));
    ret = FALSE;
    goto exit;
  }

  GST_LOG_OBJECT (dec, "VP8DecInit: VP8 decoder successfully opened");

  ret = TRUE;
exit:
  return ret;
}

static void
gst_g1_vp8_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstG1VP8Dec *dec = GST_G1_VP8_DEC (object);
  switch (prop_id) {
    case PROP_ERROR_CONCEALMENT:
      dec->error_concealment = g_value_get_boolean (value);
      break;
    case PROP_NUM_FRAMEBUFFER:
      dec->numFrameBuffers = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_g1_vp8_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstG1VP8Dec *dec = GST_G1_VP8_DEC (object);

  switch (prop_id) {
    case PROP_ERROR_CONCEALMENT:
      g_value_set_boolean (value, dec->error_concealment);
      break;
    case PROP_NUM_FRAMEBUFFER:
      g_value_set_int (value, dec->numFrameBuffers);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstFlowReturn
gst_g1_vp8_dec_parse_header (GstG1VP8Dec * dec)
{
  GstVideoDecoder *bdec = GST_VIDEO_DECODER (dec);
  GstG1BaseDec *g1dec = GST_G1_BASE_DEC (dec);
  GstFlowReturn ret;
  GstVideoFormatInfo finfoi;
  GstVideoCodecState *state;
  VP8DecInfo header;
  VP8DecRet decret;

  g_return_val_if_fail (dec, GST_FLOW_ERROR);

  decret = VP8DecGetInfo (g1dec->codec, &header);
  if (GST_G1_VP8_FAILED (decret)) {
    GST_ERROR_OBJECT (g1dec, "VP8DecGetInfo failed: %s \tError Value: %d \n",
        gst_g1_result_vp8 (decret), decret);
    ret = GST_FLOW_ERROR;
    goto exit;
  }

  GST_LOG_OBJECT (dec, "VP8DecGetInfo: "
      "header.vpVersion = %d\n"
      "header.vpProfile= %d\n"
      "header.codedWidth= %d\n"
      "header.codedHeight= %d\n"
      "header.frameWidth= %d\n"
      "header.frameHeight= %d\n"
      "header.scaledWidth= %d\n"
      "header.scaledHeight= %d\n"
      "header.dpbMode= %d\n"
      "header.outputFormat= %d\n",
      header.vpVersion,
      header.vpProfile,
      header.codedWidth,
      header.codedHeight,
      header.frameWidth,
      header.frameHeight,
      header.scaledWidth,
      header.scaledHeight, header.dpbMode, header.outputFormat);

  state = gst_video_decoder_get_output_state (bdec);
  if (state) {
    /* A 1 on either field means that it was a range at the time of fixating
     * caps. Likely the user didn't specify them. Use input size */
    if (1 == state->info.width || 1 == state->info.height) {
      state->info.width = header.frameWidth;
      state->info.height = header.frameHeight;
    }
  }

  finfoi = gst_g1_format_mp4_to_gst (header.outputFormat);
  gst_g1_base_dec_config_format (g1dec, &finfoi,
      header.frameWidth, header.frameHeight);

  ret = GST_FLOW_OK;

exit:
  return ret;
}

static void
gst_g1_vp8_dec_dwl_to_vp8 (GstG1VP8Dec * dec, DWLLinearMem_t * linearmem,
    VP8DecInput * vp8input, gsize size)
{
  g_return_if_fail (dec);
  g_return_if_fail (vp8input);
  g_return_if_fail (linearmem);

  vp8input->pStream = ((guint8 *) linearmem->virtualAddress);
  vp8input->dataLen = size;
  vp8input->streamBusAddress = linearmem->busAddress;

  vp8input->sliceHeight = 0;
  vp8input->pPicBufferY = NULL;
  vp8input->picBufferBusAddressY = 0;
  vp8input->pPicBufferC = NULL;
  vp8input->picBufferBusAddressC = 0;
}

static GstFlowReturn
gst_g1_vp8_dec_pop_picture (GstG1VP8Dec * dec, GstVideoCodecFrame * frame)
{
  GstG1BaseDec *bdec;
  VP8DecPicture picture;
  VP8DecRet decret;

  bdec = GST_G1_BASE_DEC (dec);

  do {
    decret = VP8DecNextPicture (bdec->codec, &picture, FALSE);
    if (decret != VP8DEC_PIC_RDY) {
      GST_ERROR_OBJECT (dec, "%s (%d) (%p|0x%08x)",
          gst_g1_result_vp8 (decret), decret, picture.pOutputFrame,
          picture.outputFrameBusAddress);
      break;
    }
    /* TODO: do some error checking here */

    if (picture.nbrOfErrMBs)
      GST_LOG_OBJECT (dec, "concealed %d macroblocks", picture.nbrOfErrMBs);

    gst_g1_base_dec_push_data (bdec, frame);

  } while (decret == VP8DEC_PIC_RDY);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_g1_vp8_dec_decode_headers (GstG1BaseDec * g1dec, GstBuffer * streamheader)
{
  GstG1VP8Dec *dec = GST_G1_VP8_DEC (g1dec);
  VP8DecInput vp8input;
  VP8DecOutput vp8output;
  GstMapInfo minfo;
  VP8DecRet decret;
  GstFlowReturn ret = GST_FLOW_OK;
  DWLLinearMem_t linearmem;

  gst_buffer_map (streamheader, &minfo, GST_MAP_READ);
  linearmem.virtualAddress = (guint32 *) minfo.data;
  linearmem.busAddress = gst_g1_allocator_get_physical (minfo.memory);
  linearmem.size = minfo.size;
  gst_buffer_unmap (streamheader, &minfo);

  gst_g1_vp8_dec_dwl_to_vp8 (dec, &linearmem, &vp8input, minfo.size);

  gst_util_dump_mem (vp8input.pStream, minfo.size);

  decret = VP8DecDecode (g1dec->codec, &vp8input, &vp8output);
  switch (decret) {
    case VP8DEC_HDRS_RDY:
      /* read stream info */
      ret = gst_g1_vp8_dec_parse_header (dec);
      GST_LOG_OBJECT (dec, "handle VP8DEC_DP_HDRS_RDY");
      break;
    default:
      GST_ERROR_OBJECT (dec, "Unhandled return code: %s (%d)",
          gst_g1_result_vp8 (decret), decret);
      g_return_val_if_reached (GST_FLOW_OK);
  }
  return ret;
}

static GstFlowReturn
gst_g1_vp8_dec_decode (GstG1BaseDec * g1dec, GstVideoCodecFrame * frame)
{
  GstG1VP8Dec *dec = GST_G1_VP8_DEC (g1dec);
  VP8DecInput vp8input;
  VP8DecOutput vp8output;
  GstMapInfo minfo;
  VP8DecRet decret;
  GstFlowReturn ret = GST_FLOW_ERROR;
  DWLLinearMem_t linearmem;
  gboolean error;

  gst_buffer_map (frame->input_buffer, &minfo, GST_MAP_READ);
  linearmem.virtualAddress = (guint32 *) minfo.data;
  linearmem.busAddress = gst_g1_allocator_get_physical (minfo.memory);
  linearmem.size = minfo.size;
  gst_buffer_unmap (frame->input_buffer, &minfo);

  error = FALSE;

  gst_g1_vp8_dec_dwl_to_vp8 (dec, &linearmem, &vp8input, minfo.size);

  do {
    ret = gst_g1_base_dec_allocate_output (g1dec, frame);
    if (GST_FLOW_OK != ret)
      break;

    decret = VP8DecDecode (g1dec->codec, &vp8input, &vp8output);
    switch (decret) {
      case VP8DEC_SLICE_RDY:
        GST_LOG_OBJECT (dec, "VP8DEC_SLICE_RDY");
        ret = gst_g1_vp8_dec_pop_picture (dec, frame);
        break;
      case VP8DEC_HDRS_RDY:
        /* read stream info */
        ret = gst_g1_vp8_dec_parse_header (dec);
        GST_LOG_OBJECT (dec, "handle VP8DEC_HDRS_RDY");
        break;
        /* a picture was decoded */
      case VP8DEC_PIC_DECODED:
        GST_LOG_OBJECT (dec, "VP8DEC_PIC_DECODED");
        dec->picDecodeNumber++;
        ret = gst_g1_vp8_dec_pop_picture (dec, frame);
        break;
      case VP8DEC_NOT_INITIALIZED:
        GST_ERROR_OBJECT (dec, "VP8DEC_NOT_INITIALIZED");
        error = TRUE;
        break;
      case VP8DEC_STRM_ERROR:
        GST_VIDEO_DECODER_ERROR (dec, 0, STREAM, DECODE,
            ("stream error"), (gst_g1_result_vp8 (decret)), ret);
        error = TRUE;
        break;
      case VP8DEC_HW_TIMEOUT:
      case VP8DEC_HW_BUS_ERROR:
      case VP8DEC_SYSTEM_ERROR:
      case VP8DEC_DWL_ERROR:
        GST_ELEMENT_ERROR (dec, RESOURCE, FAILED,
            ("G1 system error"), (gst_g1_result_vp8 (decret)));
        ret = GST_FLOW_ERROR;
        error = TRUE;
        break;
      default:
        GST_ERROR_OBJECT (dec, "Unhandled return code: %s (%d)",
            gst_g1_result_vp8 (decret), decret);
        g_return_val_if_reached (GST_FLOW_OK);
    }

    if (error)
      break;

  } while ((decret != VP8DEC_PIC_DECODED));

  return ret;
}

static gboolean
gst_g1_vp8_dec_close (GstG1BaseDec * g1dec)
{
  GstG1VP8Dec *dec = GST_G1_VP8_DEC (g1dec);
  GST_LOG_OBJECT (dec, "closing VP8 decoder");
  VP8DecRelease (g1dec->codec);
  return TRUE;
}
