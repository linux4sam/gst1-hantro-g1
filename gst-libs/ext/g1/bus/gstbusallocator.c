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
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "gstbusallocator.h"

GST_DEBUG_CATEGORY_EXTERN (GST_CAT_PERFORMANCE);
GST_DEBUG_CATEGORY_EXTERN (GST_CAT_MEMORY);

GST_DEBUG_CATEGORY_STATIC (gst_bus_allocator_debug);
#define GST_CAT_DEFAULT gst_bus_allocator_debug

#define BUS_DEV_MEM "/dev/mem"

#define GST_BUS_ALLOCATOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BUS_ALLOCATOR,GstBusAllocator))
#define GST_BUS_ALLOCATOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BUS_ALLOCATOR,GstBusAllocatorClass))

typedef struct
{
  GstG1Allocator parent;

  guint32 physaddress;
  gpointer virtaddress;
  gsize size;

} GstBusAllocator;

typedef struct
{
  GstG1AllocatorClass parent_class;
} GstBusAllocatorClass;

static GstBusAllocator *_bus_allocator = NULL;

static GstMemory *gst_bus_allocator_alloc (GstAllocator * allocator, gsize size,
    GstAllocationParams * params);
static void gst_bus_allocator_free (GstAllocator * allocator,
    GstMemory * memory);
static gboolean gst_bus_allocator_get_virtual_address (GstBusAllocator *
    allocator, guint32 physaddress, gsize size);

G_DEFINE_TYPE (GstBusAllocator, gst_bus_allocator, GST_TYPE_G1_ALLOCATOR);

void
gst_bus_allocator_new (guint32 physaddress, gsize size)
{
  if (_bus_allocator) {
    GST_DEBUG ("allocator already registered");
    goto exit;
  }

  _bus_allocator = g_object_new (gst_bus_allocator_get_type (), NULL);
  if (!_bus_allocator) {
    GST_ERROR ("unable to create bus allocator");
    goto exit;
  }


  if (!gst_bus_allocator_get_virtual_address (_bus_allocator, physaddress,
          size)) {
    GST_ERROR_OBJECT (_bus_allocator, "unable to open bus allocator");
    gst_object_unref (_bus_allocator);
    _bus_allocator = NULL;
    goto exit;
  }

  gst_allocator_register (GST_ALLOCATOR_BUS, GST_ALLOCATOR (_bus_allocator));
  GST_DEBUG_OBJECT (_bus_allocator,
      GST_ALLOCATOR_BUS " successfully registered");

exit:
  {
    return;
  }
}

static void
gst_bus_allocator_class_init (GstBusAllocatorClass * klass)
{
  GstAllocatorClass *allocator_class;

  allocator_class = (GstAllocatorClass *) klass;

  allocator_class->alloc = GST_DEBUG_FUNCPTR (gst_bus_allocator_alloc);
  allocator_class->free = GST_DEBUG_FUNCPTR (gst_bus_allocator_free);

  GST_DEBUG_CATEGORY_INIT (gst_bus_allocator_debug, "g1allocator",
      0, "G1 Memory Allocator");
}

static void
gst_bus_allocator_init (GstBusAllocator * allocator)
{
  GST_CAT_DEBUG (GST_CAT_MEMORY, "init allocator %p", allocator);

  allocator->physaddress = 0;
  allocator->virtaddress = NULL;
  allocator->size = 0;
}

static gboolean
gst_bus_allocator_get_virtual_address (GstBusAllocator * allocator,
    guint32 physaddress, gsize size)
{
  gint fd;

  if (allocator->virtaddress) {
    munmap (allocator->virtaddress, allocator->size);
  }

  fd = open (BUS_DEV_MEM, O_RDWR | O_NONBLOCK);
  if (-1 == fd)
    goto error;

  allocator->virtaddress =
      mmap (0, size, PROT_WRITE, MAP_SHARED, fd, physaddress);
  if (MAP_FAILED == allocator->virtaddress)
    goto closefd;

  allocator->size = size;
  allocator->physaddress = physaddress;

  /* mmap keeps the file open */
  close (fd);

  return TRUE;

closefd:
  {
    close (fd);
  }
error:
  {
    GST_ERROR_OBJECT (allocator, "unable to open fb allocator: %s",
        strerror (errno));
    allocator->virtaddress = NULL;
    allocator->size = 0;
    allocator->physaddress = 0;
    return FALSE;
  }
}

static GstMemory *
gst_bus_allocator_alloc (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
  GstBusAllocator *bus;
  GstG1Memory *g1mem;
  GstMemory *mem;
  gsize maxsize;

/* TODO: lock here if used */

  bus = GST_BUS_ALLOCATOR (allocator);

  /* Take into account prefix and padding */
  maxsize = size + params->prefix + params->padding;
  if (maxsize > bus->size) {
    GST_ERROR_OBJECT (bus, "Requested size exceeds available size");
    mem = NULL;
    goto exit;
  }

  g1mem = g_slice_new (GstG1Memory);
  mem = GST_MEMORY_CAST (g1mem);
  GST_LOG ("Allocating new slice %p of %d", mem, maxsize);

  /* Initialize GstMemory */
  gst_memory_init (mem, GST_MEMORY_FLAG_PHYSICALLY_CONTIGUOUS,
      GST_ALLOCATOR (_bus_allocator), NULL, maxsize, 0, params->prefix, size);

  g1mem->virtaddress = bus->virtaddress;
  g1mem->physaddress = bus->physaddress;

exit:
  {
    return mem;
  }
}

static void
gst_bus_allocator_free (GstAllocator * allocator, GstMemory * mem)
{
  g_return_if_fail (GST_IS_BUS_ALLOCATOR (mem->allocator));

  GST_LOG ("Freeing slice %p", mem);

  g_slice_free (GstG1Memory, (gpointer) mem);
}
