/* GStreamer drmsink plugin
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
/*
 * Based partly on gstkmssink.c found at
 * https://gitorious.org/vjaquez-gstreamer/ which has the following
 * copyright message.
 *
 * Copyright (C) 2012 Texas Instruments
 * Copyright (C) 2012 Collabora Ltd
 *
 * Authors:
 *  Alessandro Decina <alessandro.decina@collabora.co.uk>
 *  Víctor Manuel Jáquez Leal <vjaquez@igalia.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-drmsink
 *
 * The drmsink element implements an accelerated and optimized
 * video sink for the Linux console framebuffer using the libdrm library.
 * The basis of the implementation is the optimized framebuffer sink as
 * implemented in the GstFramebufferSink class.
 *
 * <refsect2>
 * <title>Property settings,<title>
 * <para>
 * The plugin comes with variety of configurable properties regulating
 * the size and frames per second of the video output, and various 
 * options regulating the rendering method (including rendering directly
 * to video memory and page flipping).
 * </para>
 * </refsect2>
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v videotestsrc ! drmsink >/dev/null
 * ]|
 * Output the video test signal to the framebuffer. The redirect to
 * null surpressed interference from console text mode.
 * |[
 * gst-launch -v videotestsrc ! drmsink native-resolution=true
 * ]|
 * Run videotstsrc at native screen resolution
 * |[
 * gst-launch -v videotestsrc horizontal_speed=10 ! drmsink \
 * native-resolution=true buffer-pool=true
 * ]|
 * This command illustrates some of the plugin's optimization features
 * by rendering to video memory with vsync and page flipping. There should
 * be no tearing with page flipping/vsync enabled. You might have to use
 * the fps property to reduce the frame rate on slower systems.
 * |[
 * gst-launch playbin uri=[uri] video-sink="drmsink native-resolution=true"
 * ]|
 * Use playbin while passing options to drmsink.
 * </refsect2>
 * <refsect2>
 * <title>Caveats</title>
 * <para>
 * The actual implementation of the Linux DRM API varies between
 * systems. Some implementations fail to implement a real vsync but instead
 * seem to be use some kind of fake timer close to the refresh frequency,
 * which will produce tearing.
 * </para>
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <math.h>
#include <glib/gprintf.h>
#include <xf86drmMode.h>
#include <xf86drm.h>
#include <libkms.h>

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <gst/video/video.h>
#include <gst/video/video-info.h>
#include "gstdrmsink.h"
#include "atmel_drm.h"

/* When LAZY_ALLOCATION is defined, memory buffers are only allocated
   when they are actually mapped for the first time. This solves the
   problem of GStreamer allocating multiple pools without freeing the
   previous one soon enough (resulting in running out of video memory) */
#define LAZY_ALLOCATION

#define USE_DRM_PLANES

#define DEFAULT_ZERO_MEMCPY FALSE
#define DEFAULT_LCD FALSE
#define DEFAULT_CX (0)
#define DEFAULT_CY (0)
#define DEFAULT_CW (0)
#define DEFAULT_CH (0)

GST_DEBUG_CATEGORY_STATIC (gst_drmsink_debug_category);
#define GST_CAT_DEFAULT gst_drmsink_debug_category

/* Inline function to produce informational output if silent property is not
   set; if silent property is enabled only debugging info is produced. */
static inline void
GST_DRMSINK_MESSAGE_OBJECT (GstDrmsink * drmsink, const gchar * message)
{
  if (!drmsink->framebuffersink.silent)
    g_print ("%s.\n", message);
  GST_INFO_OBJECT (drmsink, message);
}

#define DEFAULT_DRM_DEVICE "/dev/dri/card0"

/* Class function prototypes. */
static void gst_drmsink_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_drmsink_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);

static gboolean gst_drmsink_open_hardware (GstFramebufferSink * framebuffersink,
    GstVideoInfo * info, gsize * video_memory_size,
    gsize * pannable_video_memory_size);
static void gst_drmsink_close_hardware (GstFramebufferSink * framebuffersink);
static GstAllocator *gst_drmsink_video_memory_allocator_new (GstFramebufferSink
    * framebuffersink, GstVideoInfo * info, gboolean pannable,
    gboolean is_overlay);
static void gst_drmsink_pan_display (GstFramebufferSink * framebuffersink,
    GstMemory * memory);
static void gst_drmsink_wait_for_vsync (GstFramebufferSink * framebuffersink);

/* Local functions. */
static void gst_drmsink_reset (GstDrmsink * drmsink);
static void gst_drmsink_vblank_handler (int fd, unsigned int sequence,
    unsigned int tv_sec, unsigned int tv_usec, void *user_data);
static void gst_drmsink_page_flip_handler (int fd, unsigned int sequence,
    unsigned int tv_sec, unsigned int tv_usec, void *user_data);
static void gst_drmsink_flush_drm_events (GstDrmsink * drmsink);
static void gst_drmsink_wait_pending_drm_events (GstDrmsink * drmsink);
int divRoundClosest(const int , const int );

enum
{
  PROP_0,
  PROP_CONNECTOR,
  PROP_ZERO_MEMCPY,
  PROP_CX,
  PROP_CY,
  PROP_CW,
  PROP_CH,
  PROP_LCD
};

