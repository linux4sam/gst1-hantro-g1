/* GStreamer GstFramebufferSink class
 * Copyright (C) 2013 Harm Hanemaaijer <fgenfb@yahoo.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:class-GstFramebufferSink
 *
 * The GstFramebufferSink class implements an optimized video sink
 * for framebuffer devices. It is used as the basis for the
 * fbdev2sink en drmsink plugins. It can manage multiple buffers,
 * writing directly into video memory with page flipping support, and
 * should be usable by a wide variety of devices. The class can be
 * derived for device specific implementations with optional support
 * for hardware scaling overlays.
 *
 * <refsect2>
 * <title>Property settings,<title>
 * <para>
 * The class comes with variety of configurable properties regulating
 * the size and frames per second of the video output, and various
 * options regulating the rendering method (including rendering directly
 * to video memory and page flipping).
 * </para>
 * </refsect2>
 * <refsect2>
 * <title>Caveats</title>
 * <para>
 * The actual implementation of the Linux framebuffer API varies between
 * systems, and methods beyond the most basic operating mode may not work
 * correctly on some systems. This primarily applies to page flipping
 * and vsync. The API implementation may be slower than expected on certain
 * hardware due to, for example, extra hidden vsyncs being performed in the
 * pan function. The "pan-does-vsync" option may help in that case.
 * </para>
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <inttypes.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <math.h>
#include <glib/gprintf.h>

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <gst/video/gstvideopool.h>
#include <gst/video/video.h>
#include <gst/video/video-info.h>
#include <gst/video/gstvideometa.h>
#include "gstframebuffersink.h"

GST_DEBUG_CATEGORY_STATIC (gst_framebuffersink_debug_category);
#define GST_CAT_DEFAULT gst_framebuffersink_debug_category

static GstVideoSinkClass *parent_class = NULL;

/* Definitions to influence buffer pool allocation.
  Provide another video memory pool for repeated requests. */
//#define MULTIPLE_VIDEO_MEMORY_POOLS
/* Provide half of the available video memory pool buffers per request. */
/* #define HALF_POOLS */

#define INCLUDE_PRESERVE_PAR_PROPERTY


/* Function to produce informational output if silent property is not set;
   if the silent property is set only debugging info is produced. */
static void
GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (GstFramebufferSink * framebuffersink,
    const gchar * message)
{
  if (!framebuffersink->silent)
    g_print ("%s.\n", message);
  GST_INFO_OBJECT (framebuffersink, message);
}

#define ALIGNMENT_GET_ALIGN_BYTES(offset, align) \
    (((align) + 1 - ((offset) & (align))) & (align))
#define ALIGNMENT_GET_ALIGNED(offset, align) \
    ((offset) + ALIGNMENT_GET_ALIGN_BYTES(offset, align))
#define ALIGNMENT_APPLY(offset, align) \
    offset = ALIGNMENT_GET_ALIGNED(offset, align);

/* Class function prototypes. */
static void gst_framebuffersink_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_framebuffersink_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static GstStateChangeReturn gst_framebuffersink_change_state (GstElement *
    element, GstStateChange transition);
static GstCaps *gst_framebuffersink_get_caps (GstBaseSink * sink,
    GstCaps * filter);
static gboolean gst_framebuffersink_set_caps (GstBaseSink * sink,
    GstCaps * caps);
static gboolean gst_framebuffersink_start (GstBaseSink * sink);
static gboolean gst_framebuffersink_stop (GstBaseSink * sink);
static GstFlowReturn gst_framebuffersink_show_frame (GstVideoSink * vsink,
    GstBuffer * buf);
static gboolean gst_framebuffersink_propose_allocation (GstBaseSink * sink,
    GstQuery * query);

/* Defaults for virtual functions defined in this class. */
static GstVideoFormat
    * gst_framebuffersink_get_supported_overlay_formats (GstFramebufferSink *
    framebuffersink);
static gboolean gst_framebuffersink_open_hardware (GstFramebufferSink *
    framebuffersink, GstVideoInfo * info, gsize * video_memory_size,
    gsize * pannable_video_memory_size);
static void gst_framebuffersink_close_hardware (GstFramebufferSink *
    framebuffersink);
static GstAllocator
    * gst_framebuffersink_video_memory_allocator_new (GstFramebufferSink *
    framebuffersink, GstVideoInfo * info, gboolean pannable,
    gboolean is_overlay);
static void gst_framebuffersink_pan_display (GstFramebufferSink *
    framebuffersink, GstMemory * memory);
static void gst_framebuffersink_wait_for_vsync (GstFramebufferSink *
    framebuffersink);

/* Video memory. */
static gboolean gst_framebuffersink_is_video_memory (GstFramebufferSink *
    framebuffersink, GstMemory * mem);

enum
{
  PROP_0,
  PROP_SILENT,
  PROP_DEVICE,
  PROP_ACTUAL_WIDTH,
  PROP_ACTUAL_HEIGHT,
  PROP_REQUESTED_WIDTH,
  PROP_REQUESTED_HEIGHT,
  PROP_SCREEN_WIDTH,
  PROP_SCREEN_HEIGHT,
  PROP_WIDTH_BEFORE_SCALING,
  PROP_HEIGHT_BEFORE_SCALING,
  PROP_FULL_SCREEN,
#ifdef INCLUDE_PRESERVE_PAR_PROPERTY
  PROP_PRESERVE_PAR,
#endif
  PROP_CLEAR,
  PROP_FRAMES_PER_SECOND,
  PROP_BUFFER_POOL,
  PROP_VSYNC,
  PROP_FLIP_BUFFERS,
  PROP_PAN_DOES_VSYNC,
  PROP_USE_HARDWARE_OVERLAY,
  PROP_MAX_VIDEO_MEMORY_USED,
  PROP_OVERLAY_FORMAT,
  PROP_BENCHMARK,
};

/* pad templates */

#define GST_FRAMEBUFFERSINK_TEMPLATE_CAPS \
        GST_VIDEO_CAPS_MAKE ("RGB") \
        "; " GST_VIDEO_CAPS_MAKE ("BGR") \
        "; " GST_VIDEO_CAPS_MAKE ("RGBx") \
        "; " GST_VIDEO_CAPS_MAKE ("BGRx") \
        "; " GST_VIDEO_CAPS_MAKE ("xRGB") \
        "; " GST_VIDEO_CAPS_MAKE ("xBGR") ", " \
        "framerate = (fraction) [ 0, MAX ], " \
        "width = (int) [ 1, MAX ], " "height = (int) [ 1, MAX ]"


static GstStaticPadTemplate gst_framebuffersink_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_FRAMEBUFFERSINK_TEMPLATE_CAPS)
    );

static GstVideoFormat overlay_formats_supported_table_empty[] = {
  GST_VIDEO_FORMAT_UNKNOWN
};

/* Class initialization. */

