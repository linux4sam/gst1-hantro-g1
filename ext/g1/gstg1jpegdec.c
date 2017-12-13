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
 * SECTION:element-g1jpegdec
 *
 * Hantro G1 HW accelerated JPEG decoder
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 filesrc location=<File.jpg> ! jpegparse ! g1jpegdec \
 * ! imagefreeze ! video/x-raw,format=BGRx,width=<display-width>, \
 * height=<display-height> ! g1kmssink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif
#include "gstg1jpegdec.h"
#include "gstg1allocator.h"
#include "gstg1format.h"
#include "gstg1result.h"
#include "fifo.h"

#include <jpegdecapi.h>
#include <dwl.h>

#include <string.h>
#include <stdio.h>

enum
{
  PROP_0,
  PROP_ERROR_CONCEALMENT,
  PROP_NUM_FRAMEBUFFER,
};

#define PROP_DEFAULT_ERROR_CONCEALMENT      FALSE
#define PROP_DEFAULT_NUM_FRAMEBUFFER        6

static GstStaticPadTemplate gst_g1_jpeg_dec_sink_pad_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/jpeg,"
        "framerate = (fraction) [0/1, MAX],"
        "width = (int) [ 1, 8176 ],"
        "height = (int) [ 1, 8176 ],"
		"parsed = true"));

GST_DEBUG_CATEGORY_STATIC (g1_jpeg_dec_debug);

#define GST_CAT_DEFAULT g1_jpeg_dec_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_PERFORMANCE);

#define gst_g1_jpeg_dec_parent_class parent_class
G_DEFINE_TYPE (GstG1JPEGDec, gst_g1_jpeg_dec, GST_TYPE_G1_BASE_DEC);

#define GST_G1_JPEG_FAILED(ret) (JPEGDEC_OK != (ret))

static void gst_g1_jpeg_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_g1_jpeg_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static gboolean gst_g1_jpeg_dec_open (GstG1BaseDec * dec);
static gboolean gst_g1_jpeg_dec_close (GstG1BaseDec * dec);
static GstFlowReturn gst_g1_jpeg_dec_decode (GstG1BaseDec * decoder,
    GstVideoCodecFrame * frame);
static void gst_g1_jpeg_dec_dwl_to_jpeg (GstG1JPEGDec * dec,
    DWLLinearMem_t * linearmem, JpegDecInput * input, gsize size);

static void
gst_g1_jpeg_dec_class_init (GstG1JPEGDecClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstG1BaseDecClass *g1dec_class;

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;
  g1dec_class = (GstG1BaseDecClass *) klass;

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_g1_jpeg_dec_sink_pad_template));

  GST_DEBUG_CATEGORY_INIT (g1_jpeg_dec_debug, "g1jpegdec", 0,
      "Hantro G1 JPEG decoder");
  GST_DEBUG_CATEGORY_GET (GST_CAT_PERFORMANCE, "GST_PERFORMANCE");

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->set_property = gst_g1_jpeg_dec_set_property;
  gobject_class->get_property = gst_g1_jpeg_dec_get_property;

  g1dec_class->open = GST_DEBUG_FUNCPTR (gst_g1_jpeg_dec_open);
  g1dec_class->close = GST_DEBUG_FUNCPTR (gst_g1_jpeg_dec_close);
  g1dec_class->decode = GST_DEBUG_FUNCPTR (gst_g1_jpeg_dec_decode);

  gst_element_class_set_static_metadata (element_class,
      "Hantro G1 JPEG decoder", "Codec/Decoder/Video", "Decode an JPEG stream",
      "Sandeep Sheriker <sandeepsheriker.mallikarjun@microchip.com>");
}

