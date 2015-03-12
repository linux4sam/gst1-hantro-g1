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
/**
 * SECTION:element-g1h264dec
 *
 * Hantro G1 HW accelerated H264 decoder
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 filesrc location=file.mp4 ! qtdemux ! h264parse ! queue ! g1h264dec ! autovideosink
 * ]| 
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif
#include "gstg1h264dec.h"
#include "gstg1allocator.h"
#include "gstg1format.h"
#include "gstg1result.h"

#include <h264decapi.h>
#include <dwl.h>

enum
{
  PROP_0,
  PROP_SKIP_NON_REFERENCE,
  PROP_DISABLE_OUTPUT_REORDERING,
  PROP_INTRA_FREEZE_CONCEALMENT,
  PROP_USE_DISPLAY_SMOOTHING,
};

#define PROP_DEFAULT_SKIP_NON_REFERENCE FALSE
#define PROP_DEFAULT_DISABLE_OUTPUT_REORDERING FALSE
#define PROP_DEFAULT_INTRA_FREEZE_CONCEALMENT FALSE
#define PROP_DEFAULT_USE_DISPLAY_SMOOTHING FALSE

static GstStaticPadTemplate gst_g1_h264_dec_sink_pad_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h264, "
        "stream-format=byte-stream, " "alignment={au,nal}")
    );

GST_DEBUG_CATEGORY_STATIC (g1_h264_dec_debug);
#define GST_CAT_DEFAULT g1_h264_dec_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_PERFORMANCE);

#define gst_g1_h264_dec_parent_class parent_class
G_DEFINE_TYPE (GstG1H264Dec, gst_g1_h264_dec, GST_TYPE_G1_BASE_DEC);

#define GST_G1_H264_FAILED(ret) (H264DEC_OK != (ret))

static void gst_g1_h264_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_g1_h264_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static gboolean gst_g1_h264_dec_open (GstG1BaseDec * dec);
static gboolean gst_g1_h264_dec_close (GstG1BaseDec * dec);
static GstFlowReturn gst_g1_h264_dec_decode (GstG1BaseDec * decoder,
    GstVideoCodecFrame * frame);

static void gst_g1_h264_dec_dwl_to_h264 (GstG1H264Dec * dec,
    DWLLinearMem_t * linearmem, H264DecInput * input, gsize size);

static void
gst_g1_h264_dec_class_init (GstG1H264DecClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstG1BaseDecClass *g1dec_class;

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;
  g1dec_class = (GstG1BaseDecClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->set_property = gst_g1_h264_dec_set_property;
  gobject_class->get_property = gst_g1_h264_dec_get_property;

  g_object_class_install_property (gobject_class, PROP_SKIP_NON_REFERENCE,
      g_param_spec_boolean ("skip-non-reference", "Skip Non Reference",
          "Skip non-reference frames decoding to save CPU consumption and processing time",
          PROP_DEFAULT_SKIP_NON_REFERENCE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_DISABLE_OUTPUT_REORDERING,
      g_param_spec_boolean ("disable-output-reordering",
          "Disable Output Reordering",
          "Prevents decoder from reordering output frames. This may reduce the number "
          "of internally allocated picture buffers, but the application must reorder "
          "them externally. This property will take effect until the next time the codec "
          "is opened.", PROP_DEFAULT_DISABLE_OUTPUT_REORDERING,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_INTRA_FREEZE_CONCEALMENT,
      g_param_spec_boolean ("intra-freeze-concealment",
          "Intra Freeze concealment",
          "Enables error concealment method where decoding starts at next intra picture "
          "after an error in the bitstream. This property will take effect until the next "
          "time the codec is opened.", PROP_DEFAULT_INTRA_FREEZE_CONCEALMENT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_USE_DISPLAY_SMOOTHING,
      g_param_spec_boolean ("use-display-smoothing", "Use Display Smoothing",
          "Enable usage of extra frame buffers to achieve a smoother output. This "
          "can potentially double the number of internally allocated picture buffers. "
          "This property will take effect until the next time the codec is opened.",
          PROP_DEFAULT_USE_DISPLAY_SMOOTHING,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g1dec_class->open = GST_DEBUG_FUNCPTR (gst_g1_h264_dec_open);
  g1dec_class->close = GST_DEBUG_FUNCPTR (gst_g1_h264_dec_close);
  g1dec_class->decode = GST_DEBUG_FUNCPTR (gst_g1_h264_dec_decode);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_g1_h264_dec_sink_pad_template));
  gst_element_class_set_static_metadata (element_class,
      "Hantro G1 H264 decoder", "Codec/Decoder/Video", "Decode an H264 stream",
      "Michael Gruner <michael.gruner@ridgerun.com>");

  GST_DEBUG_CATEGORY_INIT (g1_h264_dec_debug, "g1h264dec", 0,
      "Hantro G1 H264 decoder");
  GST_DEBUG_CATEGORY_GET (GST_CAT_PERFORMANCE, "GST_PERFORMANCE");
}

static void
gst_g1_h264_dec_init (GstG1H264Dec * dec)
{
  GstG1BaseDec *g1dec = GST_G1_BASE_DEC (dec);

  GST_DEBUG_OBJECT (dec, "initializing");
  g1dec->dectype = PP_PIPELINED_DEC_TYPE_H264;
  dec->skip_non_reference = PROP_DEFAULT_SKIP_NON_REFERENCE;
  dec->disable_output_reordering = PROP_DEFAULT_DISABLE_OUTPUT_REORDERING;
  dec->intra_freeze_concealment = PROP_DEFAULT_INTRA_FREEZE_CONCEALMENT;
  dec->use_display_smoothing = PROP_DEFAULT_USE_DISPLAY_SMOOTHING;
}

static gboolean
gst_g1_h264_dec_open (GstG1BaseDec * g1dec)
{
  GstG1H264Dec *dec = GST_G1_H264_DEC (g1dec);
  H264DecRet decret;
  DecDpbFlags flags;
  gboolean ret;

  GST_INFO_OBJECT (dec, "opening H264 decoder");

  /* TODO: do we want this configurable? */
  flags = DEC_DPB_ALLOW_FIELD_ORDERING;

  decret =
      H264DecInit ((H264DecInst *) & g1dec->codec,
      dec->disable_output_reordering, dec->intra_freeze_concealment,
      dec->use_display_smoothing, flags);
  if (GST_G1_H264_FAILED (decret)) {
    GST_ERROR_OBJECT (dec, gst_g1_result_h264 (decret));
    ret = FALSE;
    goto exit;
  }

  GST_DEBUG_OBJECT (dec, "H264 decoder successfully opened");
  ret = TRUE;

