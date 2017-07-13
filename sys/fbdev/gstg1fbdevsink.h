/* GStreamer
 * Copyright (C) 2007 Sean D'Epagnier sean@depagnier.com
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */


#ifndef __GST_G1_FBDEVSINK_H__
#define __GST_G1_FBDEVSINK_H__

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <gst/video/video.h>

#include <linux/fb.h>

G_BEGIN_DECLS
#define GST_TYPE_G1_FBDEVSINK \
  (gst_g1_fbdevsink_get_type())
#define GST_G1_FBDEVSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_G1_FBDEVSINK,GstG1FBDEVSink))
#define GST_G1_FBDEVSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_G1_FBDEVSINK,GstG1FBDEVSinkClass))
#define GST_IS_G1_FBDEVSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_G1_FBDEVSINK))
#define GST_IS_G1_FBDEVSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_G1_FBDEVSINK))
typedef struct _GstG1FBDEVSink GstG1FBDEVSink;
typedef struct _GstG1FBDEVSinkClass GstG1FBDEVSinkClass;

struct _GstG1FBDEVSink
{
  GstVideoSink videosink;

  /*< private > */
  struct fb_fix_screeninfo fixinfo;
  struct fb_var_screeninfo varinfo;

  int fd;
  unsigned char *framebuffer;

  char *device;

  int width, height;
  int cx, cy, linelen, lines, bytespp;

  int fps_n, fps_d;

  GstAllocator *allocator;
  gboolean zero_memcpy;
};

struct _GstG1FBDEVSinkClass
{
  GstVideoSinkClass videosink_class;

};

GType gst_g1_fbdevsink_get_type (void);

G_END_DECLS
#endif /* __GST_G1_FBDEVSINK_H__ */