#define GST_DRMSINK_TEMPLATE_CAPS \
        GST_VIDEO_CAPS_MAKE ("RGB") \
        "; " GST_VIDEO_CAPS_MAKE ("BGR") \
        "; " GST_VIDEO_CAPS_MAKE ("RGBx") \
        "; " GST_VIDEO_CAPS_MAKE ("BGRx") \
        "; " GST_VIDEO_CAPS_MAKE ("xRGB") \
        "; " GST_VIDEO_CAPS_MAKE ("xBGR") ", " \
        "framerate = (fraction) [ 0, MAX ], " \
        "width = (int) [ 1, MAX ], " "height = (int) [ 1, MAX ]"


static GstStaticPadTemplate gst_drmsink_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_DRMSINK_TEMPLATE_CAPS)
    );

/* Class initialization. */

#define gst_drmsink_parent_class framebuffersink_parent_class
G_DEFINE_TYPE_WITH_CODE (GstDrmsink, gst_drmsink, GST_TYPE_FRAMEBUFFERSINK,
    GST_DEBUG_CATEGORY_INIT (gst_drmsink_debug_category, "drmsink", 0,
        "debug category for drmsink element"));


static void
gst_drmsink_class_init (GstDrmsinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstFramebufferSinkClass *framebuffer_sink_class =
      GST_FRAMEBUFFERSINK_CLASS (klass);

  gobject_class->set_property = gst_drmsink_set_property;
  gobject_class->get_property = gst_drmsink_get_property;

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_static_pad_template_get (&gst_drmsink_sink_template));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Optimized Linux console libdrm/KMS sink",
      "Sink/Video",
      "drm framebuffer sink", "Harm Hanemaaijer <fgenfb@yahoo.com>");

  g_object_class_install_property (gobject_class, PROP_CONNECTOR,
      g_param_spec_int ("connector", "Connector", "DRM connector id",
          0, G_MAXINT32, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ZERO_MEMCPY,
      g_param_spec_boolean ("zero-memcpy", "zero-memcpy",
          "Make drmsink propose a special allocator to upstream elements where "
          "the memory points to plane directly. If the allocator is decided to be used, "
          "the image width must match the FB width.", DEFAULT_ZERO_MEMCPY,
          G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_LCD,
      g_param_spec_boolean ("lcd", "lcd",
          "If lcd=true, plane creation and other lcd related configs are set.",
          DEFAULT_LCD, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_CX,
      g_param_spec_int ("cx", "cx", "offset of x in screen",
          -1, 1280, DEFAULT_CX, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_CY,
      g_param_spec_int ("cy", "cy", "offset of y in screen",
          -1, 720, DEFAULT_CY, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_CW,
      g_param_spec_int ("cw", "cw", "width of the plane in screen",
          -1, 1280, DEFAULT_CW, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_CH,
      g_param_spec_int ("ch", "ch", "height of the plane in screen",
          -1, 720, DEFAULT_CH, G_PARAM_READWRITE));


  framebuffer_sink_class->open_hardware =
      GST_DEBUG_FUNCPTR (gst_drmsink_open_hardware);
  framebuffer_sink_class->close_hardware =
      GST_DEBUG_FUNCPTR (gst_drmsink_close_hardware);
  framebuffer_sink_class->wait_for_vsync =
      GST_DEBUG_FUNCPTR (gst_drmsink_wait_for_vsync);
  framebuffer_sink_class->pan_display =
      GST_DEBUG_FUNCPTR (gst_drmsink_pan_display);
  framebuffer_sink_class->video_memory_allocator_new =
      GST_DEBUG_FUNCPTR (gst_drmsink_video_memory_allocator_new);
}

/* Class member functions. */

static void
gst_drmsink_init (GstDrmsink * drmsink)
{
  GstFramebufferSink *framebuffersink = GST_FRAMEBUFFERSINK (drmsink);

  drmsink->fd = -1;

  /* Override the default value of the device property from
     GstFramebufferSink. */
  framebuffersink->device = g_strdup (DEFAULT_DRM_DEVICE);
  /* Override the default value of the pan-does-vsync property from
     GstFramebufferSink. */
  framebuffersink->pan_does_vsync = TRUE;
  /* Override the default value of the preserve-par property from
     GstFramebufferSink. The option is not supported because drmsink
     doesn't support hardware scaling. */
  framebuffersink->preserve_par = FALSE;
  /* Override the default value of the hardware-overlay property from
     GstFramebufferSink. */
  framebuffersink->use_hardware_overlay_property = FALSE;

  /* Set the initial values of the properties. */
  drmsink->preferred_connector_id = -1;

  gst_drmsink_reset (drmsink);
}

void
gst_drmsink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDrmsink *drmsink = GST_DRMSINK (object);

  GST_DEBUG_OBJECT (drmsink, "set_property");

  g_return_if_fail (GST_IS_DRMSINK (object));

   switch (prop_id)
   {
    case PROP_CONNECTOR:
      drmsink->preferred_connector_id = g_value_get_int (value);
      break;
    case PROP_ZERO_MEMCPY:
    {
      drmsink->zero_memcpy = g_value_get_boolean (value);
      drmsink->framebuffersink.zeromemcpy = drmsink->zero_memcpy;
      break;
    }
    case PROP_LCD:
      drmsink->lcd = g_value_get_boolean (value);
      break;
    case PROP_CX:
      drmsink->cx = g_value_get_int (value);
      break;
    case PROP_CY:
      drmsink->cy = g_value_get_int (value);
      break;
    case PROP_CW:
      drmsink->cw = g_value_get_int (value);
      break;
    case PROP_CH:
      drmsink->ch = g_value_get_int (value);
      break;

    default:
      break;
  }

}

static void
gst_drmsink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDrmsink *drmsink = GST_DRMSINK (object);

  GST_DEBUG_OBJECT (drmsink, "get_property");
  g_return_if_fail (GST_IS_DRMSINK (object));


  switch (prop_id) {
    case PROP_CONNECTOR:
      g_value_set_int (value, drmsink->preferred_connector_id);
      break;

    case PROP_ZERO_MEMCPY:
      g_value_set_boolean (value, drmsink->zero_memcpy);
      break;

    case PROP_LCD:
      g_value_set_boolean (value, drmsink->lcd);
      break;

    case PROP_CX:
      g_value_set_int (value, drmsink->cx);
      break;

    case PROP_CY:
      g_value_set_int (value, drmsink->cy);
      break;

    case PROP_CW:
	  g_value_set_int (value, drmsink->cw);
	  break;

    case PROP_CH:
	  g_value_set_int (value, drmsink->ch);
	  break;

    default:
      break;
  }
}

static gboolean
gst_drmsink_find_mode_and_plane (GstDrmsink * drmsink, GstVideoRectangle * dim)
{
  drmModeConnector *connector;
  drmModeEncoder *encoder;
  drmModeModeInfo *mode;
  drmModePlane *plane;
  int i, pipe;
  gboolean ret;
  char s[80];

  ret = FALSE;
  encoder = NULL;

  /* First, find the connector & mode */
  connector = drmModeGetConnector (drmsink->fd, drmsink->connector_id);
  if (!connector)
    goto error_no_connector;

  if (connector->count_modes == 0)
    goto error_no_mode;

  g_sprintf (s, "Connected encoder: id = %u", connector->encoder_id);
  GST_INFO_OBJECT (drmsink, s);
  for (i = 0; i < connector->count_encoders; i++) {
    g_sprintf (s, "Available encoder: id = %u", connector->encoders[i]);
    GST_INFO_OBJECT (drmsink, s);
  }

  /* Now get the encoder */
  encoder = drmModeGetEncoder (drmsink->fd, connector->encoder_id);
  if (!encoder)
    goto error_no_encoder;

  /* XXX: just pick the first available mode, which has the highest
   * resolution. */
  mode = &connector->modes[0];
  memcpy (&drmsink->mode, &connector->modes[0], sizeof (connector->modes[0]));

  dim->x = dim->y = 0;
  dim->w = mode->hdisplay;
  dim->h = mode->vdisplay;
  GST_INFO_OBJECT (drmsink, "connector mode = %dx%d", dim->w, dim->h);

  drmsink->crtc_id = encoder->crtc_id;

  /* and figure out which crtc index it is: */
  pipe = -1;
  for (i = 0; i < drmsink->resources->count_crtcs; i++) {
    if (drmsink->crtc_id == drmsink->resources->crtcs[i]) {
      pipe = i;
      break;
    }
  }

  if (pipe == -1)
    goto error_no_crtc;

#ifdef USE_DRM_PLANES
  for (i = 2; i < drmsink->plane_resources->count_planes; i++) {
    plane = drmModeGetPlane (drmsink->fd, drmsink->plane_resources->planes[i]);
    if (plane->possible_crtcs & (1 << pipe)) {
      drmsink->plane = plane;
      break;
    } else {
      drmModeFreePlane (plane);
    }
  }

  drmsink->plane->crtc_x = drmsink->cx;
  drmsink->plane->crtc_y = drmsink->cy;

  if (!drmsink->plane)
    goto error_no_plane;
#endif

  ret = TRUE;

fail:
  if (encoder)
    drmModeFreeEncoder (encoder);

  if (connector)
    drmModeFreeConnector (connector);

  return ret;

error_no_connector:
  GST_ERROR_OBJECT (drmsink, "could not get connector (%d): %s",
      drmsink->connector_id, strerror (errno));
  goto fail;

error_no_mode:
  GST_ERROR_OBJECT (drmsink, "could not find a valid mode (count_modes %d)",
      connector->count_modes);
  goto fail;

error_no_encoder:
  GST_ERROR_OBJECT (drmsink, "could not get encoder: %s", strerror (errno));
  goto fail;

error_no_crtc:
  GST_ERROR_OBJECT (drmsink, "couldn't find a crtc");
  goto fail;

error_no_plane:
  GST_ERROR_OBJECT (drmsink, "couldn't find a plane");
  goto fail;
}

static void
gst_drmsink_reset (GstDrmsink * drmsink)
{

#ifdef USE_DRM_PLANES
  if (drmsink->plane) {
    drmModeFreePlane (drmsink->plane);
    drmsink->plane = NULL;
  }

  if (drmsink->plane_resources) {
    drmModeFreePlaneResources (drmsink->plane_resources);
    drmsink->plane_resources = NULL;
  }
#endif

  if (drmsink->resources) {
    drmModeFreeResources (drmsink->resources);
    drmsink->resources = NULL;
  }


  if (drmsink->fd != -1) {
    close (drmsink->fd);
    drmsink->fd = -1;
  }

  memset (&drmsink->screen_rect, 0, sizeof (GstVideoRectangle));

  drmsink->connector_id = -1;
}

static gboolean
gst_drmsink_open_hardware (GstFramebufferSink * framebuffersink,
    GstVideoInfo * info, gsize * video_memory_size,
    gsize * pannable_video_memory_size)
{
  GstDrmsink *drmsink = GST_DRMSINK (framebuffersink);

  drmModeConnector *connector = NULL;
  int i;
  int res;
  uint64_t has_dumb_buffers;
  gsize size;
  gchar *s;

  if (!drmAvailable ()) {
    GST_DRMSINK_MESSAGE_OBJECT (drmsink, "No kernel DRM driver loaded");
    return FALSE;
  }

  /* Open drm device. */
  drmsink->fd = open (framebuffersink->device, O_RDWR | O_CLOEXEC);
  if (drmsink->fd < 0) {
    s = g_strdup_printf ("Cannot open DRM device %s", framebuffersink->device);
    GST_DRMSINK_MESSAGE_OBJECT (drmsink, s);
    g_free (s);
    return FALSE;
  }

  res = drmGetCap (drmsink->fd, DRM_CAP_DUMB_BUFFER, &has_dumb_buffers);
  if (res < 0 || !has_dumb_buffers) {
    GST_DRMSINK_MESSAGE_OBJECT (drmsink,
        "DRM device does not support dumb buffers");
    return FALSE;
  }

  drmsink->resources = drmModeGetResources (drmsink->fd);
  if (drmsink->resources == NULL)
    goto resources_failed;

#if 0
  /* Print an overview of detected connectors/modes. */
  for (i = 0; i < drmsink->resources->count_connectors; i++) {
    int j;
    connector = drmModeGetConnector (drmsink->fd,
        drmsink->resources->connectors[i]);
    s = g_strdup_printf
        ("DRM connector found, id = %d, type = %d, connected = %d",
        connector->connector_id, connector->connector_type,
        connector->connection == DRM_MODE_CONNECTED);
    GST_INFO_OBJECT (drmsink, s);
    g_free (s);

    for (j = 0; j < connector->count_modes; j++) {
      s = g_strdup_printf ("Supported mode %s", connector->modes[j].name);
      GST_INFO_OBJECT (drmsink, s);
      g_free (s);
    }
    drmModeFreeConnector (connector);
  }
#endif

  if (drmsink->preferred_connector_id >= 0) {
    /* Connector specified as property. */
    for (i = 0; i < drmsink->resources->count_connectors; i++) {
      connector = drmModeGetConnector (drmsink->fd,
          drmsink->resources->connectors[i]);
      if (!connector)
        continue;

      if (connector->connector_id == drmsink->preferred_connector_id)
        break;

      drmModeFreeConnector (connector);
    }

    if (i == drmsink->resources->count_connectors) {
      GST_DRMSINK_MESSAGE_OBJECT (drmsink, "Specified DRM connector not found");
      drmModeFreeResources (drmsink->resources);
      return FALSE;
    }

    drmsink->connector_id = drmsink->preferred_connector_id;
  } else {
    /* Look for active connectors. */
    for (i = 0; i < drmsink->resources->count_connectors; i++) {
      connector = drmModeGetConnector (drmsink->fd,
          drmsink->resources->connectors[i]);
      if (!connector)
        continue;

      if (connector->connection == DRM_MODE_CONNECTED &&
          connector->count_modes > 0)
        break;

      drmModeFreeConnector (connector);
    }

    if (i == drmsink->resources->count_connectors) {
      GST_DRMSINK_MESSAGE_OBJECT (drmsink,
          "No currently active DRM connector found");
      drmModeFreeResources (drmsink->resources);
      return FALSE;
    }

    drmsink->connector_id = connector->connector_id;
  }

#ifdef USE_DRM_PLANES
  drmsink->plane_resources = drmModeGetPlaneResources (drmsink->fd);
  if (drmsink->plane_resources == NULL)
    goto plane_resources_failed;
#endif

  gst_drmsink_find_mode_and_plane (drmsink, &drmsink->screen_rect);


  drmsink->crtc_mode_initialized = FALSE;
  drmsink->set_plane_initialized = FALSE;
  drmsink->saved_crtc = drmModeGetCrtc (drmsink->fd, drmsink->crtc_id);

  drmsink->event_context = g_slice_new (drmEventContext);
  drmsink->event_context->version = DRM_EVENT_CONTEXT_VERSION;
  drmsink->event_context->vblank_handler = gst_drmsink_vblank_handler;
  drmsink->event_context->page_flip_handler = gst_drmsink_page_flip_handler;
  drmsink->page_flip_occurred = FALSE;
  drmsink->page_flip_pending = FALSE;

#if 0
  drmModeFreeResources (resources);

  /* Create libkms driver. */

  ret = kms_create (drmsink->fd, &drmsink->drv);
  if (ret) {
    GST_DRMSINK_MESSAGE_OBJECT (drmsink, "kms_create() failed");
    return FALSE;
  }
#endif

  gst_video_info_set_format (info, GST_VIDEO_FORMAT_BGRx,
      drmsink->screen_rect.w, drmsink->screen_rect.h);
  size = GST_VIDEO_INFO_COMP_STRIDE (info, 0) * GST_VIDEO_INFO_HEIGHT (info);

  /* GstFramebufferSink expects the amount of usable video memory to be
     be set. Because DRM doesn't really allow querying of available video
     memory, assume three screen buffers are available and rely on a specific
     setting of the video-memory property in order to use more video memory. */
  *video_memory_size = size * 3 + 1024;
  if (framebuffersink->max_video_memory_property > 0)
    *video_memory_size = (guint64) framebuffersink->max_video_memory_property
        * 1024 * 1024;
  *pannable_video_memory_size = *video_memory_size;

  s = g_strdup_printf ("Successfully initialized DRM, connector = %d, "
      "mode = %dx%d",
      drmsink->connector_id, drmsink->screen_rect.w, drmsink->screen_rect.h);
  GST_DRMSINK_MESSAGE_OBJECT (drmsink, s);
  g_free (s);

  return TRUE;

fail:
  gst_drmsink_reset (drmsink);
  return FALSE;

resources_failed:
  GST_ELEMENT_ERROR (drmsink, RESOURCE, FAILED,
      (NULL), ("drmModeGetResources failed: %s (%d)", strerror (errno), errno));
  goto fail;

plane_resources_failed:
  GST_ELEMENT_ERROR (drmsink, RESOURCE, FAILED,
      (NULL), ("drmModeGetPlaneResources failed: %s (%d)",
          strerror (errno), errno));
  goto fail;
}

static void
gst_drmsink_close_hardware (GstFramebufferSink * framebuffersink)
{
  GstDrmsink *drmsink = GST_DRMSINK (framebuffersink);

  gst_drmsink_flush_drm_events (drmsink);
  gst_drmsink_wait_pending_drm_events (drmsink);

  drmModeSetCrtc (drmsink->fd, drmsink->saved_crtc->crtc_id,
      drmsink->saved_crtc->buffer_id, drmsink->saved_crtc->x,
      drmsink->saved_crtc->y, &drmsink->connector_id, 1,
      &drmsink->saved_crtc->mode);
  drmModeFreeCrtc (drmsink->saved_crtc);

  gst_drmsink_reset (drmsink);

  GST_DRMSINK_MESSAGE_OBJECT (drmsink, "Closed DRM device");

  return;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  /* Remember to set the rank if it's an element that is meant
     to be autoplugged by decodebin. */
  return gst_element_register (plugin, "drmsink", GST_RANK_NONE,
      GST_TYPE_DRMSINK);
}

/* these are normally defined by the GStreamer build system.
   If you are creating an element to be included in gst-plugins-*,
   remove these, as they're always defined.  Otherwise, edit as
   appropriate for your external plugin package. */
#ifndef VERSION
#define VERSION "0.1"
#endif
#ifndef PACKAGE
#define PACKAGE "gstdrmsink"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "gstreamer1.0-fbdev2-plugins"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "https://github.com/hglm"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    drmsink,
    "Optimized Linux console libdrm/KMS sink",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)


/* DRM video memory allocator. */
     typedef struct
     {
       GstAllocator parent;
       GstDrmsink *drmsink;
       int w;
       int h;
       GstVideoFormatInfo format_info;
       /* The amount of video memory allocated. */
       gsize total_allocated;
     } GstDrmSinkVideoMemoryAllocator;

     typedef struct
     {
       GstAllocatorClass parent_class;
     } GstDrmSinkVideoMemoryAllocatorClass;

     GType gst_drmsink_video_memory_allocator_get_type (void);
G_DEFINE_TYPE (GstDrmSinkVideoMemoryAllocator,
    gst_drmsink_video_memory_allocator, GST_TYPE_ALLOCATOR);

     typedef struct
     {
       GstMemory mem;
       struct drm_mode_create_dumb creq;
       struct drm_mode_map_dumb mreq;
       uint32_t fb;
       gpointer map_address;
       gboolean allocated;
     } GstDrmSinkVideoMemory;

#ifdef LAZY_ALLOCATION
/* With lazy allocation, don't allocate video memory immediately, but wait
   until the first memory_map call. */
     static GstMemory *gst_drmsink_video_memory_allocator_alloc (GstAllocator *
    allocator, gsize size, GstAllocationParams * allocation_params)
{
  GstDrmSinkVideoMemory *mem;
  /* Always ignore allocation_params, but use word alignment. */
  int align = 3;
  mem = g_slice_new (GstDrmSinkVideoMemory);
  gst_memory_init (GST_MEMORY_CAST (mem), GST_MEMORY_FLAG_NO_SHARE |
      GST_MEMORY_FLAG_VIDEO_MEMORY, allocator, NULL, size, align, 0, size);
  mem->allocated = FALSE;
  mem->map_address = NULL;
  return GST_MEMORY_CAST (mem);
}
#endif

#ifdef LAZY_ALLOCATION
static GstMemory *
gst_drmsink_video_memory_allocator_alloc_actual (GstAllocator * allocator,
    gsize size, GstAllocationParams * params, GstDrmSinkVideoMemory * mem)
{
#else
static GstMemory *
gst_drmsink_video_memory_allocator_alloc (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
  GstDrmSinkVideoMemory *mem;
#endif
  GstDrmSinkVideoMemoryAllocator *drmsink_video_memory_allocator =
      (GstDrmSinkVideoMemoryAllocator *) allocator;
  struct drm_mode_destroy_dumb dreq;
  int ret;
  /* Ignore params (which should be NULL) and use word alignment. */
  int align = 3;
  int i;
  int depth;
  unsigned long physaddress;

  GST_OBJECT_LOCK (allocator);

#ifndef LAZY_ALLOCATION
  mem = g_slice_new (GstDrmSinkVideoMemory);
#endif

  mem->creq.height = drmsink_video_memory_allocator->h;
  mem->creq.width = drmsink_video_memory_allocator->w;
  mem->creq.bpp = GST_VIDEO_FORMAT_INFO_PSTRIDE
		  (&drmsink_video_memory_allocator->format_info, 0) * 8;
  mem->creq.flags = 0;

  /* handle, pitch and size will be returned in the creq struct. */
  ret = drmIoctl (drmsink_video_memory_allocator->drmsink->fd,
      DRM_IOCTL_MODE_CREATE_DUMB, &mem->creq);
  if (ret < 0) {
    GST_DRMSINK_MESSAGE_OBJECT (drmsink_video_memory_allocator->drmsink,
        "Creating dumb drm buffer failed");
#ifndef LAZY_ALLOCATION
    g_slice_free (GstDrmSinkVideoMemory, mem);
#endif
    GST_OBJECT_UNLOCK (allocator);
    return NULL;
  }

  depth = 0;
  for (i = 0;
      i <
      GST_VIDEO_FORMAT_INFO_N_COMPONENTS
      (&drmsink_video_memory_allocator->format_info); i++)
    depth +=
        GST_VIDEO_FORMAT_INFO_DEPTH
        (&drmsink_video_memory_allocator->format_info, i);


  /* create framebuffer object for the dumb-buffer */
  ret = drmModeAddFB (drmsink_video_memory_allocator->drmsink->fd,
      drmsink_video_memory_allocator->w, drmsink_video_memory_allocator->h,
      depth,
      GST_VIDEO_FORMAT_INFO_PSTRIDE
      (&drmsink_video_memory_allocator->format_info, 0) * 8, mem->creq.pitch,
      mem->creq.handle, &mem->fb);
  if (ret) {
    /* frame buffer creation failed; see "errno" */
    GST_DRMSINK_MESSAGE_OBJECT (drmsink_video_memory_allocator->drmsink,
        "DRM framebuffer creation failed.\n");
    goto fail_destroy;
  }

  /* Atmel specific IOCTL to get the physical address of gem object
     This is very much required as the decoder API is expecting
     physical address of buffer, Otherwise memcpy operation
     is performed which affects the overall system performance */
    memset (&mem->mreq, 0, sizeof (mem->mreq));
    mem->mreq.handle = mem->creq.handle;
    ret = drmIoctl(drmsink_video_memory_allocator->drmsink->fd,
    	DRM_IOCTL_ATMEL_GEM_GET,&mem->mreq);
    if (ret) {
      GST_DRMSINK_MESSAGE_OBJECT (drmsink_video_memory_allocator->drmsink,
          "DRM buffer get physical address failed.\n");
      drmModeRmFB (drmsink_video_memory_allocator->drmsink->fd, mem->creq.handle);
      goto fail_destroy;
    }

    physaddress = (unsigned int) mem->mreq.offset;
    gst_g1_gem_set_physical ((unsigned int) physaddress);


  /* the framebuffer "fb" can now used for scanout with KMS */

  /* prepare buffer for memory mapping */
  memset (&mem->mreq, 0, sizeof (mem->mreq));
  mem->mreq.handle = mem->creq.handle;
  mem->mreq.offset = (unsigned long long) 0xAA;
  ret = drmIoctl (drmsink_video_memory_allocator->drmsink->fd,
      DRM_IOCTL_MODE_MAP_DUMB, &mem->mreq);
  if (ret) {
    GST_DRMSINK_MESSAGE_OBJECT (drmsink_video_memory_allocator->drmsink,
        "DRM buffer preparation failed.\n");
    drmModeRmFB (drmsink_video_memory_allocator->drmsink->fd, mem->creq.handle);
    goto fail_destroy;
  }

  /* mem->mreq.offset now contains the new offset that can be used with mmap */

  /* perform actual memory mapping */
  mem->map_address = mmap (0, mem->creq.size, PROT_READ | PROT_WRITE,
      MAP_SHARED,
      drmsink_video_memory_allocator->drmsink->fd, mem->mreq.offset);

  if (mem->map_address == MAP_FAILED) {
    /* memory-mapping failed; see "errno" */
    GST_DRMSINK_MESSAGE_OBJECT (drmsink_video_memory_allocator->drmsink,
        "Memory mapping of DRM buffer failed.\n");
    drmModeRmFB (drmsink_video_memory_allocator->drmsink->fd, mem->creq.handle);
    goto fail_destroy;
  }


#ifndef LAZY_ALLOCATION
  gst_memory_init (GST_MEMORY_CAST (mem), GST_MEMORY_FLAG_NO_SHARE |
      GST_MEMORY_FLAG_VIDEO_MEMORY,
      (GstAllocator *) drmsink_video_memory_allocator, NULL, size, align, 0,
      size);
#endif

  drmsink_video_memory_allocator->total_allocated += size;

  GST_INFO_OBJECT (drmsink_video_memory_allocator->drmsink,
      "Allocated video memory buffer of size %zd at %p, align %d, mem = %p\n",
      size, mem->map_address, align, mem);

  GST_OBJECT_UNLOCK (allocator);

  return (GstMemory *) mem;

fail_destroy:

  dreq.handle = mem->creq.handle;
  drmIoctl (drmsink_video_memory_allocator->drmsink->fd,
      DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
#ifndef LAZY_ALLOCATION
  g_slice_free (GstDrmSinkVideoMemory, mem);
#endif
  GST_OBJECT_UNLOCK (allocator);
  return NULL;
}

static void
gst_drmsink_video_memory_allocator_free (GstAllocator * allocator,
    GstMemory * mem)
{
  GstDrmSinkVideoMemoryAllocator *drmsink_video_memory_allocator =
      (GstDrmSinkVideoMemoryAllocator *) allocator;
  GstDrmSinkVideoMemory *vmem = (GstDrmSinkVideoMemory *) mem;
  struct drm_mode_destroy_dumb dreq;

  GST_INFO_OBJECT (drmsink_video_memory_allocator->drmsink,
      "video_memory_allocator_free called, address = %p\n", vmem->map_address);

#ifdef LAZY_ALLOCATION
  if (!vmem->allocated) {
    g_slice_free (GstDrmSinkVideoMemory, vmem);
    return;
  }
#endif

  drmsink_video_memory_allocator->total_allocated -= mem->size;

  munmap (vmem->map_address, vmem->creq.size);
  dreq.handle = vmem->creq.handle;
  drmIoctl (drmsink_video_memory_allocator->drmsink->fd,
      DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);

  g_slice_free (GstDrmSinkVideoMemory, vmem);

  GST_DEBUG ("%p: freed", vmem);
}

static gpointer
gst_drmsink_video_memory_map (GstMemory * mem, gsize maxsize, GstMapFlags flags)
{
  GstDrmSinkVideoMemory *vmem = (GstDrmSinkVideoMemory *) mem;
  GST_DEBUG ("video_memory_map called, mem = %p, maxsize = %d, flags = %d, "
      "data = %p\n", mem, maxsize, flags, vmem->map_address);

  if (flags & GST_MAP_READ)
    GST_DEBUG ("Mapping video memory for reading is slow.\n");

#ifdef LAZY_ALLOCATION
  if (!vmem->allocated) {
    if (!gst_drmsink_video_memory_allocator_alloc_actual (mem->allocator,
            mem->maxsize, NULL, vmem))
      return NULL;
    vmem->allocated = TRUE;
  }
#endif

  return vmem->map_address;
}

static gboolean
gst_drmsink_video_memory_unmap (GstMemory * mem)
{
  GST_DEBUG ("%p: unmapped", mem);
  return TRUE;
}


static void
    gst_drmsink_video_memory_allocator_class_init
    (GstDrmSinkVideoMemoryAllocatorClass * klass)
{
  GstAllocatorClass *allocator_class = GST_ALLOCATOR_CLASS (klass);

  allocator_class->alloc = gst_drmsink_video_memory_allocator_alloc;
  allocator_class->free = gst_drmsink_video_memory_allocator_free;
}

static void
gst_drmsink_video_memory_allocator_init (GstDrmSinkVideoMemoryAllocator *
    video_memory_allocator)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (video_memory_allocator);

  alloc->mem_type = "drmsink_video_memory";
  alloc->mem_map = (GstMemoryMapFunction) gst_drmsink_video_memory_map;
  alloc->mem_unmap = (GstMemoryUnmapFunction) gst_drmsink_video_memory_unmap;
}

static GstAllocator *
gst_drmsink_video_memory_allocator_new (GstFramebufferSink * framebuffersink,
    GstVideoInfo * info, gboolean pannable, gboolean is_overlay)
{
  GstDrmsink *drmsink = GST_DRMSINK (framebuffersink);
  GstDrmSinkVideoMemoryAllocator *drmsink_video_memory_allocator =
      g_object_new (gst_drmsink_video_memory_allocator_get_type (), NULL);
  gchar s[128];
  gchar *str;
  drmsink_video_memory_allocator->drmsink = drmsink;
  drmsink_video_memory_allocator->w = GST_VIDEO_INFO_WIDTH (info);
  drmsink_video_memory_allocator->h = GST_VIDEO_INFO_HEIGHT (info);
  drmsink_video_memory_allocator->format_info =
      *(GstVideoFormatInfo *) info->finfo;
  drmsink_video_memory_allocator->total_allocated = 0;
  g_sprintf (s, "drmsink_video_memory_%p", drmsink_video_memory_allocator);
  gst_allocator_register (s, gst_object_ref (drmsink_video_memory_allocator));
  str = g_strdup_printf ("Created video memory allocator %s, %dx%d, format %s",
      s, drmsink_video_memory_allocator->w, drmsink_video_memory_allocator->h,
      gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (info)));
  GST_INFO_OBJECT (drmsink, str);
  g_free (str);
  return GST_ALLOCATOR_CAST (drmsink_video_memory_allocator);
}

/* DRM event related functions. */

static void
gst_drmsink_vblank_handler (int fd, unsigned int sequence, unsigned int tv_sec,
    unsigned int tv_usec, void *user_data)
{
}

static void
gst_drmsink_page_flip_handler (int fd, unsigned int sequence,
    unsigned int tv_sec, unsigned int tv_usec, void *user_data)
{
  GstDrmsink *drmsink = (GstDrmsink *) user_data;
  drmsink->page_flip_occurred = TRUE;
  if (drmsink->page_flip_pending)
    drmsink->page_flip_pending = FALSE;
}

/* Flush queued drm events. */

static void
gst_drmsink_flush_drm_events (GstDrmsink * drmsink)
{
  fd_set fds;
  struct timeval tv;
  /* Set the timeout to zero. */
  memset (&tv, 0, sizeof (tv));
  FD_ZERO (&fds);
  while (TRUE) {
    FD_SET (drmsink->fd, &fds);
    select (drmsink->fd + 1, &fds, NULL, NULL, &tv);
    if (FD_ISSET (drmsink->fd, &fds))
      drmHandleEvent (drmsink->fd, drmsink->event_context);
    else
      break;
  }
}

/* Wait until all pending page flips have finished. */

static void
gst_drmsink_wait_pending_drm_events (GstDrmsink * drmsink)
{
  fd_set fds;
  struct timeval tv;
  /* Set the timeout to zero. */
  memset (&tv, 0, sizeof (tv));
  FD_ZERO (&fds);
  while (drmsink->page_flip_pending) {
    FD_SET (drmsink->fd, &fds);
    tv.tv_sec = 5;
    select (drmsink->fd + 1, &fds, NULL, NULL, &tv);
    if (FD_ISSET (drmsink->fd, &fds))
      drmHandleEvent (drmsink->fd, drmsink->event_context);
    else
      break;
  }
}

int divRoundClosest(const int n, const int d)
{
  return ((n < 0) ^ (d < 0)) ? ((n - d/2)/d) : ((n + d/2)/d);
}

static void
gst_drmsink_pan_display (GstFramebufferSink * framebuffersink,
    GstMemory * memory)
{
  GstDrmsink *drmsink = GST_DRMSINK (framebuffersink);
  GstDrmSinkVideoMemory *vmem = (GstDrmSinkVideoMemory *) memory;
  uint32_t connectors[1];
  uint32_t cx, cy, cw, ch, sx, sy, sw, sh;

  GST_LOG_OBJECT (framebuffersink,
      "pan_display called, mem = %p, map_address = %p",
      vmem, vmem->map_address);

  if (drmsink->lcd) {

   cx = drmsink->plane->crtc_x;
   cy = drmsink->plane->crtc_y;
   cw=(divRoundClosest(drmsink->cw, 16)*16);
   ch=(divRoundClosest(drmsink->ch, 16)*16);

   if(cw == DEFAULT_CW || ch == DEFAULT_CH)
   {
	  /*width or height is zero, make it full screen*/
	   cx = 0;
	   cy = 0;
	   cw = drmsink->mode.hdisplay;
	   ch = drmsink->mode.vdisplay;

   }

   sx = 0;
   sy = 0;
   sw = (drmsink->mode.hdisplay << 16);
   sh = (drmsink->mode.vdisplay << 16);

    if (!drmsink->set_plane_initialized) {
      if (drmModeSetPlane (drmsink->fd, drmsink->plane->plane_id,
              drmsink->crtc_id, vmem->fb, 0, cx, cy, cw, ch,sx, sy, sw, sh)) {
        GST_ERROR_OBJECT (drmsink, "drmModeSetPlane failed");
        return;
      }
      drmsink->set_plane_initialized = TRUE;
    }

  } else {

	  if (!drmsink->crtc_mode_initialized) {
		connectors[0] = drmsink->connector_id;
		if (drmModeSetCrtc (drmsink->fd, drmsink->crtc_id,vmem->fb, 0, 0,
				connectors, 1, &drmsink->mode)) {
		  GST_ERROR_OBJECT (drmsink, "drmModeSetCrtc failed");
		  return;
		}
		drmsink->crtc_mode_initialized = TRUE;
	  }

  }

  gst_drmsink_flush_drm_events (drmsink);

  if (drmsink->page_flip_pending) {
    GST_INFO_OBJECT (drmsink,
        "pan_display: previous page flip still pending, skipping");
    return;
  }

  drmsink->page_flip_occurred = FALSE;
  drmsink->page_flip_pending = TRUE;
  if (drmModePageFlip (drmsink->fd, drmsink->crtc_id,vmem->fb,
          DRM_MODE_PAGE_FLIP_EVENT, drmsink)) {
    GST_ERROR_OBJECT (drmsink, "drmModePageFlip failed");
    return;
  }
}

static void
gst_drmsink_wait_for_vsync (GstFramebufferSink * framebuffersink)
{
  GstDrmsink *drmsink = GST_DRMSINK (framebuffersink);
  drmVBlank vbl;

  GST_INFO_OBJECT (drmsink, "wait_for_vsync called");

  drmsink->vblank_occurred = FALSE;
  vbl.request.type = DRM_VBLANK_RELATIVE | DRM_VBLANK_EVENT;
  vbl.request.sequence = 1;
  drmWaitVBlank (drmsink->fd, &vbl);
}