static void
gst_framebuffersink_class_init (GstFramebufferSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSinkClass *base_sink_class = GST_BASE_SINK_CLASS (klass);
  GstVideoSinkClass *video_sink_class = GST_VIDEO_SINK_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->set_property = gst_framebuffersink_set_property;
  gobject_class->get_property = gst_framebuffersink_get_property;

  /* define properties */
  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Reduce messages",
          "Whether to be very verbose or not",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DEVICE,
      g_param_spec_string ("device", "The device",
          "The device to access the framebuffer",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_ACTUAL_WIDTH,
      g_param_spec_int ("actual-width", "Actual source video width",
          "Actual width of the video window source",
          0, G_MAXINT, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_ACTUAL_HEIGHT,
      g_param_spec_int ("actual-height", "Actual source video height",
          "Actual height of the video window source",
          0, G_MAXINT, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_REQUESTED_WIDTH,
      g_param_spec_int ("width", "Requested width",
          "Requested width of the video output window (0 = auto)",
          0, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_REQUESTED_HEIGHT,
      g_param_spec_int ("height", "Requested height",
          "Requested height of the video output window (0 = auto)",
          0, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SCREEN_WIDTH,
      g_param_spec_int ("screen-width", "Screen width",
          "Width of the screen", 1, G_MAXINT,
          1, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SCREEN_HEIGHT,
      g_param_spec_int ("screen-height", "Screen height",
          "Height of the screen",
          1, G_MAXINT, 1, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_WIDTH_BEFORE_SCALING,
      g_param_spec_int ("width-before-scaling",
          "Requested source width before scaling",
          "Requested width of the video source when using hardware scaling "
          "(0 = use default source width)",
          0, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_HEIGHT_BEFORE_SCALING,
      g_param_spec_int ("height-before-scaling",
          "Requested source height before scaling",
          "Requested height of the video source when using hardware scaling "
          "(0 = use default source height)",
          0, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_FULL_SCREEN,
      g_param_spec_boolean ("full-screen", "Full-screen output",
          "Force full-screen video output resolution "
          "(equivalent to setting width and "
          "height to screen dimensions)",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#ifdef INCLUDE_PRESERVE_PAR_PROPERTY
  g_object_class_install_property (gobject_class, PROP_PRESERVE_PAR,
      g_param_spec_boolean ("preserve-par", "Preserve pixel aspect ratio",
          "Preserve the pixel aspect ratio by adding black boxes if necessary. "
          "Only works if hardware scaling can be used.",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#endif
  g_object_class_install_property (gobject_class, PROP_CLEAR,
      g_param_spec_boolean ("clear", "Clear the screen",
          "Clear the screen to black before playing",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_FRAMES_PER_SECOND,
      g_param_spec_int ("fps", "Frames per second",
          "Frames per second (0 = auto)", 0, G_MAXINT,
          0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_BUFFER_POOL,
      g_param_spec_boolean ("buffer-pool", "Use buffer pool",
          "Use a custom buffer pool in video memory and write directly to the "
          "screen if possible",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_VSYNC,
      g_param_spec_boolean ("vsync", "VSync",
          "Sync to vertical retrace. Especially useful with buffer-pool=true.",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_FLIP_BUFFERS,
      g_param_spec_int ("flip-buffers", "Max number of page-flip buffers",
          "The maximum number of buffers in video memory to use for page "
          "flipping. Page flipping is disabled when set to 1. Use of a "
          "buffer-pool requires at least 2 buffers. Default is 0 (auto).",
          0, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PAN_DOES_VSYNC,
      g_param_spec_boolean ("pan-does-vsync", "Pan does vsync indicator",
          "When set to true this property hints that the kernel display pan "
          "function performs vsync automatically or otherwise doesn't need a "
          "vsync call around it.",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_USE_HARDWARE_OVERLAY,
      g_param_spec_boolean ("hardware-overlay", "Use hardware overlay",
          "Use hardware overlay scaler if available. Not available in the "
          "default fbdev2sink but may be available in derived sinks.",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MAX_VIDEO_MEMORY_USED,
      g_param_spec_int ("video-memory", "Max video memory used in MB",
          "The maximum amount of video memory to use in MB. Three special "
          "values are defined: 0 (the default) limits the amount to the "
          "virtual resolution as reported by the Linux fb interface; -1 "
          "uses up to all available video memory as reported by the fb "
          "interface but sets sane limits; -2 aggressively uses all "
          "available memory.",
          -2, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_OVERLAY_FORMAT,
      g_param_spec_string ("overlay-format", "Overlay format",
          "Set the preferred overlay format (four character code); by default "
          "the standard rank order provided by the plugin will be applied",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_BENCHMARK,
      g_param_spec_boolean ("benchmark", "Benchmark video memory",
          "Perform video memory benchmarks at start-up",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_framebuffersink_change_state);
  base_sink_class->start = GST_DEBUG_FUNCPTR (gst_framebuffersink_start);
  base_sink_class->stop = GST_DEBUG_FUNCPTR (gst_framebuffersink_stop);
  base_sink_class->get_caps = GST_DEBUG_FUNCPTR (gst_framebuffersink_get_caps);
  base_sink_class->set_caps = GST_DEBUG_FUNCPTR (gst_framebuffersink_set_caps);
  base_sink_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_framebuffersink_propose_allocation);
  video_sink_class->show_frame =
      GST_DEBUG_FUNCPTR (gst_framebuffersink_show_frame);
  klass->open_hardware = GST_DEBUG_FUNCPTR (gst_framebuffersink_open_hardware);
  klass->close_hardware =
      GST_DEBUG_FUNCPTR (gst_framebuffersink_close_hardware);
  klass->video_memory_allocator_new =
      GST_DEBUG_FUNCPTR (gst_framebuffersink_video_memory_allocator_new);
  klass->pan_display = GST_DEBUG_FUNCPTR (gst_framebuffersink_pan_display);
  klass->wait_for_vsync =
      GST_DEBUG_FUNCPTR (gst_framebuffersink_wait_for_vsync);
  klass->get_supported_overlay_formats =
      GST_DEBUG_FUNCPTR (gst_framebuffersink_get_supported_overlay_formats);
}

static void
gst_framebuffersink_base_init (gpointer g_class)
{
  GST_DEBUG_CATEGORY_INIT (gst_framebuffersink_debug_category,
      "framebuffersink", 0, "GstFramebufferSink");
}

/* Class member functions. */

static void
gst_framebuffersink_init (GstFramebufferSink * framebuffersink)
{
  framebuffersink->pool = NULL;
  framebuffersink->caps = NULL;
  /* This will set the format to GST_VIDEO_FORMAT_UNKNOWN. */
  gst_video_info_init (&framebuffersink->screen_info);
  gst_video_info_init (&framebuffersink->video_info);

  /* Set the pixel aspect ratio of the display device to 1:1. */
  GST_VIDEO_INFO_PAR_N (&framebuffersink->screen_info) = 1;
  GST_VIDEO_INFO_PAR_D (&framebuffersink->screen_info) = 1;

  /* Set the initial values of the properties. */
  framebuffersink->device = NULL;
  framebuffersink->videosink.width = 0;
  framebuffersink->videosink.height = 0;
  framebuffersink->silent = FALSE;
  framebuffersink->full_screen = FALSE;
  framebuffersink->requested_video_width = 0;
  framebuffersink->requested_video_height = 0;
  framebuffersink->width_before_scaling = 0;
  framebuffersink->height_before_scaling = 0;
  framebuffersink->clear = TRUE;
  framebuffersink->fps = 0;
  framebuffersink->use_buffer_pool_property = FALSE;
  framebuffersink->vsync_property = TRUE;
  framebuffersink->flip_buffers = 0;
  framebuffersink->pan_does_vsync = FALSE;
  framebuffersink->use_hardware_overlay_property = TRUE;
#ifdef INCLUDE_PRESERVE_PAR_PROPERTY
  /* When the preserve_par property is available, it will default true. */
  framebuffersink->preserve_par = TRUE;
#else
  framebuffersink->preserve_par = FALSE;
#endif
  framebuffersink->max_video_memory_property = 0;
  framebuffersink->preferred_overlay_format_str = NULL;
  framebuffersink->benchmark = FALSE;
}

/* Default implementation of hardware open/close functions. */

static gboolean
gst_framebuffersink_open_hardware (GstFramebufferSink * framebuffersink,
    GstVideoInfo * info, gsize * video_memory_size, gsize *
    pannable_video_memory_size)
{
  *video_memory_size = 0;
  *pannable_video_memory_size = 0;
  return TRUE;
}

static void
gst_framebuffersink_close_hardware (GstFramebufferSink * framebuffersink)
{
}

/* Default implementation of pan_display. */

static void
gst_framebuffersink_pan_display (GstFramebufferSink * framebuffersink,
    GstMemory * memory)
{
}

/* Default implementation of wait_for_vsync. */

static void
gst_framebuffersink_wait_for_vsync (GstFramebufferSink * framebuffersink)
{
}

/* Default implementation of get_supported_overlay_formats: none supported. */

static GstVideoFormat *
gst_framebuffersink_get_supported_overlay_formats (GstFramebufferSink *
    framebuffersink)
{
  return overlay_formats_supported_table_empty;
}

/* Default implementation of video_memory_allocator_new. */

static GstAllocator *
gst_framebuffersink_video_memory_allocator_new (GstFramebufferSink *
    framebuffersink, GstVideoInfo * info, gboolean pannable,
    gboolean is_overlay)
{
  return NULL;
}

static gboolean
gst_framebuffersink_video_format_supported_by_overlay (GstFramebufferSink *
    framebuffersink, GstVideoFormat format)
{
  GstVideoFormat *f = framebuffersink->overlay_formats_supported;
  while (*f != GST_VIDEO_FORMAT_UNKNOWN) {
    if (*f == format)
      return TRUE;
    f++;
  }
  return FALSE;
}

static int
gst_framebuffersink_get_overlay_format_rank (GstFramebufferSink *
    framebuffersink, GstVideoFormat format)
{
  GstVideoFormat *f = framebuffersink->overlay_formats_supported;
  int r = 0;
  while (*f != GST_VIDEO_FORMAT_UNKNOWN) {
    if (*f == format)
      return r;
    f++;
    r++;
  }
  return G_MAXINT;
}

static void
gst_framebuffersink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstFramebufferSink *framebuffersink = GST_FRAMEBUFFERSINK (object);

  GST_DEBUG_OBJECT (framebuffersink, "set_property");
  g_return_if_fail (GST_IS_FRAMEBUFFERSINK (object));

  switch (property_id) {
    case PROP_SILENT:
      framebuffersink->silent = g_value_get_boolean (value);
      break;
    case PROP_DEVICE:
      g_free (framebuffersink->device);
      framebuffersink->device = g_value_dup_string (value);
      break;
    case PROP_REQUESTED_WIDTH:
      framebuffersink->requested_video_width = g_value_get_int (value);
      break;
    case PROP_REQUESTED_HEIGHT:
      framebuffersink->requested_video_height = g_value_get_int (value);
      break;
    case PROP_WIDTH_BEFORE_SCALING:
      framebuffersink->width_before_scaling = g_value_get_int (value);
      break;
    case PROP_HEIGHT_BEFORE_SCALING:
      framebuffersink->height_before_scaling = g_value_get_int (value);
      break;
    case PROP_FULL_SCREEN:
      framebuffersink->full_screen = g_value_get_boolean (value);
      break;
#ifdef INCLUDE_PRESERVE_PAR_PROPERTY
    case PROP_PRESERVE_PAR:
      framebuffersink->preserve_par = g_value_get_boolean (value);
      break;
#endif
    case PROP_CLEAR:
      framebuffersink->clear = g_value_get_boolean (value);
      break;
    case PROP_FRAMES_PER_SECOND:
      framebuffersink->fps = g_value_get_int (value);
      break;
    case PROP_BUFFER_POOL:
      framebuffersink->use_buffer_pool_property = g_value_get_boolean (value);
      break;
    case PROP_VSYNC:
      framebuffersink->vsync_property = g_value_get_boolean (value);
      break;
    case PROP_FLIP_BUFFERS:
      framebuffersink->flip_buffers = g_value_get_int (value);
      break;
    case PROP_PAN_DOES_VSYNC:
      framebuffersink->pan_does_vsync = g_value_get_boolean (value);
      break;
    case PROP_USE_HARDWARE_OVERLAY:
      framebuffersink->use_hardware_overlay_property =
          g_value_get_boolean (value);
      break;
    case PROP_MAX_VIDEO_MEMORY_USED:
      framebuffersink->max_video_memory_property = g_value_get_int (value);
      break;
    case PROP_OVERLAY_FORMAT:
      if (framebuffersink->preferred_overlay_format_str != NULL)
        g_free (framebuffersink->preferred_overlay_format_str);
      framebuffersink->preferred_overlay_format_str =
          g_value_dup_string (value);
      break;
    case PROP_BENCHMARK:
      framebuffersink->benchmark = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_framebuffersink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstFramebufferSink *framebuffersink = GST_FRAMEBUFFERSINK (object);

  GST_DEBUG_OBJECT (framebuffersink, "get_property");
  g_return_if_fail (GST_IS_FRAMEBUFFERSINK (object));

  switch (property_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, framebuffersink->silent);
      break;
    case PROP_DEVICE:
      g_value_set_string (value, framebuffersink->device);
      break;
    case PROP_ACTUAL_WIDTH:
      g_value_set_int (value, framebuffersink->videosink.width);
      break;
    case PROP_ACTUAL_HEIGHT:
      g_value_set_int (value, framebuffersink->videosink.height);
      break;
    case PROP_REQUESTED_WIDTH:
      g_value_set_int (value, framebuffersink->requested_video_width);
      break;
    case PROP_REQUESTED_HEIGHT:
      g_value_set_int (value, framebuffersink->requested_video_height);
      break;
    case PROP_SCREEN_WIDTH:
      g_value_set_int (value,
          GST_VIDEO_INFO_WIDTH (&framebuffersink->screen_info));
      break;
    case PROP_SCREEN_HEIGHT:
      g_value_set_int (value,
          GST_VIDEO_INFO_HEIGHT (&framebuffersink->screen_info));
      break;
    case PROP_WIDTH_BEFORE_SCALING:
      g_value_set_int (value, framebuffersink->width_before_scaling);
      break;
    case PROP_HEIGHT_BEFORE_SCALING:
      g_value_set_int (value, framebuffersink->height_before_scaling);
      break;
    case PROP_FULL_SCREEN:
      g_value_set_boolean (value, framebuffersink->full_screen);
      break;
#ifdef INCLUDE_PRESERVE_PAR_PROPERTY
    case PROP_PRESERVE_PAR:
      g_value_set_boolean (value, framebuffersink->preserve_par);
      break;
#endif
    case PROP_CLEAR:
      g_value_set_boolean (value, framebuffersink->clear);
      break;
    case PROP_FRAMES_PER_SECOND:
      g_value_set_int (value, framebuffersink->fps);
      break;
    case PROP_BUFFER_POOL:
      g_value_set_boolean (value, framebuffersink->use_buffer_pool_property);
      break;
    case PROP_VSYNC:
      g_value_set_boolean (value, framebuffersink->vsync_property);
      break;
    case PROP_FLIP_BUFFERS:
      g_value_set_int (value, framebuffersink->flip_buffers);
      break;
    case PROP_PAN_DOES_VSYNC:
      g_value_set_boolean (value, framebuffersink->pan_does_vsync);
      break;
    case PROP_USE_HARDWARE_OVERLAY:
      g_value_set_boolean (value,
          framebuffersink->use_hardware_overlay_property);
      break;
    case PROP_MAX_VIDEO_MEMORY_USED:
      g_value_set_int (value, framebuffersink->max_video_memory_property);
      break;
    case PROP_OVERLAY_FORMAT:
      g_value_set_string (value, framebuffersink->preferred_overlay_format_str);
      break;
    case PROP_BENCHMARK:
      g_value_set_boolean (value, framebuffersink->benchmark);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_framebuffersink_clear_screen (GstFramebufferSink * framebuffersink,
    int index)
{
  GstMapInfo mapinfo;
  gboolean res;

  mapinfo.data = NULL;
  res =
      gst_memory_map (framebuffersink->screens[index], &mapinfo, GST_MAP_WRITE);
  if (!res || mapinfo.data == NULL) {
    GST_ERROR_OBJECT (framebuffersink, "Could not map video memory");
    if (res)
      gst_memory_unmap (framebuffersink->screens[index], &mapinfo);
    return;
  }
  memset (mapinfo.data, 0, mapinfo.size);
  gst_memory_unmap (framebuffersink->screens[index], &mapinfo);
}

static void
gst_framebuffersink_put_image_memcpy (GstFramebufferSink * framebuffersink,
    uint8_t * src)
{
  guint8 *dest;
  guintptr dest_stride;
  int i;
  GstMapInfo mapinfo;
  gboolean res;

  mapinfo.data = NULL;
  res =
      gst_memory_map (framebuffersink->
      screens[framebuffersink->current_framebuffer_index], &mapinfo,
      GST_MAP_WRITE);
  if (!res || mapinfo.data == NULL) {
    GST_ERROR_OBJECT (framebuffersink, "Could not map video memory");
    if (res)
      gst_memory_unmap (framebuffersink->
          screens[framebuffersink->current_framebuffer_index], &mapinfo);
    return;
  }
  dest = mapinfo.data;
  dest +=
      framebuffersink->video_rectangle.y *
      GST_VIDEO_INFO_COMP_STRIDE (&framebuffersink->screen_info, 0)
      +
      framebuffersink->video_rectangle.x *
      GST_VIDEO_INFO_COMP_PSTRIDE (&framebuffersink->screen_info, 0);
  dest_stride = GST_VIDEO_INFO_COMP_STRIDE (&framebuffersink->screen_info, 0);
  if (framebuffersink->video_rectangle_width_in_bytes == dest_stride)
    memcpy (dest, src, dest_stride * framebuffersink->video_rectangle.h);
  else
    for (i = 0; i < framebuffersink->video_rectangle.h; i++) {
      memcpy (dest, src, framebuffersink->video_rectangle_width_in_bytes);
      src += framebuffersink->source_video_width_in_bytes[0];
      dest += dest_stride;
    }
  gst_memory_unmap (framebuffersink->
      screens[framebuffersink->current_framebuffer_index], &mapinfo);
  return;
}

static void
gst_framebuffersink_put_overlay_image_memcpy (GstFramebufferSink *
    framebuffersink, GstMemory * vmem, uint8_t * src)
{
  GstFramebufferSinkClass *klass =
      GST_FRAMEBUFFERSINK_GET_CLASS (framebuffersink);
  uint8_t *framebuffer_address;
  GstMapInfo mapinfo;
  gboolean res;

  mapinfo.data = NULL;
  res = gst_memory_map (vmem, &mapinfo, GST_MAP_WRITE);
  if (!res || mapinfo.data == NULL) {
    GST_ERROR_OBJECT (framebuffersink, "Could not map video memory");
    if (res)
      gst_memory_unmap (vmem, &mapinfo);
    return;
  }
  framebuffer_address = mapinfo.data;
  if (framebuffersink->overlay_alignment_is_native)
    memcpy (framebuffer_address, src, framebuffersink->video_info.size);
  else {
    int i;
    int n = GST_VIDEO_INFO_N_PLANES (&framebuffersink->video_info);
    guintptr offset;
    for (i = 0; i < n; i++) {
      offset = framebuffersink->overlay_plane_offset[i];
      if (GST_VIDEO_INFO_PLANE_STRIDE (&framebuffersink->video_info, i) ==
          framebuffersink->overlay_scanline_stride[i])
        memcpy (framebuffer_address + offset, src,
            framebuffersink->overlay_scanline_stride[i]
            * framebuffersink->videosink.height);
      else {
        int y;
        for (y = 0; y < framebuffersink->videosink.height; y++) {
          memcpy (framebuffer_address + offset +
              framebuffersink->overlay_scanline_offset[i],
              src, framebuffersink->source_video_width_in_bytes[i]);
          offset += framebuffersink->overlay_scanline_stride[i];
        }
      }
    }
  }
  gst_memory_unmap (vmem, &mapinfo);
  klass->show_overlay (framebuffersink, vmem);
}


static void
gst_framebuffersink_put_image_pan (GstFramebufferSink * framebuffersink,
    GstMemory * memory)
{
  GstFramebufferSinkClass *klass =
      GST_FRAMEBUFFERSINK_GET_CLASS (framebuffersink);
  if (framebuffersink->vsync && !framebuffersink->pan_does_vsync)
    klass->wait_for_vsync (framebuffersink);
  klass->pan_display (framebuffersink, memory);
}

/* Benchmark functionality. */

static void
clear_words (uint32_t * dest, gsize size)
{
  while (size >= 32) {
    *dest = 0;
    *(dest + 1) = 0;
    *(dest + 2) = 0;
    *(dest + 3) = 0;
    *(dest + 4) = 0;
    *(dest + 5) = 0;
    size -= 32;
    *(dest + 6) = 0;
    *(dest + 7) = 0;
    dest += 8;
  }
  while (size >= 4) {
    *dest = 0;
    size -= 4;
    dest++;
  }
}

static void
gst_framebuffersink_benchmark_clear_first_memset (GstFramebufferSink *
    framebuffersink, GstMemory ** buffers, int nu_buffers,
    GstMemory * source_buffer)
{
  GstMapInfo mapinfo;
  gst_memory_map (buffers[0], &mapinfo, GST_MAP_WRITE);
  memset (mapinfo.data, 0, mapinfo.size);
  gst_memory_unmap (buffers[0], &mapinfo);
}

static void
gst_framebuffersink_benchmark_clear_first_words (GstFramebufferSink *
    framebuffersink, GstMemory ** buffers, int nu_buffers,
    GstMemory * source_buffer)
{
  GstMapInfo mapinfo;
  uint32_t *dest;
  int size = GST_VIDEO_INFO_SIZE (&framebuffersink->screen_info);
  gst_memory_map (buffers[0], &mapinfo, GST_MAP_WRITE);
  dest = (uint32_t *) mapinfo.data;
  clear_words (dest, size);
  gst_memory_unmap (buffers[0], &mapinfo);
}

/* Test write combining. */

static void
gst_framebuffersink_benchmark_clear_first_bytes (GstFramebufferSink *
    framebuffersink, GstMemory ** buffers, int nu_buffers,
    GstMemory * source_buffer)
{
  GstMapInfo mapinfo;
  uint8_t *dest;
  int size = GST_VIDEO_INFO_SIZE (&framebuffersink->screen_info);
  gst_memory_map (buffers[0], &mapinfo, GST_MAP_WRITE);
  dest = mapinfo.data;
  while (size >= 16) {
    *dest = 0;
    *(dest + 1) = 0;
    *(dest + 2) = 0;
    *(dest + 3) = 0;
    *(dest + 4) = 0;
    *(dest + 5) = 0;
    *(dest + 6) = 0;
    *(dest + 7) = 0;
    *(dest + 8) = 0;
    *(dest + 9) = 0;
    *(dest + 10) = 0;
    *(dest + 11) = 0;
    *(dest + 12) = 0;
    *(dest + 13) = 0;
    size -= 16;
    *(dest + 14) = 0;
    *(dest + 15) = 0;
    dest += 16;
  }
  while (size > 0) {
    *dest = 0;
    size--;
    dest++;
  }
  gst_memory_unmap (buffers[0], &mapinfo);
}

/* Test read access. */

static void
gst_framebuffersink_benchmark_read_first_words (GstFramebufferSink *
    framebuffersink, GstMemory ** buffers, int nu_buffers,
    GstMemory * source_buffer)
{
  GstMapInfo mapinfo;
  uint32_t *src;
  uint32_t sum = 0;
  int size = GST_VIDEO_INFO_SIZE (&framebuffersink->screen_info);
  gst_memory_map (buffers[0], &mapinfo, GST_MAP_READ);
  src = (uint32_t *) mapinfo.data;
  while (size >= 32) {
    sum += *src;
    sum += *(src + 1);
    sum += *(src + 2);
    sum += *(src + 3);
    sum += *(src + 4);
    sum += *(src + 5);
    sum += *(src + 6);
    size -= 32;
    sum += *(src + 7);
    src += 8;
  }
  while (size >= 4) {
    sum += *src;
    size -= 4;
    src++;
  }
  *(uint32_t *) mapinfo.data = sum;
  gst_memory_unmap (buffers[0], &mapinfo);
}

static void
gst_framebuffersink_benchmark_clear_all_words (GstFramebufferSink *
    framebuffersink, GstMemory ** buffers, int nu_buffers,
    GstMemory * source_buffer)
{
  GstMapInfo mapinfo;
  uint32_t *dest;
  int size = GST_VIDEO_INFO_SIZE (&framebuffersink->screen_info);
  int i;
  for (i = 0; i < nu_buffers; i++) {
    gst_memory_map (buffers[i], &mapinfo, GST_MAP_WRITE);
    dest = (uint32_t *) mapinfo.data;
    clear_words (dest, size);
    gst_memory_unmap (buffers[i], &mapinfo);
  }
}

static void
gst_framebuffersink_benchmark_copy_first_memcpy (GstFramebufferSink *
    framebuffersink, GstMemory ** buffers, int nu_buffers,
    GstMemory * source_buffer)
{
  GstMapInfo mapinfo;
  GstMapInfo mapinfo_src;
  int size = GST_VIDEO_INFO_SIZE (&framebuffersink->screen_info);
  gst_memory_map (buffers[0], &mapinfo, GST_MAP_WRITE);
  gst_memory_map (source_buffer, &mapinfo_src, GST_MAP_READ);
  memcpy (mapinfo.data, mapinfo_src.data, size);
  gst_memory_unmap (source_buffer, &mapinfo_src);
  gst_memory_unmap (buffers[0], &mapinfo);
}

/* Copy multiple system memory buffers to a single destination buffer.
   The source buffer reverses roles as destination buffer. */

static void
gst_framebuffersink_benchmark_copy_n_to_source_memcpy (GstFramebufferSink *
    framebuffersink, GstMemory ** buffers, int nu_buffers,
    GstMemory * source_buffer)
{
  GstMapInfo mapinfo;
  GstMapInfo mapinfo_src;
  int i;
  int size = GST_VIDEO_INFO_SIZE (&framebuffersink->screen_info);
  for (i = 0; i < nu_buffers; i++) {
    gst_memory_map (buffers[0], &mapinfo, GST_MAP_READ);
    gst_memory_map (source_buffer, &mapinfo_src, GST_MAP_WRITE);
    memcpy (mapinfo_src.data, mapinfo.data, size);
    gst_memory_unmap (source_buffer, &mapinfo_src);
    gst_memory_unmap (buffers[0], &mapinfo);
  }
}

static void
gst_framebuffersink_benchmark_operation (GstFramebufferSink * framebuffersink,
    GstMemory ** buffers, int nu_buffers, GstMemory * source_buffer,
    const gchar * benchmark_name,
    void (*benchmark_operation) (GstFramebufferSink * framebuffersink,
        GstMemory ** buffers, int nu_buffers, GstMemory * source_buffer),
    gsize bytes)
{
  struct timeval tv_start, tv_end, tv_elapsed;
  int n = 0;
  double elapsed_secs;

  benchmark_operation (framebuffersink, buffers, nu_buffers, source_buffer);

  gettimeofday (&tv_start, NULL);

  for (;;) {
    int i;
    for (i = 0; i < 4; i++)
      benchmark_operation (framebuffersink, buffers, nu_buffers, source_buffer);
    n += 4;

    gettimeofday (&tv_end, NULL);
    timersub (&tv_end, &tv_start, &tv_elapsed);
    if (tv_elapsed.tv_sec >= 1)
      break;
  }

  elapsed_secs =
      (double) tv_elapsed.tv_sec + (double) tv_elapsed.tv_usec / 1000000;
  g_print ("Benchmark: %-32s %7.2lf MB/s  %6.1lf fps\n", benchmark_name,
      bytes * n / (elapsed_secs * 1024 * 1024), bytes * n / (elapsed_secs *
          GST_VIDEO_INFO_SIZE (&framebuffersink->screen_info)));
}

static void
gst_framebuffersink_benchmark (GstFramebufferSink * framebuffersink)
{
  GstMemory **buffers;
  GstMemory *system_buffers[8];
  GstMemory *source_buffer;
  GstAllocator *default_allocator;
  int i;
  int n = framebuffersink->max_framebuffers;
  buffers = g_slice_alloc (sizeof (GstMemory *) *
      framebuffersink->max_framebuffers);
  for (i = 0; i < framebuffersink->max_framebuffers; i++) {
    buffers[i] =
        gst_allocator_alloc (framebuffersink->screen_video_memory_allocator,
        GST_VIDEO_INFO_SIZE (&framebuffersink->screen_info), NULL);
    if (buffers[i] == NULL) {
      n = i;
      break;
    }
  }
  if (n == 0) {
    GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink,
        "Could not allocate buffers for benchmark");
    goto no_buffers;
  }

  default_allocator = gst_allocator_find (NULL);
  source_buffer =
      gst_allocator_alloc (default_allocator,
      GST_VIDEO_INFO_SIZE (&framebuffersink->screen_info), NULL);

  /* Perform a read operation and write operation to warm up. */
  gst_framebuffersink_benchmark_read_first_words (framebuffersink, buffers, n,
      source_buffer);
  gst_framebuffersink_benchmark_clear_first_words (framebuffersink, buffers, n,
      source_buffer);

  gst_framebuffersink_benchmark_operation (framebuffersink, buffers, n,
      source_buffer, "Clear first buffer (memset)",
      gst_framebuffersink_benchmark_clear_first_memset,
      GST_VIDEO_INFO_SIZE (&framebuffersink->screen_info));
  gst_framebuffersink_benchmark_operation (framebuffersink, buffers, n,
      source_buffer, "Clear first buffer (words)",
      gst_framebuffersink_benchmark_clear_first_words,
      GST_VIDEO_INFO_SIZE (&framebuffersink->screen_info));
  gst_framebuffersink_benchmark_operation (framebuffersink, buffers, n,
      source_buffer, "Clear first buffer (bytes)",
      gst_framebuffersink_benchmark_clear_first_bytes,
      GST_VIDEO_INFO_SIZE (&framebuffersink->screen_info));
  gst_framebuffersink_benchmark_operation (framebuffersink, buffers, n,
      source_buffer, "Read first buffer (words)",
      gst_framebuffersink_benchmark_read_first_words,
      GST_VIDEO_INFO_SIZE (&framebuffersink->screen_info));
  gst_framebuffersink_benchmark_operation (framebuffersink, buffers, n,
      source_buffer, "Clear all buffers (words)",
      gst_framebuffersink_benchmark_clear_all_words,
      GST_VIDEO_INFO_SIZE (&framebuffersink->screen_info) * n);
  gst_framebuffersink_benchmark_operation (framebuffersink, buffers, n,
      source_buffer, "Copy system to video (memcpy)",
      gst_framebuffersink_benchmark_copy_first_memcpy,
      GST_VIDEO_INFO_SIZE (&framebuffersink->screen_info));

  for (i = 0; i < 8; i++)
    system_buffers[i] = gst_allocator_alloc (default_allocator,
        GST_VIDEO_INFO_SIZE (&framebuffersink->screen_info), NULL);

  gst_framebuffersink_benchmark_operation (framebuffersink, system_buffers, 8,
      source_buffer, "Clear system memory (words)",
      gst_framebuffersink_benchmark_clear_first_words,
      GST_VIDEO_INFO_SIZE (&framebuffersink->screen_info));
  gst_framebuffersink_benchmark_operation (framebuffersink, system_buffers, 8,
      source_buffer, "Read system memory (words)",
      gst_framebuffersink_benchmark_read_first_words,
      GST_VIDEO_INFO_SIZE (&framebuffersink->screen_info));
  gst_framebuffersink_benchmark_operation (framebuffersink, system_buffers, 8,
      source_buffer, "Clear 8 system buffers (words)",
      gst_framebuffersink_benchmark_clear_all_words,
      GST_VIDEO_INFO_SIZE (&framebuffersink->screen_info) * 8);
  gst_framebuffersink_benchmark_operation (framebuffersink, system_buffers, 8,
      source_buffer, "Copy 8 system to system (memcpy)",
      gst_framebuffersink_benchmark_copy_n_to_source_memcpy,
      GST_VIDEO_INFO_SIZE (&framebuffersink->screen_info) * 8);

  for (i = 0; i < 8; i++)
    gst_allocator_free (default_allocator, system_buffers[i]);

  gst_allocator_free (default_allocator, source_buffer);
  gst_object_unref (default_allocator);

  for (i = 0; i < n; i++)
    gst_allocator_free (framebuffersink->screen_video_memory_allocator,
        buffers[i]);

no_buffers:
  g_slice_free1 (sizeof (GstMemory *) * framebuffersink->max_framebuffers,
      buffers);
}

/* Start function, called when resources should be allocated. */

static gboolean
gst_framebuffersink_start (GstBaseSink * sink)
{
  GstFramebufferSink *framebuffersink = GST_FRAMEBUFFERSINK (sink);
  GstFramebufferSinkClass *klass =
      GST_FRAMEBUFFERSINK_GET_CLASS (framebuffersink);
  gchar s[256];

  GST_DEBUG_OBJECT (framebuffersink, "start");

  framebuffersink->use_hardware_overlay =
      framebuffersink->use_hardware_overlay_property;
  framebuffersink->use_buffer_pool = framebuffersink->use_buffer_pool_property;
  framebuffersink->vsync = framebuffersink->vsync_property;

  if (!klass->open_hardware (framebuffersink, &framebuffersink->screen_info,
          &framebuffersink->video_memory_size,
          &framebuffersink->pannable_video_memory_size))
    return FALSE;

  framebuffersink->max_framebuffers =
      framebuffersink->pannable_video_memory_size /
      GST_VIDEO_INFO_SIZE (&framebuffersink->screen_info);

  g_sprintf (s,
      "Succesfully opened screen of pixel depth %d, dimensions %d x %d, "
      "format %s, %.2lf MB video memory available, "
      "max %d pannable screen buffers",
      GST_VIDEO_INFO_COMP_PSTRIDE (&framebuffersink->screen_info, 0) * 8,
      GST_VIDEO_INFO_WIDTH (&framebuffersink->screen_info),
      GST_VIDEO_INFO_HEIGHT (&framebuffersink->screen_info),
      gst_video_format_to_string (GST_VIDEO_INFO_FORMAT
          (&framebuffersink->screen_info)),
      (double) framebuffersink->video_memory_size / (1024 * 1024),
      framebuffersink->max_framebuffers);
  if (framebuffersink->vsync)
    g_sprintf (s + strlen (s), ", vsync enabled");
  GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink, s);

  if (framebuffersink->full_screen) {
    framebuffersink->requested_video_width =
        GST_VIDEO_INFO_WIDTH (&framebuffersink->screen_info);
    framebuffersink->requested_video_height =
        GST_VIDEO_INFO_HEIGHT (&framebuffersink->screen_info);
  }

  /* Get a screen allocator. */
  framebuffersink->screen_video_memory_allocator =
      klass->video_memory_allocator_new (framebuffersink,
      &framebuffersink->screen_info, TRUE, FALSE);
  framebuffersink->overlay_video_memory_allocator = NULL;

  /* Perform benchmarks if requested. */
  if (framebuffersink->benchmark)
    gst_framebuffersink_benchmark (framebuffersink);

  /* Reset overlay types. */
  framebuffersink->overlay_formats_supported =
      gst_framebuffersink_get_supported_overlay_formats (framebuffersink);
  /* Set overlay types if supported. */
  if (framebuffersink->use_hardware_overlay) {
    framebuffersink->current_overlay_index = 0;
    framebuffersink->overlay_formats_supported =
        klass->get_supported_overlay_formats (framebuffersink);
  }

  framebuffersink->current_framebuffer_index = 0;
  framebuffersink->nu_screens_used = 0;
  framebuffersink->screens = NULL;
  framebuffersink->nu_overlays_used = 0;
  framebuffersink->overlays = NULL;

  framebuffersink->stats_video_frames_video_memory = 0;
  framebuffersink->stats_video_frames_system_memory = 0;
  framebuffersink->stats_overlay_frames_video_memory = 0;
  framebuffersink->stats_overlay_frames_system_memory = 0;

  return TRUE;
}

/* Sets size, frame-rate and hardware-overlay format preferences on caps. */

static void
gst_framebuffersink_caps_set_preferences (GstFramebufferSink * framebuffersink,
    GstCaps * caps, gboolean fix_width_if_possible)
{
  /* If hardware scaling is supported, and a specific video size is requested,
     allow any reasonable size (except when the width/height_before_scaler
     properties are set) and use the scaler. */
  if (framebuffersink->use_hardware_overlay &&
      (framebuffersink->requested_video_width != 0 ||
          framebuffersink->requested_video_height != 0)) {
    if (framebuffersink->width_before_scaling != 0)
      gst_caps_set_simple (caps, "width", G_TYPE_INT,
          framebuffersink->width_before_scaling, NULL);
    else
      gst_caps_set_simple (caps, "width", GST_TYPE_INT_RANGE, 1,
          GST_VIDEO_INFO_WIDTH (&framebuffersink->screen_info), NULL);
    if (framebuffersink->height_before_scaling != 0)
      gst_caps_set_simple (caps, "height", G_TYPE_INT,
          framebuffersink->height_before_scaling, NULL);
    else
      gst_caps_set_simple (caps, "height", GST_TYPE_INT_RANGE, 1,
          GST_VIDEO_INFO_HEIGHT (&framebuffersink->screen_info), NULL);
    goto skip_video_size_request;
  }

  /* Honour video size requests, otherwise set the allowable range up to the
     screen size. */
  if (fix_width_if_possible && framebuffersink->requested_video_width != 0)
    gst_caps_set_simple (caps,
        "width", G_TYPE_INT, framebuffersink->requested_video_width, NULL);
  else
    gst_caps_set_simple (caps, "width", GST_TYPE_INT_RANGE, 1,
        GST_VIDEO_INFO_WIDTH (&framebuffersink->screen_info), NULL);
  if (fix_width_if_possible && framebuffersink->requested_video_height != 0)
    gst_caps_set_simple (caps,
        "height", G_TYPE_INT, framebuffersink->requested_video_height, NULL);
  else
    gst_caps_set_simple (caps, "height", GST_TYPE_INT_RANGE, 1,
        GST_VIDEO_INFO_HEIGHT (&framebuffersink->screen_info), NULL);

skip_video_size_request:

  /* Honour frames per second requests. */
  if (framebuffersink->fps != 0)
    gst_caps_set_simple (caps,
        "framerate", GST_TYPE_FRACTION, framebuffersink->fps, 1, NULL);
  else
    gst_caps_set_simple (caps, "framerate", GST_TYPE_FRACTION_RANGE, 0, 1,
        G_MAXINT, 1, NULL);

  /* Honour specified overlay format. */
  if (framebuffersink->preferred_overlay_format_str != NULL) {
    GstVideoFormat f =
        gst_video_format_from_string
        (framebuffersink->preferred_overlay_format_str);
    if (f != GST_VIDEO_FORMAT_UNKNOWN)
      gst_caps_set_simple (caps,
          "format", G_TYPE_STRING, gst_video_format_to_string (f), NULL);
    caps = gst_caps_simplify (caps);
  }
}

/* Return default caps, or NULL if no default caps could be not generated. */

static GstCaps *
gst_framebuffersink_get_default_caps (GstFramebufferSink * framebuffersink)
{
  GstCaps *caps;
  GstCaps *framebuffer_caps;
  GstVideoFormat *f;

  if (GST_VIDEO_INFO_FORMAT (&framebuffersink->screen_info) ==
      GST_VIDEO_FORMAT_UNKNOWN)
    goto unknown_format;

  caps = gst_caps_new_empty ();

  /* First add any specific overlay formats that are supported.
     They will have precedence over the standard framebuffer format. */

  f = framebuffersink->overlay_formats_supported;
  while (*f != GST_VIDEO_FORMAT_UNKNOWN) {
    if (*f != GST_VIDEO_INFO_FORMAT (&framebuffersink->screen_info)) {
      GstCaps *overlay_caps = gst_caps_new_simple ("video/x-raw", "format",
          G_TYPE_STRING, gst_video_format_to_string (*f), NULL);
      gst_caps_append (caps, overlay_caps);
    }
    f++;
  }

  /* Add the standard framebuffer format. */
  framebuffer_caps = gst_caps_new_simple ("video/x-raw", "format",
      G_TYPE_STRING,
      gst_video_format_to_string (GST_VIDEO_INFO_FORMAT
          (&framebuffersink->screen_info)), "interlace-mode", G_TYPE_STRING,
      "progressive", "pixel-aspect-ratio", GST_TYPE_FRACTION_RANGE, 1, G_MAXINT,
      G_MAXINT, 1, NULL);
  gst_caps_append (caps, framebuffer_caps);

  return caps;

unknown_format:

  GST_WARNING_OBJECT (framebuffersink, "could not map framebuffer format");
  return NULL;
}

/* Helper function to parse caps in a fool-proof manner and pick our preferred
   video format from caps. */

static GstVideoFormat
gst_framebuffersink_get_preferred_video_format_from_caps (GstFramebufferSink *
    framebuffersink, GstCaps * caps)
{
  GstCaps *ncaps;
  int n;
  int i;
  GstVideoFormat best_format = GST_VIDEO_FORMAT_UNKNOWN;
  int best_rank = G_MAXINT;
  GstVideoFormat preferred_overlay_format_from_property =
      GST_VIDEO_FORMAT_UNKNOWN;
  if (framebuffersink->preferred_overlay_format_str != NULL) {
    preferred_overlay_format_from_property =
        gst_video_format_from_string
        (framebuffersink->preferred_overlay_format_str);
    if (preferred_overlay_format_from_property == GST_VIDEO_FORMAT_UNKNOWN)
      GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink,
          "Unknown video format in overlay-format property");
  }
  ncaps = gst_caps_copy (caps);
  ncaps = gst_caps_normalize (ncaps);
  n = gst_caps_get_size (ncaps);
  for (i = 0; i < n; i++) {
    GstStructure *str = gst_caps_get_structure (ncaps, i);
    const char *format_s;
    format_s = gst_structure_get_string (str, "format");
    if (format_s != NULL) {
      GstVideoFormat f = gst_video_format_from_string (format_s);
      int r;
      if (!gst_framebuffersink_video_format_supported_by_overlay
          (framebuffersink, f)) {
        /* Regular formats that are not supported by the overlay get a rank 
           that is based on the order in the caps but always behind the
           overlay formats. */
        if (i + 1000000 < best_rank) {
          best_format = f;
          best_rank = i + 1000000;
        }
        continue;
      }
      if (preferred_overlay_format_from_property != GST_VIDEO_FORMAT_UNKNOWN
          && f == preferred_overlay_format_from_property)
        r = -1;
      else
        r = gst_framebuffersink_get_overlay_format_rank (framebuffersink, f);
      if (r < best_rank) {
        best_format = f;
        best_rank = r;
      }
    }
  }
  gst_caps_unref (ncaps);
  return best_format;
}

/* get_caps is called by GstBaseSink for two purposes:
   1. When filter is not NULL, it is a GST_QUERY_CAPS query.
      The function should suggest caps based on filter.
      It is advisable that the the suggested caps are a subset of filter.,
      otherwise negotation may fail.
   2. When filter is NULL, it is a GST_QUERY_ACCEPT_CAPS query.
      The function should return the allowed caps. GstBaseSink actually
      hides the specific caps used by the upstream query. */

static GstCaps *
gst_framebuffersink_get_caps (GstBaseSink * sink, GstCaps * filter)
{
  GstFramebufferSink *framebuffersink = GST_FRAMEBUFFERSINK (sink);
  GstCaps *caps;
  int i;
  int w, h;
  int par_n, par_d;
  int n;
  const char *format_str = NULL;
  GstVideoFormat format;

  GST_OBJECT_LOCK (framebuffersink);

  GST_DEBUG_OBJECT (framebuffersink, "get_caps: filter caps: %"
      GST_PTR_FORMAT "\n", filter);

  /* If the screen info hasn't been initialized yet, return template caps. */
  if (GST_VIDEO_INFO_FORMAT (&framebuffersink->screen_info) ==
      GST_VIDEO_FORMAT_UNKNOWN) {
    caps =
        gst_static_pad_template_get_caps (&gst_framebuffersink_sink_template);
    if (filter) {
      GstCaps *intersection = gst_caps_intersect_full (filter, caps,
          GST_CAPS_INTERSECT_FIRST);
      gst_caps_unref (caps);
      caps = intersection;
    }
    goto done_no_store;
  }

  /* When filter is not NULL (CAPS query), and have already stored the caps, */
  /* return the stored caps. */
  if (filter != NULL && framebuffersink->caps) {
    GST_WARNING_OBJECT (framebuffersink,
        "get_caps called after dimensions adjusted");
    caps = gst_caps_ref (framebuffersink->caps);
    goto done_no_store;
  }

  /* Generate default caps for the screen. */
  caps = gst_framebuffersink_get_default_caps (framebuffersink);
  if (caps == NULL)
    goto done_no_store;
  gst_framebuffersink_caps_set_preferences (framebuffersink, caps, TRUE);

  /* For an ACCEPT_CAPS query, return the default caps for the screen. */
  if (filter == NULL)
    goto done_no_store;

  /* Check whether upstream is reporting video dimensions and par. */
  n = gst_caps_get_size (filter);
  w = 0;
  h = 0;
  par_n = 0;
  par_d = 0;
  for (i = 0; i < n; i++) {
    const gchar *fs;
    GstStructure *str = gst_caps_get_structure (filter, i);
    gst_structure_get_int (str, "width", &w);
    gst_structure_get_int (str, "height", &h);
    if (gst_structure_has_field (str, "pixel-aspect-ratio")) {
      gst_structure_get_fraction (str, "pixel-aspect-ratio", &par_n, &par_d);
    }
    fs = gst_structure_get_string (str, "format");
    if (fs != NULL && format_str == NULL)
      format_str = fs;
  }

  /* Wait until upstream reports the video dimensions. */
  if (w == 0 || h == 0) {
    /* Upstream has not yet confirmed a video size */
    /* Return the intersection of the current caps with the filter caps. */
    GstCaps *icaps;
    icaps = gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = icaps;
    goto done_no_store;
  }

  /* Upstream has confirmed a video size */

  /* Get the video format from caps that is our preferred video */
  /* format (supported by overlay). */
  format =
      gst_framebuffersink_get_preferred_video_format_from_caps (framebuffersink,
      caps);
  if (gst_framebuffersink_video_format_supported_by_overlay (framebuffersink,
          format)) {
    /* Set the preferred format. */
    gst_caps_set_simple (caps,
        "format", G_TYPE_STRING, gst_video_format_to_string (format), NULL);
  } else
    /* Set the screen framebuffer format. */
    gst_caps_set_simple (caps,
        "format", G_TYPE_STRING,
        gst_video_format_to_string (GST_VIDEO_INFO_FORMAT
            (&framebuffersink->screen_info)), NULL);

  caps = gst_caps_simplify (caps);

  /* Return the intersection of the current caps with the filter caps. */
  if (filter != NULL) {
    GstCaps *icaps;

    icaps = gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = icaps;
  }

  /* Store the updated caps. */
  if (framebuffersink->caps)
    gst_caps_unref (framebuffersink->caps);
  framebuffersink->caps = gst_caps_ref (caps);

done_no_store:

  GST_DEBUG_OBJECT (framebuffersink, "get_caps: returned caps: %"
      GST_PTR_FORMAT "\n", caps);

  GST_OBJECT_UNLOCK (framebuffersink);

  return caps;
}

/* This function is called from set_caps when we are configured with */
/* use_buffer_pool=true, and from propose_allocation */

static GstBufferPool *
gst_framebuffersink_allocate_buffer_pool (GstFramebufferSink * framebuffersink,
    GstCaps * caps, GstVideoInfo * info)
{
  GstFramebufferSinkClass *klass =
      GST_FRAMEBUFFERSINK_GET_CLASS (framebuffersink);
  GstStructure *config;
  GstBufferPool *newpool;
  GstAllocator *allocator;
  int n;
  char s[256];

  GST_DEBUG ("allocate_buffer_pool, caps: %" GST_PTR_FORMAT, caps);

  /* Create a new pool for the new configuration. */
  newpool = gst_buffer_pool_new ();

  config = gst_buffer_pool_get_config (newpool);

  n = framebuffersink->nu_screens_used;
  if (framebuffersink->use_hardware_overlay)
    n = framebuffersink->nu_overlays_used;

#ifdef HALF_POOLS
  n /= 2;
#endif
  gst_buffer_pool_config_set_params (config, caps, info->size, n, n);

  if (framebuffersink->use_hardware_overlay) {
    /* Make sure one screen is allocated when using the hardware overlay. */
    if (framebuffersink->screens == NULL) {
      framebuffersink->screens = g_slice_alloc (sizeof (GstMemory *) * 1);
      /* Use the default alignment for the screen video memory allocator. */
      framebuffersink->screens[0] =
          gst_allocator_alloc (framebuffersink->screen_video_memory_allocator,
          GST_VIDEO_INFO_SIZE (&framebuffersink->screen_info), NULL);
    }
    /* Create the overlay allocator. */
    if (!framebuffersink->overlay_video_memory_allocator)
      framebuffersink->overlay_video_memory_allocator =
          klass->video_memory_allocator_new (framebuffersink, info, FALSE,
          TRUE);
    allocator = framebuffersink->overlay_video_memory_allocator;
  } else {
    allocator = framebuffersink->screen_video_memory_allocator;
  }

  /* Use the default allocation params for the allocator. */
  gst_buffer_pool_config_set_allocator (config, allocator, NULL);
  if (!gst_buffer_pool_set_config (newpool, config))
    goto config_failed;

  g_sprintf (s,
      "Succesfully allocated buffer pool (frame size %zd, %d buffers)",
      info->size, n);
  GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink, s);

#if 0
  if (!gst_buffer_pool_set_active (framebuffersink->pool, TRUE))
    goto activation_failed;

  GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink,
      "Succesfully activated buffer pool");
#endif

  return newpool;

/* ERRORS */
config_failed:
  {
    GST_ERROR_OBJECT (framebuffersink, "Failed to set buffer pool config");
    return NULL;
  }
#if 0
activation_failed:
  {
    GST_ERROR_OBJECT (framebuffersink, "Activation of buffer pool failed");
    return NULL;
  }
#endif
}


/* Exported utility function to conveniently convert scanline alignment to the
   GstFramebufferSinkOverlayVideoAlignment information required by the
   get_overlay_video_alignment class function if strict_alignment is TRUE
   scanlines need to be aligned to the alignment defined by scanline_align
   but should not be aligned to a greater alignment. */
void gst_framebuffersink_set_overlay_video_alignment_from_scanline_alignment
    (GstFramebufferSink * framebuffersink, GstVideoInfo * video_info,
    gint scanline_align, gboolean strict_alignment,
    GstFramebufferSinkOverlayVideoAlignment * video_alignment,
    gboolean * video_alignment_matches)
{
  guint scaled_pstride_bits[GST_VIDEO_MAX_PLANES];
  int comp[GST_VIDEO_MAX_PLANES];
  int i;
  int n;
  gboolean matches;
  /* Set the pixel strides for each plane. */
  /* Iterate components instead of planes. The width for planes which contain
     multiple components will be written multiple times but should be the same. */
  n = GST_VIDEO_INFO_N_COMPONENTS (video_info);
  for (i = 0; i < n; i++) {
    int plane = GST_VIDEO_INFO_COMP_PLANE (video_info, i);
    scaled_pstride_bits[plane] =
        GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (video_info->finfo, i,
        8) * GST_VIDEO_INFO_COMP_PSTRIDE (video_info, i);
    comp[plane] = i;
  }

  matches = TRUE;
  video_alignment->padding_top = 0;
  video_alignment->padding_bottom = 0;
  n = GST_VIDEO_INFO_N_PLANES (video_info);
  for (i = 0; i < n; i++) {
    gboolean plane_matches;
    if ((GST_VIDEO_INFO_PLANE_STRIDE (video_info, i) & scanline_align) == 0) {
      plane_matches = TRUE;
      if (strict_alignment) {
        int aligned_stride;
        int aligned_width_in_bytes;
        aligned_stride =
            ALIGNMENT_GET_ALIGNED (GST_VIDEO_INFO_PLANE_STRIDE (video_info, i),
            scanline_align);
        aligned_width_in_bytes =
            ALIGNMENT_GET_ALIGNED ((GST_VIDEO_INFO_WIDTH (video_info) *
                scaled_pstride_bits[i] + 7) / 8, scanline_align);
        if (aligned_stride != aligned_width_in_bytes)
          plane_matches = FALSE;
      }
    } else
      plane_matches = FALSE;
    if (plane_matches) {
      /* This plane matches the hardware overlay stride alignment
         requirements. */
      GST_DEBUG_OBJECT (framebuffersink,
          "Overlay stride alignment matches for plane %d", i);
      video_alignment->padding_left[i] = 0;
      video_alignment->padding_right[i] =
          (GST_VIDEO_INFO_PLANE_STRIDE (video_info,
              i) * 8 -
          GST_VIDEO_INFO_WIDTH (video_info) * scaled_pstride_bits[i]) /
          scaled_pstride_bits[i];
    } else {
      /* This plane doesn't match the stride alignment requirement. */
      int aligned_width_in_bytes;
      GST_DEBUG_OBJECT (framebuffersink,
          "Overlay stride alignment doesn't match for plane %d", i);
      aligned_width_in_bytes =
          ALIGNMENT_GET_ALIGNED (GST_VIDEO_FORMAT_INFO_SCALE_WIDTH
          (video_info->finfo, comp[i],
              GST_VIDEO_INFO_WIDTH (video_info)) * scaled_pstride_bits[i] / 8,
          scanline_align);
      video_alignment->padding_left[i] = 0;
      video_alignment->padding_right[i] = (aligned_width_in_bytes * 8 -
          GST_VIDEO_INFO_WIDTH (video_info) * scaled_pstride_bits[i])
          / scaled_pstride_bits[i];
      matches = FALSE;
    }
    video_alignment->stride_align[i] = scanline_align;
  }
  *video_alignment_matches = matches;
}

static void
gst_framebuffersink_calculate_plane_widths (GstFramebufferSink *
    framebuffersink, GstVideoInfo * info)
{
  /* Iterate components instead of planes. The width for planes which contain
     multiple components will be written multiple times but should be the
     same. */
  int n = GST_VIDEO_INFO_N_COMPONENTS (info);
  int i;
  for (i = 0; i < n; i++) {
    int plane = GST_VIDEO_INFO_COMP_PLANE (info, i);
    framebuffersink->source_video_width_in_bytes[plane] =
        GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (info->finfo, i,
        GST_VIDEO_INFO_WIDTH (info)) * GST_VIDEO_INFO_COMP_PSTRIDE (info, i);
    GST_LOG_OBJECT (framebuffersink,
        "calculate_plane_widths: component %d, plane %d, pixel stride %d\n",
        i, plane, GST_VIDEO_INFO_COMP_PSTRIDE (info, i));
  }

}

/* Set actual overlay organization in memory.
 *
 * info: The source video info.
 * video_alignment: The GstVideoAlignment of the overlay in video memory.
 * video_alignment_matches: Whether the video alignment in video memory matches the alignment
 *     of the source video data.
 * overlay_align: The alignment of the start of the complete overlay buffers in video memory.
 *
 * Sets framebuffersink->overlay_plane_offset[i], framebuffersink->overlay_scanline_offset[i],
 * and framebuffersink->overlay_scanline_stride[i] for each plane,
 * framebuffersink->overlay_size, framebufferssink->overlay_alignment,
 * and framebuffersink->overlay_alignment_is_native.
 */

static void
gst_framebuffersink_calculate_overlay_size (GstFramebufferSink *
    framebuffersink, GstVideoInfo * info,
    GstFramebufferSinkOverlayVideoAlignment * video_alignment,
    gint overlay_align, gboolean video_alignment_matches)
{
  guint scaled_pstride_bits[GST_VIDEO_MAX_PLANES];
  int comp[GST_VIDEO_MAX_PLANES];
  int i;
  int n;
  /* Set the pixel strides for each plane. */
  /* Iterate components instead of planes. The width for planes which contain
     multiple components will be written multiple times but should be the
     same. */
  n = GST_VIDEO_INFO_N_COMPONENTS (info);
  for (i = 0; i < n; i++) {
    int plane = GST_VIDEO_INFO_COMP_PLANE (info, i);
    scaled_pstride_bits[plane] =
        GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (info->finfo, i,
        8) * GST_VIDEO_INFO_COMP_PSTRIDE (info, i);
    comp[plane] = i;
  }
  n = GST_VIDEO_INFO_N_PLANES (info);
  int offset = 0;
  for (i = 0; i < n; i++) {
    int padded_width;
    int padded_width_in_bytes;
    int stride;
    offset += ALIGNMENT_GET_ALIGN_BYTES (offset,
        video_alignment->stride_align[i]);
    framebuffersink->overlay_plane_offset[i] = offset;
    framebuffersink->overlay_scanline_offset[i] =
        video_alignment->padding_left[i] * scaled_pstride_bits[i] / 8;
    padded_width =
        video_alignment->padding_left[i] + GST_VIDEO_INFO_WIDTH (info) +
        video_alignment->padding_right[i];
    padded_width_in_bytes = padded_width * scaled_pstride_bits[i] / 8;
    stride = ALIGNMENT_GET_ALIGNED (padded_width_in_bytes,
        video_alignment->stride_align[i]);
    GST_DEBUG_OBJECT (framebuffersink, "Plane %d: stride alignment = %u, "
        "padded width = %u, stride = %d",
        i, video_alignment->stride_align[i], padded_width, stride);
    framebuffersink->overlay_scanline_stride[i] = stride;
    offset += GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (info->finfo, comp[i],
        (video_alignment->padding_top + GST_VIDEO_INFO_HEIGHT (info)
            + video_alignment->padding_bottom)) * stride;
  }
  framebuffersink->overlay_size = offset;
  framebuffersink->overlay_align = overlay_align;
  if (video_alignment_matches)
    framebuffersink->overlay_alignment_is_native = TRUE;
  else
    framebuffersink->overlay_alignment_is_native = FALSE;
}

/* This function is called when the GstBaseSink should prepare itself */
/* for a given media format. It practice it may be called twice with the */
/* same caps, so we have to detect that. */

static gboolean
gst_framebuffersink_set_caps (GstBaseSink * sink, GstCaps * caps)
{
  GstFramebufferSink *framebuffersink = GST_FRAMEBUFFERSINK (sink);
  GstFramebufferSinkClass *klass =
      GST_FRAMEBUFFERSINK_GET_CLASS (framebuffersink);
  GstVideoInfo info;
  GstVideoFormat matched_overlay_format;
  GstVideoRectangle src_video_rectangle;
  GstVideoRectangle screen_video_rectangle;
  int i;



  if (!gst_video_info_from_caps (&info, caps))
    goto invalid_format;

  GST_OBJECT_LOCK (framebuffersink);

  if (gst_video_info_is_equal (&info, &framebuffersink->video_info)) {
    GST_OBJECT_UNLOCK (framebuffersink);
    GST_WARNING_OBJECT (framebuffersink, "set_caps called with same caps");
    return TRUE;
  }

  GST_INFO_OBJECT (framebuffersink, "Negotiated caps: %" GST_PTR_FORMAT "\n",
      caps);

  /* Set the video parameters for GstVideoSink. */
  framebuffersink->videosink.width = info.width;
  framebuffersink->videosink.height = info.height;

  if (framebuffersink->videosink.width <= 0 ||
      framebuffersink->videosink.height <= 0)
    goto no_display_size;

  gst_framebuffersink_calculate_plane_widths (framebuffersink, &info);

  matched_overlay_format = GST_VIDEO_INFO_FORMAT (&info);
  if (!gst_framebuffersink_video_format_supported_by_overlay (framebuffersink,
          matched_overlay_format)) {
    matched_overlay_format = GST_VIDEO_FORMAT_UNKNOWN;
  }

  /* Set the dimensions of the source video rectangle and screen video
     rectangle. */
  src_video_rectangle.x = 0;
  src_video_rectangle.y = 0;
  src_video_rectangle.w = info.width;
  src_video_rectangle.h = info.height;
  screen_video_rectangle.x = 0;
  screen_video_rectangle.y = 0;
  screen_video_rectangle.w =
      GST_VIDEO_INFO_WIDTH (&framebuffersink->screen_info);
  screen_video_rectangle.h = GST_VIDEO_INFO_HEIGHT
      (&framebuffersink->screen_info);


  /* Clip and center video rectangle. */
  if (matched_overlay_format == GST_VIDEO_FORMAT_UNKNOWN) {
    if (framebuffersink->preserve_par && (info.par_n !=
            framebuffersink->screen_info.par_n ||
            info.par_d != framebuffersink->screen_info.par_d))
      GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink,
          "Cannot preserve aspect ratio in non-hardware scaling mode");

    /* No scaling; clip and center against the dimensions of the screen. */
    gst_video_sink_center_rect (src_video_rectangle, screen_video_rectangle,
        &framebuffersink->video_rectangle, FALSE);
  } else {
    /* Set video rectangle when hardware scaler is enabled. */
    GstVideoRectangle dst_video_rectangle;
    GstVideoRectangle temp_video_rectangle;
    dst_video_rectangle.x = 0;
    dst_video_rectangle.y = 0;
    dst_video_rectangle.w = info.width;
    dst_video_rectangle.h = info.height;
    /* When using the hardware scaler, the incoming video size may not match
       the desired window size. */
    if (framebuffersink->requested_video_width != 0 &&
        framebuffersink->requested_video_width != info.width)
      dst_video_rectangle.w = framebuffersink->requested_video_width;

    if (framebuffersink->requested_video_height != 0 &&
        framebuffersink->requested_video_height != info.height)
      dst_video_rectangle.h = framebuffersink->requested_video_height;

    /* Correct for aspect ratio if preserve_par property is set. */

    if (framebuffersink->preserve_par) {
      src_video_rectangle.w =
          gst_util_uint64_scale_round (src_video_rectangle.w,
          info.par_d * framebuffersink->screen_info.par_d,
          info.par_n * framebuffersink->screen_info.par_n);
      GST_DEBUG_OBJECT (framebuffersink,
          "Source video rectangle after correction of size (%u, %u)",
          src_video_rectangle.w, src_video_rectangle.h);

      /* Insert black boxes if necessary. */
      gst_video_sink_center_rect (src_video_rectangle, dst_video_rectangle,
          &temp_video_rectangle, TRUE);

      GST_DEBUG_OBJECT (framebuffersink,
          "Video rectangle after scaling of (%u, %u)",
          temp_video_rectangle.w, temp_video_rectangle.h);

      /* Center it. */
      gst_video_sink_center_rect (temp_video_rectangle, screen_video_rectangle,
          &framebuffersink->video_rectangle, FALSE);
    } else
      /* Center it. */
      gst_video_sink_center_rect (dst_video_rectangle, screen_video_rectangle,
          &framebuffersink->video_rectangle, FALSE);
    GST_INFO_OBJECT (framebuffersink,
        "Display rectangle at (%u, %u) of size (%u, %u)",
        framebuffersink->video_rectangle.x, framebuffersink->video_rectangle.y,
        framebuffersink->video_rectangle.w, framebuffersink->video_rectangle.h);

  }

  framebuffersink->video_rectangle_width_in_bytes =
      framebuffersink->video_rectangle.w *
      GST_VIDEO_INFO_COMP_PSTRIDE (&framebuffersink->screen_info, 0);

  if (framebuffersink->video_rectangle_width_in_bytes <= 0 ||
      framebuffersink->video_rectangle.h <= 0)
    goto no_display_output_size;

  if (framebuffersink->flip_buffers > 0) {
    if (framebuffersink->flip_buffers < framebuffersink->max_framebuffers)
      framebuffersink->max_framebuffers = framebuffersink->flip_buffers;
  }

  /* Check whether we will use the hardware overlay feature. */
  if (((framebuffersink->video_rectangle.w != framebuffersink->videosink.width
              || framebuffersink->video_rectangle.h !=
              framebuffersink->videosink.height)
          || matched_overlay_format !=
          GST_VIDEO_INFO_FORMAT (&framebuffersink->screen_info))
      && matched_overlay_format != GST_VIDEO_FORMAT_UNKNOWN
      && framebuffersink->use_hardware_overlay) {

    GstFramebufferSinkOverlayVideoAlignment overlay_video_alignment;
    gint overlay_align;
    gboolean overlay_video_alignment_matches;
    int max_overlays;
    int first_overlay_offset;
    /* The video dimensions are different from the requested ones, or the video
       format is not equal to the framebuffer format, and we are allowed to use
       the hardware overlay. */
    if (!klass->get_overlay_video_alignment (framebuffersink, &info,
            &overlay_video_alignment, &overlay_align,
            &overlay_video_alignment_matches))
      goto no_overlay;
    /* Calculate the overlay total size and alignment, and plane offsets and
       strides in video memory. */
    gst_framebuffersink_calculate_overlay_size (framebuffersink, &info,
        &overlay_video_alignment, overlay_align,
        overlay_video_alignment_matches);
    /* Calculate how may overlays fit in the available video memory (after the
       visible  screen). */
    first_overlay_offset = GST_VIDEO_INFO_SIZE (&framebuffersink->screen_info);
    ALIGNMENT_APPLY (first_overlay_offset, framebuffersink->overlay_align);
    max_overlays = (framebuffersink->video_memory_size - first_overlay_offset)
        / ALIGNMENT_GET_ALIGNED (framebuffersink->overlay_size,
        framebuffersink->overlay_align);
    /* Limit the number of overlays used, unless the agressive max video memory
       setting is enabled. */
    if (framebuffersink->max_video_memory_property != -2 && max_overlays > 30)
      max_overlays = 30;
    if (max_overlays >= 2 && klass->prepare_overlay (framebuffersink,
            matched_overlay_format)) {
      /* Use the hardware overlay. */
      framebuffersink->nu_screens_used = 1;
      framebuffersink->nu_overlays_used = max_overlays;
      if (framebuffersink->use_buffer_pool) {
        if (framebuffersink->overlay_alignment_is_native) {
          GstBufferPool *pool;
          pool = gst_framebuffersink_allocate_buffer_pool (framebuffersink,
              caps, &info);
          if (pool) {
            /* Use buffer pool. */
            framebuffersink->pool = pool;
            if (!framebuffersink->silent)
              GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink,
                  "Using custom buffer pool "
                  "(streaming directly to video memory)");
            goto success_overlay;
          }
        }
        framebuffersink->use_buffer_pool = FALSE;
        if (!framebuffersink->silent) {
          if (!framebuffersink->overlay_alignment_is_native)
            GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink,
                "Alignment restrictions make overlay buffer-pool mode "
                "impossible for this video size");
          GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink,
              "Falling back to non buffer-pool mode");
        }
      }
      /* Not using buffer pool. Using a lot of off-screen buffers may not
         help. */
      if (framebuffersink->nu_overlays_used > 8)
        framebuffersink->nu_overlays_used = 8;
      goto success_overlay;
    }
  }

no_overlay:
  if (framebuffersink->use_hardware_overlay) {
    GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink,
        "Disabling hardware overlay");
    framebuffersink->use_hardware_overlay = FALSE;
  }

  if (matched_overlay_format != GST_VIDEO_FORMAT_UNKNOWN &&
      matched_overlay_format !=
      GST_VIDEO_INFO_FORMAT (&framebuffersink->screen_info))
    goto overlay_failed;

reconfigure:

  /* When using buffer pools, do the appropriate checks and allocate a
     new buffer pool. */
  if (framebuffersink->use_buffer_pool &&
      framebuffersink->video_rectangle_width_in_bytes !=
      GST_VIDEO_INFO_COMP_STRIDE (&framebuffersink->screen_info, 0)) {
    GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink,
        "Cannot use buffer pool in video memory because video width is not "
        "equal to the configured framebuffer width");
    framebuffersink->use_buffer_pool = FALSE;
  }
  if (framebuffersink->use_buffer_pool && framebuffersink->max_framebuffers < 2) {
    GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink,
        "Not enough framebuffer memory to use a buffer pool "
        "(need at least two framebuffers)");
    framebuffersink->use_buffer_pool = FALSE;
  }
  framebuffersink->nu_screens_used = 1;
  if (framebuffersink->max_framebuffers >= 2) {
    framebuffersink->nu_screens_used = framebuffersink->max_framebuffers;
    /* Using a fair number of buffers could be advantageous, but use no more
       than 10 by default except if the agressive video memory property
       setting is enabled. */
    if (framebuffersink->use_buffer_pool) {
      if (framebuffersink->flip_buffers == 0
          && framebuffersink->nu_screens_used > 10
          && framebuffersink->max_video_memory_property != -2)
        framebuffersink->nu_screens_used = 10;
    } else
      /* When not using a buffer pool, only a few buffers are required for
         page flipping. */
    if (framebuffersink->flip_buffers == 0
        && framebuffersink->nu_screens_used > 3)
      framebuffersink->nu_screens_used = 2;
    if (!framebuffersink->silent) {
      char s[80];
      g_sprintf (s, "Using %d framebuffers for page flipping",
          framebuffersink->nu_screens_used);
      GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink, s);
    }
  }
  if (framebuffersink->use_buffer_pool) {
    GstBufferPool *pool;
    pool = gst_framebuffersink_allocate_buffer_pool (framebuffersink, caps,
        &info);

    if (pool) {
      framebuffersink->pool = pool;
      if (!framebuffersink->silent)
        GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink,
            "Using custom buffer pool (streaming directly to video memory)");
      goto success;
    }
    framebuffersink->use_buffer_pool = FALSE;
    if (!framebuffersink->silent)
      GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink,
          "Falling back to non buffer-pool mode");
    goto reconfigure;
  }

success:

  if (!framebuffersink->use_buffer_pool) {
    if (framebuffersink->zeromemcpy)
      framebuffersink->nu_screens_used = 1;

    gchar *s = g_strdup_printf ("Allocating %d screen buffers",
        framebuffersink->nu_screens_used);
    GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink, s);
    g_free (s);
    framebuffersink->screens = g_slice_alloc (sizeof (GstMemory *) *
        framebuffersink->nu_screens_used);
    for (i = 0; i < framebuffersink->nu_screens_used; i++) {
      framebuffersink->screens[i] =
          gst_allocator_alloc (framebuffersink->screen_video_memory_allocator,
          GST_VIDEO_INFO_HEIGHT (&framebuffersink->screen_info) *
          GST_VIDEO_INFO_COMP_STRIDE (&framebuffersink->screen_info, 0), NULL);
      if (framebuffersink->screens[i] == NULL) {
        s = g_strdup_printf ("Could only allocate %d screen buffers", i);
        GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink, s);
        g_free (s);
        framebuffersink->nu_screens_used = i;
        break;
      }
    }
  }

finish:

  framebuffersink->video_info = info;

  /* Clear all used framebuffers to black. */
  if (framebuffersink->clear) {
    if (framebuffersink->use_hardware_overlay)
      gst_framebuffersink_clear_screen (framebuffersink, 0);
    else if (!framebuffersink->use_buffer_pool)
      for (i = 0; i < framebuffersink->nu_screens_used; i++)
        gst_framebuffersink_clear_screen (framebuffersink, i);
  }

  GST_OBJECT_UNLOCK (framebuffersink);
  return TRUE;

success_overlay:

  if (!framebuffersink->use_buffer_pool) {
    framebuffersink->screens = g_slice_alloc (sizeof (GstMemory *));
    framebuffersink->screens[0] =
        gst_allocator_alloc (framebuffersink->screen_video_memory_allocator,
        GST_VIDEO_INFO_HEIGHT (&framebuffersink->screen_info) *
        GST_VIDEO_INFO_COMP_STRIDE (&framebuffersink->screen_info, 0), NULL);
    framebuffersink->overlay_video_memory_allocator =
        klass->video_memory_allocator_new (framebuffersink, &info, FALSE, TRUE);
    framebuffersink->overlays =
        g_slice_alloc (sizeof (GstMemory *) *
        framebuffersink->nu_overlays_used);
    for (i = 0; i < framebuffersink->nu_overlays_used; i++) {
      framebuffersink->overlays[i] =
          gst_allocator_alloc (framebuffersink->overlay_video_memory_allocator,
          info.size, NULL);
      if (framebuffersink->overlays[i] == NULL) {
        framebuffersink->nu_overlays_used = i;
        break;
      }
    }
  }

  if (!framebuffersink->silent) {
    char s[128];
    sprintf (s,
        "Using one framebuffer plus %d overlays in video memory (format %s)",
        framebuffersink->nu_overlays_used,
        gst_video_format_to_string (matched_overlay_format));
    GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink, s);
  }
  goto finish;

/* ERRORS */
invalid_format:
  {
    GST_ERROR_OBJECT (framebuffersink,
        "Could not locate image format from caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }
no_display_size:
  {
    GST_ERROR_OBJECT (framebuffersink,
        "No video size configured, caps: %" GST_PTR_FORMAT, caps);
    GST_OBJECT_UNLOCK (framebuffersink);
    return FALSE;
  }
no_display_output_size:
  {
    GST_ERROR_OBJECT (framebuffersink, "No display output size configured");
    GST_OBJECT_UNLOCK (framebuffersink);
    return FALSE;
  }
overlay_failed:
  {
    GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink,
        "Cannot not handle overlay format (hardware overlay failed)");
    GST_OBJECT_UNLOCK (framebuffersink);
    return FALSE;
  }
}

/* Reset function. Called from gst_framebuffersink_stop and when going
 * from PAUSED to READY. */

static void
gst_framebuffersink_reset (GstFramebufferSink * framebuffersink)
{
  int i;
  /* Free screen buffers, but be careful because in buffer-pool mode,
     nu_screens_used will be > 0 but screens will be NULL. */
  if (framebuffersink->screens != NULL) {
    for (i = 0; i < framebuffersink->nu_screens_used; i++)
      gst_allocator_free (framebuffersink->screen_video_memory_allocator,
          framebuffersink->screens[i]);
    if (framebuffersink->nu_screens_used > 0)
      g_slice_free1 (sizeof (GstMemory *) * framebuffersink->nu_screens_used,
          framebuffersink->screens);
  }

  /* Free overlay buffers. */
  if (framebuffersink->overlays != NULL) {
    for (i = 0; i < framebuffersink->nu_overlays_used; i++)
      gst_allocator_free (framebuffersink->overlay_video_memory_allocator,
          framebuffersink->overlays[i]);
    if (framebuffersink->nu_overlays_used > 0)
      g_slice_free1 (sizeof (GstMemory *) * framebuffersink->nu_overlays_used,
          framebuffersink->overlays);
  }

  framebuffersink->current_framebuffer_index = 0;
  framebuffersink->nu_screens_used = 0;
  framebuffersink->screens = NULL;
  framebuffersink->nu_overlays_used = 0;
  framebuffersink->overlays = NULL;

  GST_OBJECT_LOCK (framebuffersink);
  if (framebuffersink->pool) {
    gst_buffer_pool_set_active (framebuffersink->pool, FALSE);
    gst_object_unref (framebuffersink->pool);
    framebuffersink->pool = NULL;
  }
  GST_OBJECT_UNLOCK (framebuffersink);

  GST_VIDEO_SINK_WIDTH (framebuffersink) = 0;
  GST_VIDEO_SINK_HEIGHT (framebuffersink) = 0;
  if (framebuffersink->caps) {
    gst_object_unref (framebuffersink->caps);
    framebuffersink->caps = NULL;
  }

  /* Reset variables derived from properties. */
  framebuffersink->use_hardware_overlay =
      framebuffersink->use_hardware_overlay_property;
  framebuffersink->use_buffer_pool = framebuffersink->use_buffer_pool_property;
  framebuffersink->vsync = framebuffersink->vsync_property;

  /* Free the overlay video memory allocator if present. */
  if (framebuffersink->overlay_video_memory_allocator) {
    g_object_unref (framebuffersink->overlay_video_memory_allocator);
    framebuffersink->overlay_video_memory_allocator = NULL;
  }
}

/* The stop function should release resources. */

static gboolean
gst_framebuffersink_stop (GstBaseSink * sink)
{
  GstFramebufferSink *framebuffersink = GST_FRAMEBUFFERSINK (sink);
  GstFramebufferSinkClass *klass =
      GST_FRAMEBUFFERSINK_GET_CLASS (framebuffersink);
  char s[128];

  GST_DEBUG_OBJECT (framebuffersink, "stop");

  sprintf (s, "%d frames rendered, %d from system memory, %d from video memory",
      framebuffersink->stats_video_frames_video_memory +
      framebuffersink->stats_overlay_frames_video_memory +
      framebuffersink->stats_video_frames_system_memory +
      framebuffersink->stats_overlay_frames_system_memory,
      framebuffersink->stats_video_frames_system_memory +
      framebuffersink->stats_overlay_frames_system_memory,
      framebuffersink->stats_video_frames_video_memory +
      framebuffersink->stats_overlay_frames_video_memory);
  GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink, s);

  gst_framebuffersink_reset (framebuffersink);

  /* Free the screen allocator. */
  g_object_unref (framebuffersink->screen_video_memory_allocator);

  klass->close_hardware (framebuffersink);

  /* The device property string should probably not be freed because start
     may be called again. */
  /* g_free (framebuffersink->device); */

  return TRUE;
}

/* The show frame function can deal with both video memory buffers
   that require a pan and with regular buffers that need to be memcpy-ed.
   There are seperate show_frame functions for overlays (with a video memory
   or a system memory buffer pool), screen buffers with buffer 
   pool in video memory, and screen buffers with a buffer pool in
   system memory. */

static GstFlowReturn
gst_framebuffersink_show_frame_memcpy (GstFramebufferSink * framebuffersink,
    GstBuffer * buffer)
{
  GstFramebufferSinkClass *klass =
      GST_FRAMEBUFFERSINK_GET_CLASS (framebuffersink);
  GstMapInfo mapinfo;
  GstMemory *mem;

  mem = gst_buffer_get_memory (buffer, 0);
  if (!gst_memory_map (mem, &mapinfo, GST_MAP_READ)) {
    GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink,
        "memory_map of system memory buffer for reading failed");
    gst_memory_unref (mem);
    return GST_FLOW_ERROR;
  }
  /* When not using page flipping, wait for vsync before copying. */
  if (framebuffersink->nu_screens_used == 1 && framebuffersink->vsync)
    klass->wait_for_vsync (framebuffersink);
  gst_framebuffersink_put_image_memcpy (framebuffersink, mapinfo.data);
  gst_memory_unmap (mem, &mapinfo);

  /* When using page flipping, wait for vsync after copying and then flip. */
  if (framebuffersink->nu_screens_used = 2) {
    if (framebuffersink->vsync && !framebuffersink->pan_does_vsync)
      klass->wait_for_vsync (framebuffersink);
    klass->pan_display (framebuffersink,
        framebuffersink->screens[framebuffersink->current_framebuffer_index]);
    framebuffersink->current_framebuffer_index++;
    if (framebuffersink->current_framebuffer_index >=
        framebuffersink->nu_screens_used)
      framebuffersink->current_framebuffer_index = 0;
  }

  gst_memory_unref (mem);

  framebuffersink->stats_video_frames_system_memory++;

  return GST_FLOW_OK;
}


static GstFlowReturn
gst_framebuffersink_show_plane_overlay (GstFramebufferSink * framebuffersink,
    GstBuffer * buffer)
{
  GstFramebufferSinkClass *klass =
      GST_FRAMEBUFFERSINK_GET_CLASS (framebuffersink);

  GstMapInfo mapinfo;
  GstMemory *mem;

  mem = gst_buffer_get_memory (buffer, 0);
  if (!gst_memory_map (mem, &mapinfo, GST_MAP_READ)) {
    GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink,
        "memory_map of system memory buffer for reading failed");
    gst_memory_unref (mem);
    return GST_FLOW_ERROR;
  }
#if 0
  if (framebuffersink->vsync && !framebuffersink->pan_does_vsync)
    klass->wait_for_vsync (framebuffersink);
  klass->pan_display (framebuffersink, buffer);

#endif

  /* When not using page flipping, wait for vsync before copying. */
  if (framebuffersink->nu_screens_used == 1 && framebuffersink->vsync)
    klass->wait_for_vsync (framebuffersink);

  gst_memory_unmap (mem, &mapinfo);

  /* When using page flipping, wait for vsync after copying and then flip. */

  if (framebuffersink->vsync && !framebuffersink->pan_does_vsync)
    klass->wait_for_vsync (framebuffersink);
  klass->pan_display (framebuffersink,
      framebuffersink->screens[framebuffersink->current_framebuffer_index]);
  framebuffersink->current_framebuffer_index++;
  if (framebuffersink->current_framebuffer_index >=
      framebuffersink->nu_screens_used)
    framebuffersink->current_framebuffer_index = 0;

  gst_memory_unref (mem);

  framebuffersink->stats_video_frames_system_memory++;

  return GST_FLOW_OK;

}

static GstFlowReturn
gst_framebuffersink_show_frame_buffer_pool (GstFramebufferSink *
    framebuffersink, GstBuffer * buf)
{
  GstMemory *mem;

  mem = gst_buffer_get_memory (buf, 0);
  if (!mem)
    goto invalid_memory;

  if (gst_framebuffersink_is_video_memory (framebuffersink, mem)) {
    /* This a video memory buffer. */

    GST_LOG_OBJECT (framebuffersink, "Video memory buffer encountered");

    gst_framebuffersink_put_image_pan (framebuffersink, mem);

    gst_memory_unref (mem);

    framebuffersink->stats_video_frames_video_memory++;

    return GST_FLOW_OK;
  } else {
    /* This is a normal memory buffer (system memory). */

    GST_LOG_OBJECT (framebuffersink, "Non-video memory buffer encountered");

    gst_memory_unref (mem);

    GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink,
        "Unexpected system memory buffer provided in buffer-pool mode, "
        "ignoring");

    return GST_FLOW_OK;
  }

invalid_memory:
  GST_ERROR_OBJECT (framebuffersink,
      "Show frame called with invalid memory buffer");
  return GST_FLOW_ERROR;
}

static GstFlowReturn
gst_framebuffersink_show_frame_overlay (GstFramebufferSink * framebuffersink,
    GstBuffer * buf)
{
  GstFramebufferSinkClass *klass =
      GST_FRAMEBUFFERSINK_GET_CLASS (framebuffersink);
  GstMemory *mem;
  GstMapInfo mapinfo;

  mem = gst_buffer_get_memory (buf, 0);
  if (!mem)
    goto invalid_memory;

  if (gst_framebuffersink_is_video_memory (framebuffersink, mem)) {
    /* This a video memory buffer. */
    GST_LOG_OBJECT (framebuffersink,
        "Video memory overlay buffer encountered, mem = %p", mem);

    /* Wait for vsync before changing the overlay address. */
    if (framebuffersink->vsync)
      klass->wait_for_vsync (framebuffersink);
    klass->show_overlay (framebuffersink, mem);

    gst_memory_unref (mem);

    framebuffersink->stats_overlay_frames_video_memory++;

    return GST_FLOW_OK;
  } else {
    /* This is a normal memory buffer (system memory), but it is
       overlay data. */

    GST_LOG_OBJECT (framebuffersink,
        "Non-video memory overlay buffer encountered, mem = %p", mem);

    if (!gst_memory_map (mem, &mapinfo, GST_MAP_READ)) {
      GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink,
          "memory_map of system memory buffer for reading failed");
      gst_memory_unref (mem);
      return GST_FLOW_ERROR;
    }

    if (framebuffersink->use_buffer_pool) {
      /* When using a buffer pool in video memory, being requested to show an
         overlay frame from system memory (which shouldn't normally happen)
         poses a bit of problem. We need to allocate a temporary video memory
         area to store the overlay frame and show it. */

      GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink,
          "Unexpected system memory overlay in buffer pool mode");

      GstMemory *vmem;
      vmem =
          gst_allocator_alloc (framebuffersink->overlay_video_memory_allocator,
          mapinfo.size, NULL);
      if (vmem == NULL)
        GST_FRAMEBUFFERSINK_MESSAGE_OBJECT (framebuffersink,
            "Could not allocate temporary video memory buffer for overlay");
      else {
        gst_framebuffersink_put_overlay_image_memcpy (framebuffersink,
            vmem, mapinfo.data);
        gst_allocator_free (framebuffersink->overlay_video_memory_allocator,
            vmem);
      }
      goto end;
    }

    /* Copy the image into video memory in one of the slots after the first
       screen. */
    gst_framebuffersink_put_overlay_image_memcpy (framebuffersink,
        framebuffersink->overlays[framebuffersink->current_overlay_index],
        mapinfo.data);

    framebuffersink->current_overlay_index++;
    if (framebuffersink->current_overlay_index >=
        framebuffersink->nu_overlays_used)
      framebuffersink->current_overlay_index = 0;

  end:
    gst_memory_unmap (mem, &mapinfo);
    gst_memory_unref (mem);

    framebuffersink->stats_overlay_frames_system_memory++;

    return GST_FLOW_OK;
  }

