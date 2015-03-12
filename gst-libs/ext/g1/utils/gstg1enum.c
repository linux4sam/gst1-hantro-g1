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
#include <ppapi.h>
#include "gstg1enum.h"

GType
gst_g1_enum_rotation_get_type ()
{
  static GType rotation_type = 0;

  static const GEnumValue rotation_types[] = {
    {PP_ROTATION_NONE, "None/0 deg", "0deg"},
    {PP_ROTATION_LEFT_90, "90 deg/-270 deg/90 deg CCW", "90deg"},
    {PP_ROTATION_180, "180 deg/180CW", "180deg"},
    {PP_ROTATION_RIGHT_90, "270 deg/-90 deg/90 deg CW", "270deg"},
    {PP_ROTATION_HOR_FLIP, "Horizontal Flip/Mirror", "horflip"},
    {PP_ROTATION_VER_FLIP, "Vertical Flip/Mirror", "verflip"},
    {0, NULL, NULL}
  };

  if (!rotation_type) {
    rotation_type =
        g_enum_register_static ("GstG1EnumRotationType", rotation_types);
  }
  return rotation_type;
}