exit:
  {
    return ret;
  }
}

static void
gst_g1_h264_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstG1H264Dec *dec = GST_G1_H264_DEC (object);

  switch (prop_id) {
    case PROP_SKIP_NON_REFERENCE:
      dec->skip_non_reference = g_value_get_boolean (value);
      break;
    case PROP_DISABLE_OUTPUT_REORDERING:
      dec->disable_output_reordering = g_value_get_boolean (value);
      break;
    case PROP_INTRA_FREEZE_CONCEALMENT:
      dec->intra_freeze_concealment = g_value_get_boolean (value);
      break;
    case PROP_USE_DISPLAY_SMOOTHING:
      dec->use_display_smoothing = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_g1_h264_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstG1H264Dec *dec = GST_G1_H264_DEC (object);

  switch (prop_id) {
    case PROP_SKIP_NON_REFERENCE:
      g_value_set_boolean (value, dec->skip_non_reference);
      break;
    case PROP_DISABLE_OUTPUT_REORDERING:
      g_value_set_boolean (value, dec->disable_output_reordering);
      break;
    case PROP_INTRA_FREEZE_CONCEALMENT:
      g_value_set_boolean (value, dec->intra_freeze_concealment);
      break;
    case PROP_USE_DISPLAY_SMOOTHING:
      g_value_set_boolean (value, dec->use_display_smoothing);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstFlowReturn
gst_g1_h264_dec_parse_header (GstG1H264Dec * dec)
{
  GstVideoDecoder *bdec = GST_VIDEO_DECODER (dec);
  GstG1BaseDec *g1dec = GST_G1_BASE_DEC (dec);
  GstFlowReturn ret;
  GstVideoFormatInfo finfoi;
  GstVideoCodecState *state;
  H264DecInfo header;
  H264DecRet decret;

  g_return_val_if_fail (dec, GST_FLOW_ERROR);

  decret = H264DecGetInfo (g1dec->codec, &header);
  if (GST_G1_H264_FAILED (decret)) {
    GST_ERROR_OBJECT (g1dec, gst_g1_result_h264 (decret));
    ret = GST_FLOW_ERROR;
    goto exit;
  }

  GST_INFO_OBJECT (dec, "Parsed H264 headers:\n"
      "\tWidth=%d\n"
      "\tHeight=%d\n"
      "\tVideo Range=%d\n"
      "\tMatrix Coefficients=%d\n"
      "\tOuput Format=%d\n"
      "\tSAR Width=%d\n"
      "\tSAR Height=%d\n"
      "\tMonochrome=%d\n"
      "\tInterlaced=%d\n"
      "\tDPB Mode=%d\n"
      "\tPic Buffer Size=%d\n"
      "\tMulti Buffer PP Size=%d",
      header.picWidth,
      header.picHeight,
      header.videoRange,
      header.matrixCoefficients,
      header.outputFormat,
      header.sarWidth,
      header.sarHeight,
      header.monoChrome,
      header.interlacedSequence,
      header.dpbMode, header.picBuffSize, header.multiBuffPpSize);

  state = gst_video_decoder_get_output_state (bdec);
  if (state) {
    state->info.par_n = header.sarWidth;
    state->info.par_d = header.sarHeight;

    /* A 1 on either field means that it was a range at the time of
       fixating caps. Likely the user didn't specify them. Use input
       size */
    if (1 == state->info.width || 1 == state->info.height) {
      state->info.width = header.picWidth;
      state->info.height = header.picHeight;
    }

  }
  finfoi = gst_g1_format_h264_to_gst (H264DEC_SEMIPLANAR_YUV420);
  gst_g1_base_dec_config_format (g1dec, &finfoi,
      header.picWidth, header.picHeight);

  ret = GST_FLOW_OK;

exit:
  {
    return ret;
  }
}

static GstFlowReturn
gst_g1_h264_dec_pop_picture (GstG1H264Dec * dec, GstVideoCodecFrame * frame)
{
  GstG1BaseDec *bdec;
  H264DecPicture picture;
  H264DecRet decret;

  bdec = GST_G1_BASE_DEC (dec);

  do {
    decret = H264DecNextPicture (bdec->codec, &picture, FALSE);
    GST_LOG_OBJECT (dec, "%s (%d) (%p|0x%08x)", gst_g1_result_h264 (decret),
        decret, picture.pOutputPicture, picture.outputPictureBusAddress);

    if (decret != H264DEC_PIC_RDY) {
      break;
    }
    /* TODO: do some error checking here */

    if (picture.nbrOfErrMBs)
      GST_WARNING_OBJECT (dec, "concealed %d macroblocks", picture.nbrOfErrMBs);

    gst_g1_base_dec_push_data (bdec, frame);

  } while (decret == H264DEC_PIC_RDY);



  GST_LOG_OBJECT (dec, "No more pictures to pop");
  return GST_FLOW_OK;
}

static void
gst_g1_h264_dec_dwl_to_h264 (GstG1H264Dec * dec, DWLLinearMem_t * linearmem,
    H264DecInput * h264input, gsize size)
{
  gboolean skip_non_reference;

  g_return_if_fail (dec);
  g_return_if_fail (h264input);
  g_return_if_fail (linearmem);

  GST_OBJECT_LOCK (dec);
  skip_non_reference = dec->skip_non_reference;
  GST_OBJECT_UNLOCK (dec);

  h264input->pStream = (guint8 *) linearmem->virtualAddress;
  h264input->streamBusAddress = linearmem->busAddress;
  h264input->dataLen = size;

  h264input->picId = 0;
  h264input->skipNonReference = skip_non_reference;
  h264input->pUserData = NULL;
}

static GstFlowReturn
gst_g1_h264_dec_decode (GstG1BaseDec * g1dec, GstVideoCodecFrame * frame)
{
  GstG1H264Dec *dec = GST_G1_H264_DEC (g1dec);
  H264DecInput h264input;
  H264DecOutput h264output;
  GstMapInfo minfo;
  H264DecRet decret;
  GstFlowReturn ret;
  DWLLinearMem_t linearmem;
  gboolean error;

  gst_buffer_map (frame->input_buffer, &minfo, GST_MAP_READ);
  linearmem.virtualAddress = (guint32 *) minfo.data;
  linearmem.busAddress = gst_g1_allocator_get_physical (minfo.memory);
  linearmem.size = minfo.size;
  gst_buffer_unmap (frame->input_buffer, &minfo);

  error = FALSE;

  gst_g1_h264_dec_dwl_to_h264 (dec, &linearmem, &h264input, minfo.size);
  do {
    ret = gst_g1_base_dec_allocate_output (g1dec, frame);
    if (GST_FLOW_OK != ret)
      break;

    decret = H264DecDecode (g1dec->codec, &h264input, &h264output);
    GST_LOG_OBJECT (dec, "%s (%d), %d@(%p|0x%08x)", gst_g1_result_h264 (decret),
        decret, h264output.dataLeft, h264output.pStrmCurrPos,
        h264output.strmCurrBusAddress);

    switch (decret) {
      case H264DEC_STRM_PROCESSED:
        GST_LOG_OBJECT (dec, "Frame successfully processed");
        ret = GST_FLOW_OK;
        break;

      case H264DEC_HDRS_RDY:
        ret = gst_g1_h264_dec_parse_header (dec);
        break;

      case H264DEC_PIC_DECODED:
        ret = gst_g1_h264_dec_pop_picture (dec, frame);
        break;

      case H264DEC_ADVANCED_TOOLS:
      case H264DEC_NONREF_PIC_SKIPPED:
        /* NOP */
        break;

      case H264DEC_STREAM_NOT_SUPPORTED:
      case H264DEC_STRM_ERROR:
        GST_VIDEO_DECODER_ERROR (dec, 0, STREAM, DECODE, ("stream error"),
            (gst_g1_result_h264 (decret)), ret);
        break;

      case H264DEC_HW_TIMEOUT:
      case H264DEC_HW_BUS_ERROR:
      case H264DEC_SYSTEM_ERROR:
      case H264DEC_DWL_ERROR:
        GST_ELEMENT_ERROR (dec, RESOURCE, FAILED, ("G1 system error"),
            (gst_g1_result_h264 (decret)));
        ret = GST_FLOW_ERROR;
        error = TRUE;
        break;

      default:
        GST_ERROR_OBJECT (dec, "Unhandled return code: %s (%d)",
            gst_g1_result_h264 (decret), decret);
        g_return_val_if_reached (GST_FLOW_OK);
    }

    if (error)
      break;

    GST_LOG_OBJECT (dec, "Updating pointers");
    h264input.dataLen = h264output.dataLeft;
    h264input.pStream = h264output.pStrmCurrPos;
    h264input.streamBusAddress = h264output.strmCurrBusAddress;

  } while ((decret != H264DEC_STRM_PROCESSED) && (h264output.dataLeft > 0));

  if (h264output.dataLeft > 0)
    GST_WARNING_OBJECT (dec, "found %d bytes corrupted", h264output.dataLeft);

  return ret;
}

static gboolean
gst_g1_h264_dec_close (GstG1BaseDec * g1dec)
{
  GstG1H264Dec *dec = GST_G1_H264_DEC (g1dec);

  GST_INFO_OBJECT (dec, "closing H264 decoder");

  H264DecRelease (g1dec->codec);

  return TRUE;
}
