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
 * SECTION:element-g1mp4dec
 *
 * Hantro G1 HW accelerated mpeg4/H263 decoder
 *
 * <refsect2>
 * <title>Example launch line</title>
 *
 * Play MPEG4 video stream
 *
 * gst-launch-1.0 uridecodebin uri=file:///opt/Serenity.mp4
 * expose-all-streams=false name=srcVideo caps="video/mpeg" srcVideo.
 * ! mpeg4videoparse ! queue ! g1mp4dec use-drm=true
 * ! drmsink full-screen=true zero-memcpy=true
 *
 * Play h263 video stream
 *
 * gst-launch-1.0 uridecodebin uri=file:///opt/100374.mov
 * expose-all-streams=false name=srcVideo caps="video/x-h263" srcVideo.
 * ! h263parse ! queue ! g1mp4dec use-drm=true
 * ! drmsink full-screen=true zero-memcpy=true &
 *
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstg1mp4dec.h"
#include "gstg1allocator.h"
#include "gstg1format.h"
#include "gstg1result.h"

#include <mp4decapi.h>
#include <dwl.h>

enum
{
  PROP_0,
  PROP_SKIP_NON_REFERENCE,
  PROP_ERROR_CONCEALMENT,
  PROP_NUM_FRAMEBUFFER,
};

#define PROP_DEFAULT_SKIP_NON_REFERENCE     FALSE
#define PROP_DEFAULT_ERROR_CONCEALMENT      FALSE
#define PROP_DEFAULT_NUM_FRAMEBUFFER        4

static GstStaticPadTemplate gst_g1_mp4_dec_sink_pad_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("video/mpeg,systemstream=(boolean)false,mpegversion=(int)4,profile=(string){ simple, advanced-simple };"
        "video/x-h263,variant=(string)\"itu\";")
    );

GST_DEBUG_CATEGORY_STATIC (g1_mp4_dec_debug);
#define GST_CAT_DEFAULT g1_mp4_dec_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_PERFORMANCE);

#define gst_g1_mp4_dec_parent_class parent_class
G_DEFINE_TYPE (GstG1MP4Dec, gst_g1_mp4_dec, GST_TYPE_G1_BASE_DEC);

#define GST_G1_MP4_FAILED(ret) (MP4DEC_OK != (ret))

static void gst_g1_mp4_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_g1_mp4_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static gboolean gst_g1_mp4_dec_open (GstG1BaseDec * dec);

static gboolean gst_g1_mp4_dec_close (GstG1BaseDec * dec);

static GstFlowReturn gst_g1_mp4_dec_decode_header (GstG1BaseDec * g1dec,
    GstBuffer * streamheader);

static GstFlowReturn gst_g1_mp4_dec_decode (GstG1BaseDec * decoder,
    GstVideoCodecFrame * frame);

static void gst_g1_mp4_dec_dwl_to_mp4 (GstG1MP4Dec * dec,
    DWLLinearMem_t * linearmem, MP4DecInput * input, gsize size);

