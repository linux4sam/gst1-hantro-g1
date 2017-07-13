/* GStreamer G1 plugin
 *
 * Copyright (C) 2014-2015  Atmel Corporation.
 * Copyright (C) 2017 Microchip Technology Inc.
 *			 Sandeep Sheriker M <sandeepsheriker.mallikarjun@microchip.com>
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
#include <gst/gst.h>

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gstg1h264dec.h"
#include "gstg1mp4dec.h"
#include "gstdwlallocator.h"

/* Register of all the elements of the plugin */
static gboolean
plugin_init (GstPlugin * plugin)
{
  /* Register allocator */
  gst_dwl_allocator_new ();

  if (!gst_element_register (plugin, "g1h264dec", GST_RANK_PRIMARY,
          GST_TYPE_G1_H264_DEC))
    return FALSE;
  if (!gst_element_register (plugin, "g1mp4dec", GST_RANK_PRIMARY,
          GST_TYPE_G1_MP4_DEC))
    return FALSE;

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR, g1,
    "GStreamer plug-in supporting the Hantro G1 HW accelerated decoder",
    plugin_init, PACKAGE_VERSION, GST_LICENSE, PACKAGE_NAME, PACKAGE_URL)