invalid_memory:
  GST_ERROR_OBJECT (framebuffersink,
      "Show frame called with invalid memory buffer");
  return GST_FLOW_ERROR;
}

// callvid
static GstFlowReturn
gst_framebuffersink_show_frame (GstVideoSink * vsink, GstBuffer * buf)
{
  GstFramebufferSink *framebuffersink = GST_FRAMEBUFFERSINK (vsink);
  GstFlowReturn res;

  if (framebuffersink->zeromemcpy) {
    res = gst_framebuffersink_show_plane_overlay (framebuffersink, buf);
    return res;
  } else {
    if (framebuffersink->use_hardware_overlay)
      res = gst_framebuffersink_show_frame_overlay (framebuffersink, buf);
    else if (framebuffersink->use_buffer_pool)
      res = gst_framebuffersink_show_frame_buffer_pool (framebuffersink, buf);
    else
      res = gst_framebuffersink_show_frame_memcpy (framebuffersink, buf);
  }

  return res;

}

static gboolean
gst_framebuffersink_set_buffer_pool_query_answer (GstFramebufferSink *
    framebuffersink, GstQuery * query, GstBufferPool * pool, GstCaps * caps,
    GstVideoInfo * info)
{
  GstStructure *config;
  GstAllocator *allocator;
  GstAllocationParams params;
  gsize size;
  int n;

  GST_INFO_OBJECT (framebuffersink, "Providing video memory buffer pool");

  size = info->size;
  n = framebuffersink->nu_screens_used;
  if (framebuffersink->use_hardware_overlay)
    n = framebuffersink->nu_overlays_used;

  config = gst_buffer_pool_get_config (pool);
#ifdef HALF_POOLS
  gst_buffer_pool_config_set_params (config, caps, size, n / 2, n / 2);
#else
  gst_buffer_pool_config_set_params (config, caps, size, n, n);
#endif
  if (!gst_buffer_pool_set_config (pool, config))
    return FALSE;

  /* Add the video memory allocator currently configured on the buffer
     pool. */
  gst_buffer_pool_config_get_allocator (config, &allocator, &params);
  gst_query_add_allocation_param (query, allocator, NULL);

#ifdef HALF_POOLS
  n /= 2;
#endif
  gst_query_add_allocation_pool (query, pool, size, n, n);

  GST_INFO_OBJECT (framebuffersink,
      "propose_allocation: size = %.2lf MB, %d buffers",
      (double) size / (1024 * 1024), n);

  GST_INFO_OBJECT (framebuffersink,
      "propose_allocation: provide our video memory buffer pool");

  return TRUE;
}

