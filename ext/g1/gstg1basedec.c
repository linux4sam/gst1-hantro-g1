 /*
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
 * SECTION:element-g1basedec
 *
 * Hantro G1 HW accelerated base decoder class
 *
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstg1basedec.h"
#include "gstg1result.h"
#include "gstg1format.h"
#include "gstg1enum.h"

#include <string.h>
#include <stdio.h>

enum
{
  PROP_0,
  PROP_ROTATION,
  PROP_BRIGHTNESS,
  PROP_CONTRAST,
  PROP_SATURATION,
  PROP_CROP_X,
  PROP_CROP_Y,
  PROP_CROP_WIDTH,
  PROP_CROP_HEIGHT,
  PROP_MASK1_LOCATION,
  PROP_MASK1_X,
  PROP_MASK1_Y,
  PROP_MASK1_WIDTH,
  PROP_MASK1_HEIGHT,
  PROP_USE_DRM,
};

#define PROP_DEFAULT_ROTATION PP_ROTATION_NONE
#define PROP_DEFAULT_BRIGHTNESS 0
#define PROP_DEFAULT_CONTRAST 0
#define PROP_DEFAULT_SATURATION 0
#define PROP_DEFAULT_CROP_X 0
#define PROP_DEFAULT_CROP_Y 0
#define PROP_DEFAULT_CROP_WIDTH  0
#define PROP_DEFAULT_CROP_HEIGHT 0
#define PROP_DEFAULT_MASK1_LOCATION NULL
#define PROP_DEFAULT_USE_DRM FALSE
#define PROP_DEFAULT_MASK1_X 0
#define PROP_DEFAULT_MASK1_Y 0
#define PROP_DEFAULT_MASK1_WIDTH 0
#define PROP_DEFAULT_MASK1_HEIGHT 0

/* TODO: There are non standard formats missing, add them! */
static GstStaticPadTemplate gst_g1_base_dec_src_pad_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
        ("{ GRAY8, YUY2, YVYU, UYVY, NV16, I420, NV12, RGB15, RGB16, BGR15, BGR16, RGBx, BGRx }")));

GST_DEBUG_CATEGORY_STATIC (g1_base_dec_debug);
#define GST_CAT_DEFAULT g1_base_dec_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_PERFORMANCE);

#define gst_g1_base_dec_parent_class parent_class
G_DEFINE_TYPE (GstG1BaseDec, gst_g1_base_dec, GST_TYPE_VIDEO_DECODER);

#define GST_G1_PP_FAILED(ret) ((PP_OK != (ret)))

/* Enable to get physical address of gem, 
   otherwise hardcoded physical address for debugging */
#define ATMEL_GET_PHYSICAL

static void gst_g1_base_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_g1_base_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static gboolean gst_g1_base_dec_open (GstVideoDecoder * decoder);
static GstFlowReturn gst_g1_base_dec_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame);
static gboolean gst_g1_base_dec_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state);
static gboolean gst_g1_base_dec_close (GstVideoDecoder * decoder);
static gboolean gst_g1_base_dec_decide_allocation (GstVideoDecoder * decoder,
    GstQuery * query);
static gboolean gst_g1_base_dec_propose_allocation (GstVideoDecoder * decoder,
    GstQuery * query);

static gboolean gst_g1_base_dec_copy_memory (GstG1BaseDec * dec,
    GstMemory ** dst, GstMemory * src);
static gboolean gst_g1_base_dec_get_config (GstG1BaseDec * g1dec,
    PPConfig * config);
static gboolean gst_g1_base_dec_setup_pp (GstG1BaseDec * g1dec);
static void gst_g1_base_dec_config_rotation (GstG1BaseDec * g1dec,
    gint rotation);
static void gst_g1_base_dec_config_brightness (GstG1BaseDec * g1dec,
    gint brightness);
static void gst_g1_base_dec_config_contrast (GstG1BaseDec * g1dec,
    gint contrast);
static void gst_g1_base_dec_config_saturation (GstG1BaseDec * g1dec,
    gint saturation);
static void gst_g1_base_dec_config_crop (GstG1BaseDec * g1dec,
    gint x, gint y, gint width, gint height);
static void gst_g1_base_dec_config_mask1 (GstG1BaseDec * g1dec,
    gchar * location, gint x, gint y, gint width, gint height);

