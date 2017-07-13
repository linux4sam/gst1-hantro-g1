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
#ifndef _GST_G1_ALLOCATOR_H_
#define _GST_G1_ALLOCATOR_H_

#include <gst/gst.h>
#include <gst/gstmemory.h>

G_BEGIN_DECLS
#define GST_TYPE_G1_ALLOCATOR \
  (gst_g1_allocator_get_type())
#define GST_IS_G1_ALLOCATOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_G1_ALLOCATOR))
#define GST_IS_G1_ALLOCATOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_G1_ALLOCATOR))
#define GST_G1_ALLOCATOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_G1_ALLOCATOR,GstG1Allocator))
#define GST_G1_ALLOCATOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_G1_ALLOCATOR,GstG1AllocatorClass))
typedef struct _GstG1Memory GstG1Memory;
typedef struct _GstG1Allocator GstG1Allocator;
typedef struct _GstG1AllocatorClass GstG1AllocatorClass;

struct _GstG1Memory
{
  GstMemory mem;

  gpointer virtaddress;
  guint32 physaddress;
};

struct _GstG1Allocator
{
  GstAllocator parent;
};

struct _GstG1AllocatorClass
{
  GstAllocatorClass parent_class;
};

GType gst_g1_allocator_get_type (void);

/**
 * Returns the physical address of the memory
 *
 * @param mem The GstMemory to query the physical address from. This
 * memory must have been allocated with the GstG1Allocator or subclass.
 *
 * @return The physical address of the data or 0 if the mem was not
 * allocated by a G1 allocator.
 */
guint32 gst_g1_allocator_get_physical (GstMemory * mem);

G_END_DECLS
#endif /*_GST_G1_ALLOCATOR_H_*/