/* This function is called by upstream asking for the buffer allocation
   configuration. We need to answer with our own video memory-based
   buffer configuration, when it is enabled. */

static gboolean
gst_framebuffersink_propose_allocation (GstBaseSink * bsink, GstQuery * query)
{
  GstFramebufferSink *framebuffersink = GST_FRAMEBUFFERSINK (bsink);
  GstBufferPool *pool;
  GstStructure *config;
  GstCaps *caps;
  guint size;
  GstVideoInfo info;
  gboolean need_pool;

  gst_query_parse_allocation (query, &caps, &need_pool);

  GST_INFO_OBJECT (framebuffersink,
      "propose_allocation called, need_pool = %d", need_pool);

  if (caps == NULL)
    goto no_caps;

  if (!gst_video_info_from_caps (&info, caps))
    goto invalid_caps;

  GST_OBJECT_LOCK (framebuffersink);

  /* Take a look at our pre-initialized pool in video memory. */
  pool = framebuffersink->pool ? gst_object_ref (framebuffersink->pool) : NULL;

  /* If we had a buffer pool in video memory and it has been allocated,
     we can't easily provide regular system memory buffers because
     due to the difficulty of handling page flips correctly. However,
     with a lazy allocation scheme multiple video memory pools can
     coexist without running out of video memory. */
  if (framebuffersink->use_buffer_pool && pool == NULL) {
    GST_INFO_OBJECT (framebuffersink,
        "propose_allocation: Already provided video memory buffer pool");
  }

  if (pool != NULL) {
    GstCaps *pcaps;

    /* We have a pool, check the caps. */
    GST_LOG_OBJECT (framebuffersink, "check existing pool caps");
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_get_params (config, &pcaps, &size, NULL, NULL);

    if (!gst_caps_is_equal (caps, pcaps)) {
      GST_LOG_OBJECT (framebuffersink, "pool has different caps");
      /* Different caps, we can't use our pool. */
      gst_object_unref (pool);
      pool = NULL;
    }
    gst_structure_free (config);
  }

  if (pool) {
    framebuffersink->pool = NULL;
  }
#ifdef MULTIPLE_VIDEO_MEMORY_POOLS
  if (framebuffersink->use_buffer_pool && pool == NULL && need_pool) {
    /* Try to provide (another) pool from video memory. */

    pool = gst_framebuffersink_allocate_buffer_pool (framebuffersink, caps,
        &info);
    if (!pool)
      return FALSE;
    pool = gst_object_ref (pool);
  }
#endif

  /* At this point if pool is not NULL we have a video memory pool */
  /* to provide. */
  if (pool != NULL) {
    if (!gst_framebuffersink_set_buffer_pool_query_answer (framebuffersink,
            query, pool, caps, &info))
      goto config_failed;

    gst_object_unref (pool);
    goto end;
  }

  if (!need_pool) {
    GST_OBJECT_UNLOCK (framebuffersink);
    return FALSE;
  } else {
    /* Provide a regular system memory buffer pool. */
    GstAllocator *allocator;
    int n;

    n = gst_query_get_n_allocation_pools (query);
    GST_INFO_OBJECT (framebuffersink, "%d allocation pools in query", n);
    GST_INFO_OBJECT (framebuffersink, "%d allocation params in query",
        gst_query_get_n_allocation_params (query));

    GST_INFO_OBJECT (framebuffersink, "create new system memory pool");
    pool = gst_buffer_pool_new ();

    allocator = gst_allocator_find (GST_ALLOCATOR_SYSMEM);

    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_params (config, caps, info.size, 2, 0);
    gst_buffer_pool_config_set_allocator (config, allocator, NULL);
    if (!gst_buffer_pool_set_config (pool, config)) {
      gst_object_unref (allocator);
      goto config_failed;
    }

    gst_query_add_allocation_param (query, allocator, NULL);
    gst_query_add_allocation_pool (query, pool, info.size, 2, 0);
    gst_object_unref (allocator);
    gst_object_unref (pool);

  }

end:
  /* we also support various metadata */
//  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
//  gst_query_add_allocation_meta (query, GST_VIDEO_CROP_META_API_TYPE, NULL);

  GST_OBJECT_UNLOCK (framebuffersink);
  return TRUE;

  /* ERRORS */
no_caps:
  {
    GST_ERROR_OBJECT (framebuffersink, "no caps specified");
    return FALSE;
  }
invalid_caps:
  {
    GST_ERROR_OBJECT (framebuffersink, "invalid caps specified");
    return FALSE;
  }
config_failed:
  {
    GST_ERROR_OBJECT (framebuffersink, "failed setting config");
    gst_object_unref (pool);
    GST_OBJECT_UNLOCK (framebuffersink);
    return FALSE;
  }
}