const gchar *
gst_g1_result_jpeg (JpegDecRet r)
{
  const gchar *ret;

  switch (r) {
    case JPEGDEC_SLICE_READY:
      ret = "Jpeg slice ready";
      break;
    case JPEGDEC_FRAME_READY:
      ret = "Jpeg frameready";
      break;
    case JPEGDEC_STRM_PROCESSED:
      ret = "Jpeg stream processed";
      break;
    case JPEGDEC_SCAN_PROCESSED:
      ret = "Jpeg scan processed";
      break;
    case JPEGDEC_OK:
      ret = "ok";
      break;
    case JPEGDEC_ERROR:
      ret = "Jpeg decode error";
      break;
    case JPEGDEC_UNSUPPORTED:
      ret = "Jpeg decode unsupported";
      break;
    case JPEGDEC_PARAM_ERROR:
      ret = "Jpeg param error";
      break;
    case JPEGDEC_MEMFAIL:
      ret = "Jpeg decode memfail";
      break;
    case JPEGDEC_INITFAIL:
      ret = "Jpeg init fail";
      break;
    case JPEGDEC_INVALID_STREAM_LENGTH:
      ret = "Jpeg decode invalid stream length";
      break;
    case JPEGDEC_STRM_ERROR:
      ret = "Jpeg stream error";
      break;
    case JPEGDEC_INVALID_INPUT_BUFFER_SIZE:
      ret = "Jpeg invalid input buffer size";
      break;
    case JPEGDEC_HW_RESERVED:
      ret = "Jpeg hardware reserved";
      break;
    case JPEGDEC_INCREASE_INPUT_BUFFER:
      ret = "Jpeg increase input buffer";
      break;
    case JPEGDEC_SLICE_MODE_UNSUPPORTED:
      ret = "Jpeg silce mode unsupported";
      break;
    case JPEGDEC_DWL_HW_TIMEOUT:
      ret = "Jpeg dwl hardware timeout";
      break;
    case JPEGDEC_DWL_ERROR:
      ret = "Jpeg dwl error";
      break;
    case JPEGDEC_HW_BUS_ERROR:
      ret = "Jpeg hw bus error";
      break;
    case JPEGDEC_SYSTEM_ERROR:
      ret = "Jpeg system error";
      break;
    case JPEGDEC_FORMAT_NOT_SUPPORTED:
      ret = "Jpeg format not supported";
      break;
    default:
      g_return_val_if_reached ("(Invalid code)");
  }
  return ret;
}

static void
gst_g1_jpeg_dec_init (GstG1JPEGDec * dec)
{
  GstG1BaseDec *g1dec = GST_G1_BASE_DEC (dec);

  GST_LOG_OBJECT (dec, "initializing");
  g1dec->dectype = PP_PIPELINED_DEC_TYPE_JPEG;
}

static gboolean
gst_g1_jpeg_dec_open (GstG1BaseDec * g1dec)
{
  GstG1JPEGDec *dec = GST_G1_JPEG_DEC (g1dec);
  JpegDecRet decret;
  gboolean ret;

  GST_LOG_OBJECT (dec, "opening JPEG decoder");

  decret = JpegDecInit ((JpegDecInst *) & g1dec->codec);
  if (GST_G1_JPEG_FAILED (decret)) {
    GST_ERROR_OBJECT (dec, gst_g1_result_jpeg (decret));
    ret = FALSE;
    goto exit;
  }

  GST_LOG_OBJECT (dec, "JPEGDecInit: JPEG decoder successfully opened");

  ret = TRUE;
exit:
  return ret;
}

static void
gst_g1_jpeg_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
}

static void
gst_g1_jpeg_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
}

static void
gst_g1_jpeg_dec_dwl_to_jpeg (GstG1JPEGDec * dec, DWLLinearMem_t * linearmem,
    JpegDecInput * jpeginput, gsize size)
{
  g_return_if_fail (dec);
  g_return_if_fail (jpeginput);
  g_return_if_fail (linearmem);

  jpeginput->streamBuffer.pVirtualAddress =
      ((guint32 *) linearmem->virtualAddress);
  jpeginput->streamLength = size;
  jpeginput->streamBuffer.busAddress = linearmem->busAddress;
  jpeginput->bufferSize = 0;
  jpeginput->decImageType = 0;
  jpeginput->sliceMbSet = 0;
  jpeginput->pictureBufferY.pVirtualAddress = NULL;
  jpeginput->pictureBufferY.busAddress = 0;
  jpeginput->pictureBufferCbCr.pVirtualAddress = NULL;
  jpeginput->pictureBufferCbCr.busAddress = 0;
  jpeginput->pictureBufferCr.pVirtualAddress = NULL;
  jpeginput->pictureBufferCr.busAddress = 0;

}