static void
gst_g1_mp4_dec_class_init (GstG1MP4DecClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstG1BaseDecClass *g1dec_class;

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;
  g1dec_class = (GstG1BaseDecClass *) klass;

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_g1_mp4_dec_sink_pad_template));

  GST_DEBUG_CATEGORY_INIT (g1_mp4_dec_debug, "g1mp4dec", 0,
      "Hantro G1 MPEG-4 decoder");

  GST_DEBUG_CATEGORY_GET (GST_CAT_PERFORMANCE, "GST_PERFORMANCE");

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->set_property = gst_g1_mp4_dec_set_property;
  gobject_class->get_property = gst_g1_mp4_dec_get_property;

  g_object_class_install_property (gobject_class, PROP_SKIP_NON_REFERENCE,
      g_param_spec_boolean ("skip-non-reference",
          "Skip Non Reference",
          "Skip non-reference frames decoding to save CPU consumption &"
          "processing time", PROP_DEFAULT_SKIP_NON_REFERENCE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ERROR_CONCEALMENT,
      g_param_spec_boolean ("video-freeze-concealment",
          "Video Freeze concealment",
          "When set to non-zero value the decoder will conceal every"
          "frame after an error has been detected in thebitstream,"
          "until the next key frame is decoded. When set to zero,"
          "decoder will conceal only the frames having errors in"
          "bitstream", PROP_DEFAULT_ERROR_CONCEALMENT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_NUM_FRAMEBUFFER,
      g_param_spec_uint ("numFrameBuffers",
          "numFrameBuffers",
          "Number of frame buffers the decoder should allocate."
          "Maximum value is 16, minimum value is 2 or 3depending"
          "on the stream contents. Extra buffers allow for "
          "application-specific post processing by guaranteeing"
          "that the output frame is not immediately overwritten"
          "by the next decoded frame", 3, 16,
          PROP_DEFAULT_NUM_FRAMEBUFFER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));


  g1dec_class->open = GST_DEBUG_FUNCPTR (gst_g1_mp4_dec_open);
  g1dec_class->close = GST_DEBUG_FUNCPTR (gst_g1_mp4_dec_close);
  g1dec_class->decode = GST_DEBUG_FUNCPTR (gst_g1_mp4_dec_decode);
  g1dec_class->decode_header = GST_DEBUG_FUNCPTR (gst_g1_mp4_dec_decode_header);


  gst_element_class_set_static_metadata (element_class,
      "Hantro G1 MPEG4/H263 decoder", "Codec/Decoder/Video",
      "Decode an MPEG4/H263 stream",
      "Sandeep Sheriker <sandeepsheriker.mallikarjun@microchip.com>");
}

static void
gst_g1_mp4_dec_init (GstG1MP4Dec * dec)
{
  GstG1BaseDec *g1dec = GST_G1_BASE_DEC (dec);

  GST_INFO_OBJECT (dec, "initializing");
  g1dec->dectype = PP_PIPELINED_DEC_TYPE_MPEG4;
  dec->skip_non_reference = PROP_DEFAULT_SKIP_NON_REFERENCE;
  dec->error_concealment = PROP_DEFAULT_ERROR_CONCEALMENT;
  dec->numFrameBuffers = PROP_DEFAULT_NUM_FRAMEBUFFER;
  dec->picDecodeNumber = 0;
}

static gboolean
gst_g1_mp4_dec_open (GstG1BaseDec * g1dec)
{
  GstG1MP4Dec *dec = GST_G1_MP4_DEC (g1dec);
  MP4DecRet decret;
  gboolean ret;

  GST_INFO_OBJECT (dec, "opening MP4 decoder");

  decret = MP4DecInit ((MP4DecInst *) & g1dec->codec,
      MP4DEC_MPEG4, dec->error_concealment,
      dec->numFrameBuffers, DEC_REF_FRM_RASTER_SCAN);

  if (GST_G1_MP4_FAILED (decret)) {
    GST_ERROR_OBJECT (dec, gst_g1_result_mp4 (decret));
    ret = FALSE;
    goto exit;
  }

  GST_INFO_OBJECT (dec, "MP4DecInit: MP4 decoder successfully opened");

  ret = TRUE;

exit:
  return ret;
}

static void
gst_g1_mp4_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstG1MP4Dec *dec = GST_G1_MP4_DEC (object);

  switch (prop_id) {
    case PROP_ERROR_CONCEALMENT:
      dec->error_concealment = g_value_get_boolean (value);
      break;
    case PROP_NUM_FRAMEBUFFER:
      dec->numFrameBuffers = g_value_get_int (value);
      break;
    case PROP_SKIP_NON_REFERENCE:
      dec->skip_non_reference = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_g1_mp4_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstG1MP4Dec *dec = GST_G1_MP4_DEC (object);

  switch (prop_id) {
    case PROP_ERROR_CONCEALMENT:
      g_value_set_boolean (value, dec->error_concealment);
      break;
    case PROP_NUM_FRAMEBUFFER:
      g_value_set_int (value, dec->numFrameBuffers);
      break;
    case PROP_SKIP_NON_REFERENCE:
      g_value_set_boolean (value, dec->skip_non_reference);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstFlowReturn
gst_g1_mp4_dec_parse_header (GstG1MP4Dec * dec)
{
  GstVideoDecoder *bdec = GST_VIDEO_DECODER (dec);
  GstG1BaseDec *g1dec = GST_G1_BASE_DEC (dec);
  GstFlowReturn ret = GST_FLOW_OK;
  GstVideoFormatInfo finfoi;
  GstVideoCodecState *state;
  MP4DecInfo header;
  MP4DecRet decret;

  g_return_val_if_fail (dec, GST_FLOW_ERROR);

  decret = MP4DecGetInfo (g1dec->codec, &header);
  if (GST_G1_MP4_FAILED (decret)) {
    GST_ERROR_OBJECT (g1dec, "MP4DecGetInfo failed %s %d\n",
        gst_g1_result_mp4 (decret), decret);
    ret = GST_FLOW_ERROR;
    goto exit;
  }

  GST_LOG_OBJECT (dec, "Parsed MP4 headers:\n"
      "\tframeWidth=%d\n"
      "\tframeHeight=%d\n"
      "\tcodedWidth=%d\n"
      "\tcodedHeight=%d\n"
      "\tstreamFormat=%d\n"
      "\tprofileAndLevelIndication=%d\n"
      "\tvideoFormat=%d\n"
      "\tvideoRange=%d\n"
      "\tparWidth=%d\n"
      "\tparHeight=%d\n"
      "\tinterlacedSequence=%d\n"
      "\tdpbMode=%d\n"
      "\tmultiBuffPpSize=%d\n"
      "\toutputFormat=%d\n",
      header.frameWidth,
      header.frameHeight,
      header.codedWidth,
      header.codedHeight,
      header.streamFormat,
      header.profileAndLevelIndication,
      header.videoFormat,
      header.videoRange,
      header.parWidth,
      header.parHeight,
      header.interlacedSequence,
      header.dpbMode, header.multiBuffPpSize, header.outputFormat);

  state = gst_video_decoder_get_output_state (bdec);
  if (state) {
    state->info.par_n = header.parWidth;
    state->info.par_d = header.parHeight;

    /* A 1 on either field means that it was a range at the time of
       fixating caps. Likely the user didn't specify them. Use input
       size */
    if (1 == state->info.width || 1 == state->info.height) {
      state->info.width = header.frameWidth;
      state->info.height = header.frameHeight;
    }
  }

  finfoi = gst_g1_format_mp4_to_gst (MP4DEC_SEMIPLANAR_YUV420);
  gst_g1_base_dec_config_format (g1dec, &finfoi, header.frameWidth,
      header.frameHeight);
exit:
  return ret;
}

static void
gst_g1_mp4_dec_dwl_to_mp4 (GstG1MP4Dec * dec,
    DWLLinearMem_t * linearmem, MP4DecInput * mp4input, gsize size)
{
  gboolean skip_non_reference;

  g_return_if_fail (dec);
  g_return_if_fail (mp4input);
  g_return_if_fail (linearmem);

  GST_OBJECT_LOCK (dec);
  skip_non_reference = dec->skip_non_reference;
  GST_OBJECT_UNLOCK (dec);

  mp4input->pStream = ((guint8 *) linearmem->virtualAddress);
  mp4input->dataLen = size;
  mp4input->streamBusAddress = linearmem->busAddress;

  mp4input->picId = 0;
  mp4input->skipNonReference = skip_non_reference;
}

static GstFlowReturn
gst_g1_mp4_dec_pop_picture (GstG1MP4Dec * dec, GstVideoCodecFrame * frame)
{
  GstG1BaseDec *bdec;
  MP4DecPicture picture;
  MP4DecRet decret;

  bdec = GST_G1_BASE_DEC (dec);

  do {
    decret = MP4DecNextPicture (bdec->codec, &picture, FALSE);
    if (decret != MP4DEC_PIC_RDY) {
      GST_ERROR_OBJECT (dec, "%s (%d) (%p|0x%08x)",
          gst_g1_result_mp4 (decret), decret, picture.pOutputPicture,
          picture.outputPictureBusAddress);
      break;
    }
    /* TODO: do some error checking here */

    if (picture.nbrOfErrMBs)
      GST_LOG_OBJECT (dec, "concealed %d macroblocks", picture.nbrOfErrMBs);

    gst_g1_base_dec_push_data (bdec, frame);

  } while (decret == MP4DEC_PIC_RDY);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_g1_mp4_dec_decode_header (GstG1BaseDec * g1dec, GstBuffer * streamheader)
{
  GstG1MP4Dec *dec = GST_G1_MP4_DEC (g1dec);
  MP4DecInput mp4input;
  MP4DecOutput mp4output;
  GstMapInfo minfo;
  MP4DecRet decret;
  GstFlowReturn ret = GST_FLOW_OK;
  DWLLinearMem_t linearmem;

  gst_buffer_map (streamheader, &minfo, GST_MAP_READ);
  linearmem.virtualAddress = (guint32 *) minfo.data;
  linearmem.busAddress = gst_g1_allocator_get_physical (minfo.memory);
  linearmem.size = minfo.size;
  gst_buffer_unmap (streamheader, &minfo);

  gst_g1_mp4_dec_dwl_to_mp4 (dec, &linearmem, &mp4input, minfo.size);

  decret = MP4DecDecode (g1dec->codec, &mp4input, &mp4output);
  switch (decret) {
    case MP4DEC_HDRS_RDY:
    case MP4DEC_DP_HDRS_RDY:
      /* read stream info */
      ret = gst_g1_mp4_dec_parse_header (dec);
      GST_LOG_OBJECT (dec, "handle MP4DEC_DP_HDRS_RDY");
      break;
    default:
      GST_ERROR_OBJECT (dec, "Unhandled return code: %s (%d)",
          gst_g1_result_mp4 (decret), decret);
      g_return_val_if_reached (GST_FLOW_OK);
  }
  return ret;
}

static GstFlowReturn
gst_g1_mp4_dec_decode (GstG1BaseDec * g1dec, GstVideoCodecFrame * frame)
{
  GstG1MP4Dec *dec = GST_G1_MP4_DEC (g1dec);
  MP4DecInput mp4input;
  MP4DecOutput mp4output;
  GstMapInfo minfo;
  MP4DecRet decret;
  GstFlowReturn ret;
  DWLLinearMem_t linearmem;
  gboolean error = FALSE;

  gst_buffer_map (frame->input_buffer, &minfo, GST_MAP_READ);
  linearmem.virtualAddress = (guint32 *) minfo.data;
  linearmem.busAddress = gst_g1_allocator_get_physical (minfo.memory);
  linearmem.size = minfo.size;
  gst_buffer_unmap (frame->input_buffer, &minfo);

  gst_g1_mp4_dec_dwl_to_mp4 (dec, &linearmem, &mp4input, minfo.size);

  do {
    ret = gst_g1_base_dec_allocate_output (g1dec, frame);
    if (ret != GST_FLOW_OK)
      break;

    mp4input.picId = dec->picDecodeNumber;

    decret = MP4DecDecode (g1dec->codec, &mp4input, &mp4output);
    switch (decret) {
      case MP4DEC_HDRS_RDY:
      case MP4DEC_DP_HDRS_RDY:
        /* read stream info */
        ret = gst_g1_mp4_dec_parse_header (dec);
        GST_LOG_OBJECT (dec, "handle MP4DEC_DP_HDRS_RDY");
        break;
        /* a picture was decoded */
      case MP4DEC_PIC_DECODED:
        GST_LOG_OBJECT (dec, "MP4DEC_PIC_DECODED");
        dec->picDecodeNumber++;
        ret = gst_g1_mp4_dec_pop_picture (dec, frame);
        break;
      case MP4DEC_STRM_PROCESSED:
        GST_LOG_OBJECT (dec, "Frame successfully processed");
        ret = gst_g1_mp4_dec_parse_header (dec);
        ret = GST_FLOW_OK;
        break;
      case MP4DEC_NOT_INITIALIZED:
        GST_ERROR_OBJECT (dec, "MP4DEC_NOT_INITIALIZED");
        error = TRUE;
        break;
      case MP4DEC_FORMAT_NOT_SUPPORTED:
      case MP4DEC_STRM_NOT_SUPPORTED:
      case MP4DEC_STRM_ERROR:
        GST_VIDEO_DECODER_ERROR (dec, 0, STREAM, DECODE,
            ("stream error"), (gst_g1_result_mp4 (decret)), ret);
        error = TRUE;
        break;
      case MP4DEC_HW_TIMEOUT:
      case MP4DEC_HW_BUS_ERROR:
      case MP4DEC_SYSTEM_ERROR:
      case MP4DEC_DWL_ERROR:
        GST_ELEMENT_ERROR (dec, RESOURCE, FAILED,
            ("G1 system error"), (gst_g1_result_mp4 (decret)));
        ret = GST_FLOW_ERROR;
        error = TRUE;
        break;
      default:
        GST_ERROR_OBJECT (dec, "Unhandled return code: %s (%d)",
            gst_g1_result_mp4 (decret), decret);
        g_return_val_if_reached (GST_FLOW_OK);
        break;
    }

    if (error)
      break;

    mp4input.dataLen = mp4output.dataLeft;
    mp4input.pStream = mp4output.pStrmCurrPos;
    mp4input.streamBusAddress = mp4output.strmCurrBusAddress;

  } while ((decret != MP4DEC_STRM_PROCESSED) && (mp4output.dataLeft > 0));

  if (mp4output.dataLeft > 0)
    GST_LOG_OBJECT (dec, "dataLeft = %d bytes", mp4output.dataLeft);

  return ret;
}

static gboolean
gst_g1_mp4_dec_close (GstG1BaseDec * g1dec)
{
  GstG1MP4Dec *dec = GST_G1_MP4_DEC (g1dec);

  GST_INFO_OBJECT (dec, "closing MP4 decoder");
  MP4DecRelease (g1dec->codec);
  return TRUE;
}