static void
gst_g1_base_dec_class_init (GstG1BaseDecClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstVideoDecoderClass *vdec_class;

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;
  vdec_class = (GstVideoDecoderClass *) klass;

  gobject_class->set_property = gst_g1_base_dec_set_property;
  gobject_class->get_property = gst_g1_base_dec_get_property;

  g_object_class_install_property (gobject_class, PROP_ROTATION,
      g_param_spec_enum ("rotation", "Rotation",
          "Picture rotation",
          GST_G1_ENUM_ROTATION_TYPE,
          PROP_DEFAULT_ROTATION, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BRIGHTNESS,
      g_param_spec_int ("brightness",
          "Brightness",
          "Output picture's brightness",
          -128, 127,
          PROP_DEFAULT_BRIGHTNESS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CONTRAST,
      g_param_spec_int ("contrast", "Contrast",
          "Output picture's contrast",
          -64, 64,
          PROP_DEFAULT_CONTRAST, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SATURATION,
      g_param_spec_int ("saturation",
          "Saturation",
          "Output picture's saturation",
          -64, 128,
          PROP_DEFAULT_SATURATION, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CROP_X,
      g_param_spec_uint ("crop-x", "Crop X",
          "X coordinate of the cropping area. Must be less than the input image's "
          "width and multiple of 16.",
          0, 4096,
          PROP_DEFAULT_CROP_X, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CROP_Y,
      g_param_spec_uint ("crop-y", "Crop Y",
          "Y coordinate of the cropping area. Must be less than the input image's "
          "height and multiple of 16.",
          0, 4096,
          PROP_DEFAULT_CROP_Y, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CROP_WIDTH,
      g_param_spec_uint ("crop-width",
          "Crop Width",
          "Width of the cropping area. Must be at least 1/3 the output image's "
          "width and multiple of 8. Setting crop width or height to 0 disables cropping.",
          0, 4672,
          PROP_DEFAULT_CROP_WIDTH, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CROP_HEIGHT,
      g_param_spec_uint ("crop-height",
          "Crop Height",
          "Height of the cropping area. Must be at least 1/3 the output image's "
          "height-2 and multiple of 8. Setting crop width or height to 0 disables "
          "cropping.", 0, 4672,
          PROP_DEFAULT_CROP_HEIGHT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MASK1_LOCATION,
      g_param_spec_string ("mask1-location",
          "Mask 1 Location",
          "Path to the file containing the first mask. This file must be in a raw "
          "ARGB format",
          PROP_DEFAULT_MASK1_LOCATION,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MASK1_X,
      g_param_spec_uint ("mask1-x", "Mask 1 X",
          "X coordinate of the first mask",
          0, 4096,
          PROP_DEFAULT_MASK1_X, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MASK1_Y,
      g_param_spec_uint ("mask1-y", "Mask 1 Y",
          "Y coordinate of the first mask",
          0, 4096,
          PROP_DEFAULT_MASK1_Y, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MASK1_WIDTH,
      g_param_spec_uint ("mask1-width",
          "Mask 1 Width",
          "Width of the first mask",
          0, 4096,
          PROP_DEFAULT_MASK1_WIDTH,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MASK1_HEIGHT,
      g_param_spec_uint ("mask1-height",
          "Mask 1 Height",
          "Height of the first mask",
          0, 4096,
          PROP_DEFAULT_MASK1_HEIGHT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_USE_DRM,
      g_param_spec_boolean ("use-drm", "Use DRM",
          "Identify fbdev or drm"
          "true/false", PROP_DEFAULT_USE_DRM, G_PARAM_READWRITE));

  parent_class = g_type_class_peek_parent (klass);

  klass->open = NULL;
  klass->close = NULL;
  klass->decode = NULL;

  vdec_class->open = GST_DEBUG_FUNCPTR (gst_g1_base_dec_open);
  vdec_class->handle_frame = GST_DEBUG_FUNCPTR (gst_g1_base_dec_handle_frame);
  vdec_class->set_format = GST_DEBUG_FUNCPTR (gst_g1_base_dec_set_format);
  vdec_class->close = GST_DEBUG_FUNCPTR (gst_g1_base_dec_close);
  vdec_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_g1_base_dec_decide_allocation);
  vdec_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_g1_base_dec_propose_allocation);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_g1_base_dec_src_pad_template));

  GST_DEBUG_CATEGORY_INIT (g1_base_dec_debug, "g1basedec", 0,
      "Hantro G1 base decoder class");
  GST_DEBUG_CATEGORY_GET (GST_CAT_PERFORMANCE, "GST_PERFORMANCE");
}

static void
gst_g1_base_dec_init (GstG1BaseDec * dec)
{
  GST_DEBUG_OBJECT (dec, "initializing");

  dec->codec = NULL;
  dec->pp = NULL;
  dec->dectype = PP_PIPELINE_DISABLED;
  dec->ppconfig = (const PPConfig) {
    {
  0}};
  dec->allocator = NULL;

  dec->rotation = PROP_DEFAULT_ROTATION;

  dec->brightness = PROP_DEFAULT_BRIGHTNESS;
  dec->contrast = PROP_DEFAULT_CONTRAST;
  dec->saturation = PROP_DEFAULT_SATURATION;

  dec->crop_x = PROP_DEFAULT_CROP_X;
  dec->crop_y = PROP_DEFAULT_CROP_Y;
  dec->crop_width = PROP_DEFAULT_CROP_WIDTH;
  dec->crop_height = PROP_DEFAULT_CROP_HEIGHT;

  dec->mask1_location = PROP_DEFAULT_MASK1_LOCATION;
  dec->mask1_x = PROP_DEFAULT_MASK1_X;
  dec->mask1_y = PROP_DEFAULT_MASK1_Y;
  dec->mask1_width = PROP_DEFAULT_MASK1_WIDTH;
  dec->mask1_height = PROP_DEFAULT_MASK1_HEIGHT;
  dec->mask1_mem = NULL;
}

static gboolean
gst_g1_base_dec_open (GstVideoDecoder * decoder)
{
  GstG1BaseDec *g1dec;
  GstG1BaseDecClass *g1decclass;
  PPResult ppret;
  gboolean ret;

  g1dec = GST_G1_BASE_DEC (decoder);
  g1decclass = GST_G1_BASE_DEC_CLASS (G_OBJECT_GET_CLASS (g1dec));

  GST_DEBUG_OBJECT (g1dec, "opening G1 decoder");

  g1dec->allocator = gst_allocator_find (GST_ALLOCATOR_DWL);
  /* Any error here is a programming error */
  g_return_val_if_fail (g1dec->allocator, FALSE);

  ppret = PPInit (&g1dec->pp);
  if (GST_G1_PP_FAILED (ppret)) {
    GST_ERROR_OBJECT (g1dec, "Failed to open post processor, %s",
        gst_g1_result_pp (ppret));
    ret = FALSE;
    goto exit;
  }

  g_return_val_if_fail (g1decclass->open, FALSE);
  if (!g1decclass->open (g1dec)) {
    GST_ERROR_OBJECT (g1dec, "Failed to open codec");
    ret = FALSE;
    goto exit;
  }

  g_return_val_if_fail (g1dec->dectype, FALSE);
  ppret = PPDecCombinedModeEnable (g1dec->pp, g1dec->codec, g1dec->dectype);
  if (GST_G1_PP_FAILED (ppret)) {
    GST_ERROR_OBJECT (g1dec, "Failed to chain post processor, %s",
        gst_g1_result_pp (ppret));
    ret = FALSE;
    goto exit;
  }

  if (!gst_g1_base_dec_setup_pp (g1dec)) {
    GST_ERROR_OBJECT (g1dec, "Failed to set pp initial configuration");
    ret = FALSE;
    goto exit;
  }

  GST_INFO_OBJECT (g1dec, "Successfully opened codec");
  ret = TRUE;

exit:
  {
    return ret;
  }
}

static gboolean
gst_g1_base_dec_propose_allocation (GstVideoDecoder * decoder, GstQuery * query)
{
  GstG1BaseDec *g1dec = GST_G1_BASE_DEC (decoder);
  GstAllocationParams params;

  params = (const GstAllocationParams) {
  0};
  params.flags |= GST_MEMORY_FLAG_PHYSICALLY_CONTIGUOUS;

  GST_INFO_OBJECT (g1dec, "proposing " GST_ALLOCATOR_DWL " allocator");

  /* By now we should have the allocator already */
  g_return_val_if_fail (g1dec->allocator, FALSE);

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  gst_query_add_allocation_param (query, g1dec->allocator, &params);

  return GST_VIDEO_DECODER_CLASS (parent_class)->propose_allocation (decoder,
      query);

}

static gboolean
gst_g1_base_dec_decide_allocation (GstVideoDecoder * decoder, GstQuery * query)
{
  guint nparams;
  gint i;
  GstAllocationParams params;
  GstAllocator *allocator;

  nparams = gst_query_get_n_allocation_params (query);
  for (i = 0; i < nparams; ++i) {
    gst_query_parse_nth_allocation_param (query, i, &allocator, &params);
    if (GST_IS_G1_ALLOCATOR (allocator)) {
      GST_INFO_OBJECT (decoder,
          "downstream provided a compatible G1 allocator");
    } else {
      GST_DEBUG_OBJECT (decoder, "discarding incompatible allocator");
      gst_query_remove_nth_allocation_param (query, i);
    }
  }

  /* If no one provided a compatible g1 allocator, provide it ourselves */
  nparams = gst_query_get_n_allocation_params (query);
  if (!nparams) {
    GST_INFO_OBJECT (decoder, "using fallback " GST_ALLOCATOR_DWL " allocator");
    gst_g1_base_dec_propose_allocation (decoder, query);
  }

  return GST_VIDEO_DECODER_CLASS (parent_class)->decide_allocation (decoder,
      query);
}

static gboolean
gst_g1_base_dec_copy_memory (GstG1BaseDec * dec, GstMemory ** dst,
    GstMemory * src)
{
  GstAllocationParams params;
  GstMapInfo srcinfo;
  GstMapInfo dstinfo;
  const gchar *errormsg;

  g_return_val_if_fail (src, FALSE);
  g_return_val_if_fail (dec, FALSE);
  g_return_val_if_fail (dec->allocator, FALSE);

  params = (GstAllocationParams) {
  0};
  params.flags |= GST_MEMORY_FLAG_PHYSICALLY_CONTIGUOUS;

  *dst = gst_allocator_alloc (dec->allocator, src->size, &params);

  if (!gst_memory_map (src, &srcinfo, GST_MAP_READ)) {
    errormsg = "unable to map src memory";
    goto srcerror;
  }

  if (!gst_memory_map (*dst, &dstinfo, GST_MAP_WRITE)) {
    errormsg = "unable to map dst memory";
    goto dsterror;
  }

  GST_CAT_LOG (GST_CAT_PERFORMANCE,
      "the G1 decoders only accept physically contiguous memory, copying data...");
  memcpy (dstinfo.data, srcinfo.data, dstinfo.size);

  gst_memory_unmap (src, &srcinfo);
  gst_memory_unmap (*dst, &dstinfo);
  return TRUE;

dsterror:
  {
    gst_memory_unmap (src, &srcinfo);
  }
srcerror:
  {
    GST_ERROR_OBJECT (dec, errormsg);
    gst_allocator_free (dec->allocator, *dst);
    *dst = NULL;
    return FALSE;
  }
}

static GstFlowReturn
gst_g1_base_dec_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  GstG1BaseDec *g1dec;
  GstG1BaseDecClass *g1decclass;
  GstMemory *mem, *g1mem;
  GstFlowReturn ret;
  GstClockTime start, end;

  g1dec = GST_G1_BASE_DEC (decoder);
  g1decclass = GST_G1_BASE_DEC_CLASS (G_OBJECT_GET_CLASS (g1dec));

  g_return_val_if_fail (g1decclass->decode, GST_FLOW_NOT_SUPPORTED);

  start = gst_util_get_timestamp ();

  mem = gst_buffer_get_all_memory (frame->input_buffer);

  GST_LOG_OBJECT (g1dec, "Testing contiguousness");
  if (!GST_IS_G1_ALLOCATOR (mem->allocator)) {
    if (!gst_g1_base_dec_copy_memory (g1dec, &g1mem, mem)) {
      GST_ERROR_OBJECT (g1dec,
          "unable to copy input buffer to contiguous memory");
      ret = GST_FLOW_NOT_SUPPORTED;
      goto exit;
    }
    gst_memory_unref (mem);
    gst_buffer_replace_all_memory (frame->input_buffer, g1mem);
  }

  GST_LOG_OBJECT (g1dec, "Passing buffer to decoder");
  ret = g1decclass->decode (g1dec, frame);
  end = gst_util_get_timestamp ();
  GST_CAT_DEBUG (GST_CAT_PERFORMANCE, "Processed buffer in %" GST_TIME_FORMAT,
      GST_TIME_ARGS (end - start));

  goto exit;

exit:
  {
    gst_video_decoder_drop_frame (decoder, frame);
    return ret;
  }
}

static gboolean
gst_g1_base_dec_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state)
{
  GstG1BaseDec *dec = GST_G1_BASE_DEC (decoder);
  GstCaps *caps;
  GstVideoInfo vinfo;
  gboolean ret;
  gchar *desc;

  caps = gst_pad_get_allowed_caps (GST_VIDEO_DECODER_SRC_PAD (decoder));
  caps = gst_caps_fixate (caps);
  desc = gst_caps_to_string (caps);

  GST_DEBUG_OBJECT (dec, "Parsed %s from downstream", desc);

  if (!gst_video_info_from_caps (&vinfo, caps)) {
    GST_ERROR_OBJECT (dec, "Unable to parse downstream caps");
    ret = FALSE;
    goto exit;
  }

  state->info = vinfo;
  state->caps = caps;

#define Y 0
#define CbCr 1
  GST_VIDEO_INFO_PLANE_OFFSET (&state->info, Y) = 0;
  GST_VIDEO_INFO_PLANE_OFFSET (&state->info, CbCr) =
      GST_VIDEO_INFO_WIDTH (&vinfo) * GST_VIDEO_INFO_HEIGHT (&vinfo);

  gst_video_decoder_set_output_state (decoder,
      GST_VIDEO_FORMAT_INFO_FORMAT (vinfo.finfo),
      GST_VIDEO_INFO_WIDTH (&vinfo), GST_VIDEO_INFO_HEIGHT (&vinfo), state);
  gst_video_decoder_negotiate (decoder);

  /* Cropping depends on output format */
  gst_g1_base_dec_config_crop (dec, dec->crop_x, dec->crop_y,
      dec->crop_width, dec->crop_height);

exit:
  {
    g_free (desc);
    return ret;
  }
}

static gboolean
gst_g1_base_dec_close (GstVideoDecoder * decoder)
{
  GstG1BaseDec *g1dec;
  GstG1BaseDecClass *g1decclass;

  g1dec = GST_G1_BASE_DEC (decoder);
  g1decclass = GST_G1_BASE_DEC_CLASS (G_OBJECT_GET_CLASS (g1dec));

  GST_DEBUG_OBJECT (decoder, "close G1 decoder");

  PPRelease (g1dec->pp);
  g1dec->pp = NULL;

  g_return_val_if_fail (g1decclass->close, FALSE);
  return g1decclass->close (g1dec);
}

GstFlowReturn
gst_g1_base_dec_allocate_output (GstG1BaseDec * dec, GstVideoCodecFrame * frame)
{
  GstVideoDecoder *bdec = GST_VIDEO_DECODER (dec);
  GstVideoCodecState *state;
  GstVideoInfo *vinfo;
  GstVideoFormatInfo *finfo;
  GstMemory *mem;
  guint32 physaddress;
  GstFlowReturn ret;
  PPResult ppret;

  g_return_val_if_fail (dec, GST_FLOW_ERROR);
  g_return_val_if_fail (frame, GST_FLOW_ERROR);

  /* If the ppconfig hasn't been set we are not ready yet */
  if (!dec->ppconfig.ppInImg.width || !dec->ppconfig.ppInImg.height ||
      !dec->ppconfig.ppInImg.pixFormat) {
    GST_DEBUG_OBJECT (dec,
        "Decoder has not parsed stream headers, skipping buffer");
    ret = GST_FLOW_OK;
    goto exit;
  }

  state = gst_video_decoder_get_output_state (bdec);
  vinfo = &state->info;
  finfo = vinfo->finfo;

  if (frame->output_buffer) {
    gst_buffer_unref (frame->output_buffer);
    frame->output_buffer = NULL;
  }

  ret = gst_video_decoder_allocate_output_frame (bdec, frame);
  if (GST_FLOW_OK != ret) {
    GST_ELEMENT_ERROR (dec, RESOURCE, NO_SPACE_LEFT,
        ("unable to allocate memory for post processor"), (NULL));
    goto stateunref;
  }
  /* Atmel to identify fbdevsink or drmsink */
  if (dec->use_drm) {
    /* Atmel: Get physical address of gem object to pass to PP API */
    physaddress = gst_g1_gem_get_physical ();
  } else {
    /* It is mandatory for this buffer to be G1 */
    mem = gst_buffer_get_all_memory (frame->output_buffer);
    g_return_val_if_fail (GST_IS_G1_ALLOCATOR (mem->allocator), GST_FLOW_ERROR);
    physaddress = gst_g1_allocator_get_physical (mem);
  }
#define Y 0
#define CbCr 1

  dec->ppconfig.ppOutImg.bufferBusAddr = physaddress +
      GST_VIDEO_INFO_PLANE_OFFSET (vinfo, Y);
  dec->ppconfig.ppOutImg.bufferChromaBusAddr =
      dec->ppconfig.ppOutImg.bufferBusAddr + GST_VIDEO_INFO_PLANE_OFFSET (vinfo,
      CbCr);
  dec->ppconfig.ppOutImg.pixFormat = gst_g1_format_gst_to_pp (finfo);
  dec->ppconfig.ppOutImg.width = GST_VIDEO_INFO_WIDTH (vinfo);
  dec->ppconfig.ppOutImg.height = GST_VIDEO_INFO_HEIGHT (vinfo);
  dec->ppconfig.ppOutRgb.ditheringEnable = 1;

  dec->ppconfig.ppOutFrmBuffer.enable = 0;
  dec->ppconfig.ppOutFrmBuffer.writeOriginX = 200;
  dec->ppconfig.ppOutFrmBuffer.writeOriginY = 120;
  dec->ppconfig.ppOutFrmBuffer.frameBufferWidth = 400;
  dec->ppconfig.ppOutFrmBuffer.frameBufferHeight = 240;

  ppret = PPSetConfig (dec->pp, &dec->ppconfig);
  if (GST_G1_PP_FAILED (ppret)) {
    GST_ERROR_OBJECT (dec, gst_g1_result_pp (ppret));
    ret = GST_FLOW_ERROR;
    goto memunref;
  }

  ret = GST_FLOW_OK;
  GST_LOG_OBJECT (dec, "Succesfully allocated memory 0x%08x", physaddress);

memunref:
  {
    if (!dec->use_drm)
      gst_memory_unref (mem);
  }
stateunref:
  {
    gst_video_codec_state_unref (state);
  }
exit:
  {
    return ret;
  }
}

GstFlowReturn
gst_g1_base_dec_push_data (GstG1BaseDec * dec, GstVideoCodecFrame * frame)
{
  GstVideoDecoder *bdec = GST_VIDEO_DECODER (dec);
  GstFlowReturn ret;
  PPResult ppret;

  g_return_val_if_fail (dec, GST_FLOW_ERROR);
  g_return_val_if_fail (frame, GST_FLOW_ERROR);
  g_return_val_if_fail (frame->output_buffer, GST_FLOW_ERROR);

  ppret = PPGetResult (dec->pp);
  if (GST_G1_PP_FAILED (ppret)) {
    GST_ERROR_OBJECT (dec, gst_g1_result_pp (ppret));
    ret = GST_FLOW_ERROR;
    goto exit;
  }

  GST_LOG_OBJECT (dec, "Successfully pushed buffer");
  gst_video_codec_frame_ref (frame);
  ret = gst_video_decoder_finish_frame (bdec, frame);

exit:
  {
    return ret;
  }
}

static void
gst_g1_base_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstG1BaseDec *g1dec = GST_G1_BASE_DEC (object);

  switch (prop_id) {
    PROP_ROTATION:
      gst_g1_base_dec_config_rotation (g1dec, g_value_get_enum (value));
      break;
    case PROP_BRIGHTNESS:
      gst_g1_base_dec_config_brightness (g1dec, g_value_get_int (value));
      break;
    case PROP_CONTRAST:
      gst_g1_base_dec_config_contrast (g1dec, g_value_get_int (value));
      break;
    case PROP_SATURATION:
      gst_g1_base_dec_config_saturation (g1dec, g_value_get_int (value));
      break;
    case PROP_CROP_X:
      gst_g1_base_dec_config_crop (g1dec, (gint) g_value_get_uint (value),
          -1, -1, -1);
      break;
    case PROP_CROP_Y:
      gst_g1_base_dec_config_crop (g1dec, -1, (gint) g_value_get_uint (value),
          -1, -1);
      break;
    case PROP_CROP_WIDTH:
      gst_g1_base_dec_config_crop (g1dec, -1, -1,
          (gint) g_value_get_uint (value), -1);
      break;
    case PROP_CROP_HEIGHT:
      gst_g1_base_dec_config_crop (g1dec, -1, -1, -1,
          (gint) g_value_get_uint (value));
      break;
    case PROP_MASK1_LOCATION:
      g_print ("Locatio=%s\n", g_value_get_string (value));
      gst_g1_base_dec_config_mask1 (g1dec, g_value_get_string (value),
          -1, -1, -1, -1);
      break;
    case PROP_MASK1_X:
      gst_g1_base_dec_config_mask1 (g1dec, (gpointer) - 1,
          (gint) g_value_get_uint (value), -1, -1, -1);
      break;
    case PROP_MASK1_Y:
      gst_g1_base_dec_config_mask1 (g1dec, (gpointer) - 1,
          -1, (gint) g_value_get_uint (value), -1, -1);
      break;
    case PROP_MASK1_WIDTH:
      gst_g1_base_dec_config_mask1 (g1dec, (gpointer) - 1,
          -1, -1, (gint) g_value_get_uint (value), -1);
      break;
    case PROP_MASK1_HEIGHT:
      gst_g1_base_dec_config_mask1 (g1dec, (gpointer) - 1,
          -1, -1, -1, (gint) g_value_get_uint (value));
      break;
    case PROP_USE_DRM:
      g1dec->use_drm = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_g1_base_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstG1BaseDec *g1dec = GST_G1_BASE_DEC (object);

  switch (prop_id) {
    case PROP_ROTATION:
      g_value_set_enum (value, g1dec->rotation);
      break;
    case PROP_BRIGHTNESS:
      g_value_set_int (value, g1dec->brightness);
      break;
    case PROP_CONTRAST:
      g_value_set_int (value, g1dec->contrast);
      break;
    case PROP_SATURATION:
      g_value_set_int (value, g1dec->saturation);
      break;
    case PROP_CROP_X:
      g_value_set_uint (value, g1dec->crop_x);
      break;
    case PROP_CROP_Y:
      g_value_set_uint (value, g1dec->crop_y);
      break;
    case PROP_CROP_WIDTH:
      g_value_set_uint (value, g1dec->crop_width);
      break;
    case PROP_CROP_HEIGHT:
      g_value_set_uint (value, g1dec->crop_height);
      break;
    case PROP_MASK1_LOCATION:
      g_value_set_string (value, g1dec->mask1_location);
      break;
    case PROP_MASK1_X:
      g_value_set_uint (value, g1dec->mask1_x);
      break;
    case PROP_MASK1_Y:
      g_value_set_uint (value, g1dec->mask1_y);
      break;
    case PROP_MASK1_WIDTH:
      g_value_set_uint (value, g1dec->mask1_width);
      break;
    case PROP_MASK1_HEIGHT:
      g_value_set_uint (value, g1dec->mask1_height);
      break;
    case PROP_USE_DRM:
      g_value_set_boolean (value, g1dec->use_drm);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_g1_base_dec_get_config (GstG1BaseDec * g1dec, PPConfig * config)
{
  PPResult ppret;
  gboolean ret;

  g_return_val_if_fail (g1dec, FALSE);

  if (!g1dec->pp) {
    GST_DEBUG_OBJECT (g1dec,
        "postponing configuration until pipeline is started");
    ret = TRUE;
    goto exit;
  }

  ppret = PPGetConfig (g1dec->pp, config);
  if (GST_G1_PP_FAILED (ppret)) {
    GST_ERROR_OBJECT (g1dec, "Unable to retrieve post processor's config");
    ret = FALSE;
  } else {
    ret = TRUE;
  }

exit:
  {
    return ret;
  }
}

void
gst_g1_base_dec_config_format (GstG1BaseDec * dec, GstVideoFormatInfo * fmt,
    gint32 width, gint32 height)
{
  dec->ppconfig.ppInImg.pixFormat = gst_g1_format_gst_to_pp (fmt);
  dec->ppconfig.ppInImg.width = width;
  dec->ppconfig.ppInImg.height = height;

  /* Cropping depends on input format */
  gst_g1_base_dec_config_crop (dec, dec->crop_x, dec->crop_y,
      dec->crop_width, dec->crop_height);
}

static void
gst_g1_base_dec_config_rotation (GstG1BaseDec * g1dec, gint rotation)
{
  /* TODO: there are restrictions with the output format. Put safe
   * checks here */
  g1dec->rotation = rotation;
  g1dec->ppconfig.ppInRotation.rotation = g1dec->rotation;
}

static void
gst_g1_base_dec_config_brightness (GstG1BaseDec * g1dec, gint brightness)
{
  g1dec->brightness = brightness;
  g1dec->ppconfig.ppOutRgb.brightness = g1dec->brightness;
}

static void
gst_g1_base_dec_config_contrast (GstG1BaseDec * g1dec, gint contrast)
{
  g1dec->contrast = contrast;
  g1dec->ppconfig.ppOutRgb.contrast = g1dec->contrast;
}

static void
gst_g1_base_dec_config_saturation (GstG1BaseDec * g1dec, gint saturation)
{
  g1dec->saturation = saturation;
  g1dec->ppconfig.ppOutRgb.saturation = g1dec->saturation;
}

static void
gst_g1_base_dec_config_mask1 (GstG1BaseDec * g1dec,
    gchar * location, gint x, gint y, gint width, gint height)
{
  FILE *rgbfile = NULL;
  gsize rgbsize;
  gchar *tmplocation;

  if (location != -1) {
    tmplocation = g1dec->mask1_location;

    if (location) {
      g1dec->mask1_location = g_strdup (location);
    } else {
      g1dec->mask1_location = NULL;
    }
    if (tmplocation) {
      g_free (tmplocation);
    }
  }
  g_print ("Location2 = %p->%s\n", g1dec->mask1_location,
      g1dec->mask1_location);

  if (x != -1) {
    g1dec->mask1_x = x;
    g1dec->ppconfig.ppOutMask1.originX = g1dec->mask1_x;
    g1dec->ppconfig.ppOutMask1.blendOriginX = 0;
  }

  if (y != -1) {
    g1dec->mask1_y = y;
    g1dec->ppconfig.ppOutMask1.originY = g1dec->mask1_y;
    g1dec->ppconfig.ppOutMask1.blendOriginY = 0;
  }

  if (width != -1) {
    g1dec->mask1_width = width;
    g1dec->ppconfig.ppOutMask1.width = g1dec->mask1_width;
    g1dec->ppconfig.ppOutMask1.blendWidth = g1dec->mask1_width;
  }

  if (height != -1) {
    g1dec->mask1_height = height;
    g1dec->ppconfig.ppOutMask1.height = g1dec->mask1_height;
    g1dec->ppconfig.ppOutMask1.blendHeight = g1dec->mask1_height;
  }

  /* Check if we have enough info to configure ourselves */
  if (g1dec->mask1_location && g1dec->mask1_height && g1dec->mask1_width
      && g1dec->allocator) {
    printf ("DEBUG: %s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__);
    if (g1dec->mask1_mem) {
      printf ("DEBUG: %s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__);
      gst_allocator_free (g1dec->allocator, g1dec->mask1_mem);
      g1dec->mask1_mem = NULL;
    }

    rgbfile = fopen (g1dec->mask1_location, "r");
    if (!rgbfile) {
      GST_ERROR_OBJECT (g1dec, "unable to open mask1 %s: %s",
          g1dec->mask1_location, strerror (errno));
      goto exit;
    }
    printf ("DEBUG: %s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__);
    rgbsize = g1dec->mask1_width * g1dec->mask1_height * 4;
    g1dec->mask1_mem =
        (GstG1Memory *) gst_allocator_alloc (g1dec->allocator, rgbsize, NULL);
    printf ("DEBUG: %s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__);
    if (rgbsize != fread (g1dec->mask1_mem->virtaddress, 1, rgbsize, rgbfile)) {
      GST_ERROR_OBJECT (g1dec, "error reading mask1 %s", g1dec->mask1_location);
      gst_allocator_free (g1dec->allocator, (GstMemory *) g1dec->mask1_mem);
      g1dec->mask1_mem = NULL;
      goto exit;
    }

    printf ("DEBUG: %s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__);
    g1dec->ppconfig.ppOutMask1.enable = 1;
    g1dec->ppconfig.ppOutMask1.alphaBlendEna = 1;
    g1dec->ppconfig.ppOutMask1.blendComponentBase =
        g1dec->mask1_mem->physaddress;
  } else {
    g1dec->ppconfig.ppOutMask1.enable = 0;
    g1dec->ppconfig.ppOutMask1.alphaBlendEna = 0;
    g1dec->ppconfig.ppOutMask1.blendComponentBase = 0;
  }

exit:
  {
    if (rgbfile)
      fclose (rgbfile);
  }
  printf ("DEBUG: %s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__);
}

static void
gst_g1_base_dec_config_crop (GstG1BaseDec * g1dec,
    gint x, gint y, gint width, gint height)
{
  GstVideoCodecState *state;
  GstVideoInfo *vinfo;
  gint tmp;
  gint tmpres;
  gint ppinres;
  gboolean configured;

  state = gst_video_decoder_get_output_state (GST_VIDEO_DECODER (g1dec));
  vinfo = &state->info;
  ppinres = g1dec->ppconfig.ppInImg.width;

  if (!ppinres || !state)
    configured = FALSE;
  else
    configured = TRUE;

  /* Safe checks for crop:
     - (X,Y) aligned to 16
     - (Width, Height) aligned to 8
     - (X+Width,Y+Height) < (InWidth, InHeight)
     - 3*(Width, Height-2/3) >= (OutWidth, OutHeight)
   */
  if ((x != -1) && (x & 0xf)) {
    tmp = x & ~0xf;
    GST_WARNING_OBJECT (g1dec, "Crop X of %d is not a multiple of 16, "
        "forcing alignment to %d", x, tmp);
    x = tmp;
  }

  if ((y != -1) && (y & 0xf)) {
    tmp = y & ~0xf;
    GST_WARNING_OBJECT (g1dec, "Crop Y of %d is not a multiple of 16, "
        "forcing alignment to %d", y, tmp);
    y = tmp;
  }

  if ((width != -1) && (width & 0x7)) {
    tmp = width & ~0x7;
    GST_WARNING_OBJECT (g1dec, "Crop width of %d is not a multiple of 8, "
        "forcing alignment to %d", width, tmp);
    width = tmp;
  }

  if ((height != -1) && (height & 0x7)) {
    tmp = height & ~0x7;
    GST_WARNING_OBJECT (g1dec, "Crop height of %d is not a multiple of 8, "
        "forcing alignment to %d", height, tmp);
    height = tmp;
  }

  if (state && width && width != -1) {
    if (GST_VIDEO_INFO_WIDTH (vinfo)
        && ((3 * width) < GST_VIDEO_INFO_WIDTH (vinfo))) {
      GST_ERROR_OBJECT (g1dec,
          "crop width (%d) must be at least 1/3 of the output width (%d)",
          width, GST_VIDEO_INFO_WIDTH (vinfo));
      goto exit;
    }
  }

  if (state && height && height != -1) {
    if (GST_VIDEO_INFO_HEIGHT (vinfo)
        && ((3 * height - 2) < GST_VIDEO_INFO_HEIGHT (vinfo))) {
      GST_ERROR_OBJECT (g1dec,
          "crop height (%d) must be at least 1/3 of the output height (%d)",
          height, GST_VIDEO_INFO_HEIGHT (vinfo));
      goto exit;
    }
  }

  if ((x != -1) || (width != -1)) {
    tmp = x == -1 ? g1dec->crop_x : x;
    tmpres = width == -1 ? g1dec->crop_width : width;

    if (ppinres && (tmp + tmpres) > ppinres) {
      GST_ERROR_OBJECT (g1dec, "{(X+Width) = (%d+%d)} > {InWidth = %d}",
          tmp, tmpres, ppinres);
      goto exit;
    }
  }

  if ((y != -1) || (height != -1)) {
    ppinres = g1dec->ppconfig.ppInImg.height;
    tmp = y == -1 ? g1dec->crop_y : y;
    tmpres = height == -1 ? g1dec->crop_height : height;

    if (ppinres && (tmp + tmpres) > ppinres) {
      GST_ERROR_OBJECT (g1dec, "{(Y+Height) = (%d+%d)} > {InHeight = %d}",
          tmp, tmpres, ppinres);
      goto exit;
    }
  }

  g1dec->crop_x = x != -1 ? x : g1dec->crop_x;
  if (configured)
    g1dec->ppconfig.ppInCrop.originX = g1dec->crop_x;

  g1dec->crop_y = y != -1 ? y : g1dec->crop_y;
  if (configured)
    g1dec->ppconfig.ppInCrop.originY = g1dec->crop_y;

  g1dec->crop_width = width != -1 ? width : g1dec->crop_width;
  if (configured)
    g1dec->ppconfig.ppInCrop.width = g1dec->crop_width;

  g1dec->crop_height = height != -1 ? height : g1dec->crop_height;
  if (configured)
    g1dec->ppconfig.ppInCrop.height = g1dec->crop_height;

  if (!configured || !g1dec->crop_height || !g1dec->crop_width) {
    g1dec->ppconfig.ppInCrop.enable = 0;
  } else {
    g1dec->ppconfig.ppInCrop.enable = 1;
  }

exit:
  {
    if (state)
      gst_video_codec_state_unref (state);
  }
}

static gboolean
gst_g1_base_dec_setup_pp (GstG1BaseDec * g1dec)
{
  gboolean ret;

  /* Get initial pp configuration */
  if (!gst_g1_base_dec_get_config (g1dec, &g1dec->ppconfig)) {
    ret = FALSE;
    goto exit;
  }

  /* Force format parsing */
  g1dec->ppconfig.ppInImg.width = 0;
  g1dec->ppconfig.ppInImg.height = 0;
  g1dec->ppconfig.ppInImg.pixFormat = 0;

  gst_g1_base_dec_config_rotation (g1dec, g1dec->rotation);
  gst_g1_base_dec_config_brightness (g1dec, g1dec->brightness);
  gst_g1_base_dec_config_contrast (g1dec, g1dec->contrast);
  gst_g1_base_dec_config_saturation (g1dec, g1dec->saturation);
  gst_g1_base_dec_config_crop (g1dec, g1dec->crop_x, g1dec->crop_y,
      g1dec->crop_width, g1dec->crop_height);
  g_print ("Location3=%p %s\n", g1dec->mask1_location, g1dec->mask1_location);
  gst_g1_base_dec_config_mask1 (g1dec, g1dec->mask1_location, g1dec->mask1_x,
      g1dec->mask1_y, g1dec->mask1_width, g1dec->mask1_height);

  ret = TRUE;

exit:
  {
    return ret;
  }
}
