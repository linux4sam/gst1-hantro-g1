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
#ifndef __GST_G1_ENUM_H__
#define __GST_G1_ENUM_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_G1_ENUM_ROTATION_TYPE (gst_g1_enum_rotation_get_type())
GType gst_g1_enum_rotation_get_type (void);

G_END_DECLS
#endif //__GST_G1_ENUM_H__
