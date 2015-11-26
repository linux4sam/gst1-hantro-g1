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
#include <string.h>

#include "gstg1allocator.h"
int g1_gem_physical_addr;

GST_DEBUG_CATEGORY_EXTERN (GST_CAT_MEMORY);

GST_DEBUG_CATEGORY_STATIC (gst_g1_allocator_debug);
#define GST_CAT_DEFAULT gst_g1_allocator_debug

static gpointer gst_g1_allocator_map (GstMemory * mem, gsize maxsize,
    GstMapFlags flags);
static void gst_g1_allocator_unmap (GstMemory * mem);

G_DEFINE_TYPE (GstG1Allocator, gst_g1_allocator, GST_TYPE_ALLOCATOR);

static void
gst_g1_allocator_class_init (GstG1AllocatorClass * klass)
{
  GST_DEBUG_CATEGORY_INIT (gst_g1_allocator_debug, "g1allocator",
      0, "G1 Memory Allocator");
}

static void
gst_g1_allocator_init (GstG1Allocator * allocator)
{
  GstAllocator *alloc = GST_ALLOCATOR (allocator);
  GST_CAT_DEBUG (GST_CAT_MEMORY, "init allocator %p", allocator);

  /* Instance specific functions */
  alloc->mem_map = GST_DEBUG_FUNCPTR (gst_g1_allocator_map);
  alloc->mem_unmap = GST_DEBUG_FUNCPTR (gst_g1_allocator_unmap);
}

static gpointer
gst_g1_allocator_map (GstMemory * mem, gsize maxsize, GstMapFlags flags)
{
  GstG1Memory *g1mem;

  g1mem = (GstG1Memory *) mem;

  GST_LOG ("Mapping memory, virtual: %p physical: 0x%08x",
      g1mem->virtaddress, g1mem->physaddress);

  g_return_val_if_fail (GST_IS_G1_ALLOCATOR (mem->allocator), NULL);

  return g1mem->virtaddress;
}

static void
gst_g1_allocator_unmap (GstMemory * mem)
{
  GstG1Memory *g1mem;

  g1mem = (GstG1Memory *) mem;

  g_return_if_fail (GST_IS_G1_ALLOCATOR (mem->allocator));

  GST_LOG ("Unmapping memory, virtual: %p physical: 0x%08x",
      g1mem->virtaddress, g1mem->physaddress);
}

guint32
gst_g1_allocator_get_physical (GstMemory * mem)
{
  GstG1Memory *g1mem;

  g_return_val_if_fail (GST_IS_G1_ALLOCATOR (mem->allocator), 0);

  g1mem = (GstG1Memory *) mem;
  return g1mem->physaddress;
}

guint32
gst_g1_gem_set_physical (unsigned int physaddress)
{
  g1_gem_physical_addr = physaddress;
  return 0;
}

guint32
gst_g1_gem_get_physical (void)
{
  return g1_gem_physical_addr;
}