/* Implementing this base class function may not be necessary, */

static GstStateChangeReturn
gst_framebuffersink_change_state (GstElement * element,
    GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstFramebufferSink *framebuffersink = GST_FRAMEBUFFERSINK (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      /* Forget everything about the current stream. */
      gst_framebuffersink_reset (framebuffersink);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }
  return ret;
}

/* The following function works for all video memory types as long as the
  GST_MEMORY_FLAG_VIDEO_MEMORY flag is set on the memory object. */
static gboolean
gst_framebuffersink_is_video_memory (GstFramebufferSink * framebuffersink,
    GstMemory * mem)
{
  return GST_MEMORY_FLAG_IS_SET (mem, GST_MEMORY_FLAG_VIDEO_MEMORY);
}

GType
gst_framebuffersink_get_type (void)
{
  static GType framebuffersink_type = 0;

  if (!framebuffersink_type) {
    static const GTypeInfo framebuffersink_info = {
      sizeof (GstFramebufferSinkClass),
      gst_framebuffersink_base_init,
      NULL,
      (GClassInitFunc) gst_framebuffersink_class_init,
      NULL,
      NULL,
      sizeof (GstFramebufferSink),
      0,
      (GInstanceInitFunc) gst_framebuffersink_init,
    };

    framebuffersink_type = g_type_register_static (GST_TYPE_VIDEO_SINK,
        "GstFramebufferSink", &framebuffersink_info, 0);
  }

  return framebuffersink_type;
}
