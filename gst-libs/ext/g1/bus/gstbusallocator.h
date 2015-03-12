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
#ifndef _GST_BUS_ALLOCATOR_H_
#define _GST_BUS_ALLOCATOR_H_

#include <gst/gst.h>
#include <gst/gstmemory.h>

#include "gstg1allocator.h"

G_BEGIN_DECLS
#define GST_ALLOCATOR_BUS "BusMemoryAllocator"

#define GST_TYPE_BUS_ALLOCATOR \
  (gst_bus_allocator_get_type())
#define GST_IS_BUS_ALLOCATOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BUS_ALLOCATOR))
#define GST_IS_BUS_ALLOCATOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BUS_ALLOCATOR))

GType gst_bus_allocator_get_type (void);

/**
 * Creates and initializes a new bus allocator singleton
 *
 * @param physaddress The physical address to return as allocated memory
 * @param size The size of the data
 */
void gst_bus_allocator_new (guint32 physaddress, gsize size);

G_END_DECLS
#endif /*_GST_BUS_ALLOCATOR_H_*/