static GstFlowReturn
gst_g1_jpeg_dec_decode (GstG1BaseDec * g1dec, GstVideoCodecFrame * frame)
{
  GstG1JPEGDec *dec = GST_G1_JPEG_DEC (g1dec);
  JpegDecInput jpeginput;
  JpegDecOutput jpegoutput;
  GstVideoFormatInfo finfoi;
  JpegDecImageInfo imageInfo;
  GstMapInfo minfo;
  JpegDecRet decret;
  GstFlowReturn ret = GST_FLOW_ERROR;
  DWLLinearMem_t linearmem;
  GstVideoCodecState *state;
  gboolean error;

  gst_buffer_map (frame->input_buffer, &minfo, GST_MAP_READ);
  linearmem.virtualAddress = (guint32 *) minfo.data;
  linearmem.busAddress = gst_g1_allocator_get_physical (minfo.memory);
  linearmem.size = minfo.size;
  gst_buffer_unmap (frame->input_buffer, &minfo);

  error = FALSE;
  GST_LOG_OBJECT (dec, "Size = %d\n", minfo.size);
  //gst_util_dump_mem(minfo.data, minfo.size);

  gst_g1_jpeg_dec_dwl_to_jpeg (dec, &linearmem, &jpeginput, minfo.size);

  decret = JpegDecGetImageInfo (g1dec->codec, &jpeginput, &imageInfo);
  if (GST_G1_JPEG_FAILED (decret)) {
    GST_ERROR_OBJECT (dec, gst_g1_result_jpeg (decret));
    return ret;
  } else {
    GST_LOG_OBJECT (dec, "imageInfo: \n"
        "\t displayWidth = %d \n"
        "\t displayHeight = %d \n"
        "\t outputWidth = %d \n"
        "\t outputHeight = %d \n"
        "\t version = %d \n"
        "\t units = %d \n"
        "\t xDensity = %d \n"
        "\t yDensity = %d \n"
        "\t outputFormat = %d \n"
        "\t codingMode = %d \n"
        "\t thumbnailType = %d \n"
        "\t displayWidthThumb = %d \n"
        "\t displayHeightThumb = %d \n"
        "\t outputWidthThumb = %d \n"
        "\t outputHeightThumb = %d \n"
        "\t outputFormatThumb = %d \n"
        "\t codingModeThumb = %d \n",
        imageInfo.displayWidth,
        imageInfo.displayHeight,
        imageInfo.outputWidth,
        imageInfo.outputHeight,
        imageInfo.version,
        imageInfo.units,
        imageInfo.xDensity,
        imageInfo.yDensity,
        imageInfo.outputFormat,
        imageInfo.codingMode,
        imageInfo.thumbnailType,
        imageInfo.displayWidthThumb,
        imageInfo.displayHeightThumb,
        imageInfo.outputWidthThumb,
        imageInfo.outputHeightThumb,
        imageInfo.outputFormatThumb, imageInfo.codingModeThumb);
    if (imageInfo.thumbnailType == JPEGDEC_THUMBNAIL_JPEG) {
      GST_LOG_OBJECT (dec, "decImageType = JPEGDEC_THUMBNAIL");
      jpeginput.decImageType = JPEGDEC_THUMBNAIL;
    } else if (imageInfo.thumbnailType == JPEGDEC_NO_THUMBNAIL) {
      GST_LOG_OBJECT (dec, "decImageType = JPEGDEC_IMAGE");
      jpeginput.decImageType = JPEGDEC_IMAGE;
    } else if (imageInfo.thumbnailType ==
        JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT) {
      GST_LOG_OBJECT (dec, "decImageType = JPEGDEC_IMAGE");
      jpeginput.decImageType = JPEGDEC_IMAGE;
    }

    state = gst_video_decoder_get_output_state (dec);
    if (state) {
      state->info.par_n = imageInfo.outputWidth;
      state->info.par_d = imageInfo.outputHeight;

      /* A 1 on either field means that it was a range at the time of
         fixating caps. Likely the user didn't specify them. Use input
         size */
      if (1 == state->info.width || 1 == state->info.height) {
        state->info.width = imageInfo.outputWidth;
        state->info.height = imageInfo.outputHeight;
      }
    }

    finfoi = gst_g1_format_mp4_to_gst (MP4DEC_SEMIPLANAR_YUV420);
    gst_g1_base_dec_config_format (g1dec, &finfoi, imageInfo.outputWidth,
        imageInfo.outputHeight);
    GST_LOG_OBJECT (dec, "outputWidth = %d outputHeight = %d\n",
        imageInfo.outputWidth, imageInfo.outputHeight);
  }

  /* reset output */
  jpegoutput.outputPictureY.pVirtualAddress = NULL;
  jpegoutput.outputPictureY.busAddress = 0;
  jpegoutput.outputPictureCbCr.pVirtualAddress = NULL;
  jpegoutput.outputPictureCbCr.busAddress = 0;
  jpegoutput.outputPictureCr.pVirtualAddress = NULL;
  jpegoutput.outputPictureCr.busAddress = 0;

  do {

    ret = gst_g1_base_dec_allocate_output (g1dec, frame);
    if (GST_FLOW_OK != ret)
      break;

    decret = JpegDecDecode (g1dec->codec, &jpeginput, &jpegoutput);
    switch (decret) {
      case JPEGDEC_SLICE_READY:
        GST_LOG_OBJECT (dec, "JPEGDEC_SLICE_READY");
        break;
      case JPEGDEC_FRAME_READY:
        GST_LOG_OBJECT (dec, "JPEGDEC_FRAME_READY");
        gst_g1_base_dec_push_data (g1dec, frame);
        ret = GST_FLOW_OK;
        break;
      case JPEGDEC_STRM_PROCESSED:
        GST_LOG_OBJECT (dec, "JPEGDEC_STRM_PROCESSED");
        break;
      case JPEGDEC_SCAN_PROCESSED:
        GST_LOG_OBJECT (dec, "JPEGDEC_SCAN_PROCESSED");
        break;
      case JPEGDEC_OK:
        GST_LOG_OBJECT (dec, "JPEGDEC_OK");
        break;
      case JPEGDEC_ERROR:
      case JPEGDEC_UNSUPPORTED:
      case JPEGDEC_PARAM_ERROR:
      case JPEGDEC_MEMFAIL:
      case JPEGDEC_INITFAIL:
      case JPEGDEC_INVALID_STREAM_LENGTH:
      case JPEGDEC_STRM_ERROR:
      case JPEGDEC_INVALID_INPUT_BUFFER_SIZE:
      case JPEGDEC_HW_RESERVED:
      case JPEGDEC_INCREASE_INPUT_BUFFER:
      case JPEGDEC_SLICE_MODE_UNSUPPORTED:
        GST_VIDEO_DECODER_ERROR (dec, 0, STREAM, DECODE,
            ("G1 JpegDec error"), (gst_g1_result_jpeg (decret)), ret);
        ret = GST_FLOW_ERROR;
        error = TRUE;
        break;
      case JPEGDEC_DWL_HW_TIMEOUT:
      case JPEGDEC_DWL_ERROR:
      case JPEGDEC_HW_BUS_ERROR:
      case JPEGDEC_SYSTEM_ERROR:
        GST_ELEMENT_ERROR (dec, RESOURCE, FAILED,
            ("G1 stream error"), (gst_g1_result_jpeg (decret)));
        ret = GST_FLOW_ERROR;
        error = TRUE;
        break;
      default:
        GST_ERROR_OBJECT (dec, "Unhandled return code: %s (%d)",
            gst_g1_result_jpeg (decret), decret);
        g_return_val_if_reached (GST_FLOW_OK);
        break;
    }

    if (error)
      break;

  } while (decret != JPEGDEC_FRAME_READY);

  return ret;
}

static gboolean
gst_g1_jpeg_dec_close (GstG1BaseDec * g1dec)
{
  GstG1JPEGDec *dec = GST_G1_JPEG_DEC (g1dec);
  GST_LOG_OBJECT (dec, "closing JPEG decoder");
  JpegDecRelease (g1dec->codec);
  return TRUE;
}
