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
#ifndef _GST_DWL_ALLOCATOR_H_
#define _GST_DWL_ALLOCATOR_H_

#include <gst/gst.h>
#include <gst/gstmemory.h>
#include <dwl.h>

#include "gstg1allocator.h"

G_BEGIN_DECLS
#define GST_ALLOCATOR_DWL "DwlMemoryAllocator"

#define GST_TYPE_DWL_ALLOCATOR \
  (gst_dwl_allocator_get_type())
#define GST_IS_DWL_ALLOCATOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DWL_ALLOCATOR))
#define GST_IS_DWL_ALLOCATOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DWL_ALLOCATOR))

GType gst_dwl_allocator_get_type (void);

/**
 * Creates and initializes a new allocator singleton
 */
void gst_dwl_allocator_new (void);

G_END_DECLS
#endif /*_GST_DWL_ALLOCATOR_H_*/
