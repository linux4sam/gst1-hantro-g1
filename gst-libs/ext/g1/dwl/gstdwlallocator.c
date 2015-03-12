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

#include "gstdwlallocator.h"

GST_DEBUG_CATEGORY_EXTERN (GST_CAT_PERFORMANCE);
GST_DEBUG_CATEGORY_EXTERN (GST_CAT_MEMORY);

GST_DEBUG_CATEGORY_STATIC (gst_dwl_allocator_debug);
#define GST_CAT_DEFAULT gst_dwl_allocator_debug

static GstAllocator *_dwl_allocator = NULL;

#define DWL_FAILED(ret) (DWL_OK != (ret))

static GstMemory *gst_dwl_allocator_alloc (GstAllocator * allocator, gsize size,
    GstAllocationParams * params);
static void gst_dwl_allocator_free (GstAllocator * allocator,
    GstMemory * memory);

#define GST_DWL_ALLOCATOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DWL_ALLOCATOR,GstDwlAllocator))
#define GST_DWL_ALLOCATOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DWL_ALLOCATOR,GstDwlAllocatorClass))

typedef struct _GstDwlMemory
{
  GstG1Memory mem;

  DWLLinearMem_t linearmem;
} GstDwlMemory;

typedef struct
{
  GstG1Allocator parent;

  gpointer dwl;
} GstDwlAllocator;

typedef struct
{
  GstG1AllocatorClass parent_class;
} GstDwlAllocatorClass;

G_DEFINE_TYPE (GstDwlAllocator, gst_dwl_allocator, GST_TYPE_G1_ALLOCATOR);

void
gst_dwl_allocator_new (void)
{
  if (_dwl_allocator) {
    GST_DEBUG ("allocator already registered");
    goto exit;
  }

  _dwl_allocator = g_object_new (gst_dwl_allocator_get_type (), NULL);
  if (!_dwl_allocator) {
    GST_ERROR ("unable to create dwl allocator");
    goto exit;
  }

  gst_allocator_register (GST_ALLOCATOR_DWL, _dwl_allocator);
  GST_DEBUG (GST_ALLOCATOR_DWL " successfully registered");

exit:
  {
    return;
  }
}

static void
gst_dwl_allocator_class_init (GstDwlAllocatorClass * klass)
{
  GstAllocatorClass *allocator_class;

  allocator_class = (GstAllocatorClass *) klass;

  allocator_class->alloc = GST_DEBUG_FUNCPTR (gst_dwl_allocator_alloc);
  allocator_class->free = GST_DEBUG_FUNCPTR (gst_dwl_allocator_free);

  GST_DEBUG_CATEGORY_INIT (gst_dwl_allocator_debug, "g1allocator",
      0, "G1 Memory Allocator");
}

static void
gst_dwl_allocator_init (GstDwlAllocator * allocator)
{
  DWLInitParam_t params;

  GST_CAT_DEBUG (GST_CAT_MEMORY, "init allocator %p", allocator);

  /* Use H264 as client, not really needed for anything but as a container */
  params.clientType = DWL_CLIENT_TYPE_H264_DEC;
  allocator->dwl = DWLInit (&params);
}

static GstMemory *
gst_dwl_allocator_alloc (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
  GstDwlAllocator *dwl;
  GstDwlMemory *dwlmem;
  GstG1Memory *g1mem;
  GstMemory *mem;
  gsize maxsize;
  gint ret;

  dwl = GST_DWL_ALLOCATOR (allocator);
  dwlmem = g_slice_new (GstDwlMemory);
  mem = GST_MEMORY_CAST (dwlmem);

  /* Take into account prefix and padding */
  maxsize = size + params->prefix + params->padding;

  GST_LOG ("Allocating new slice %p of %d", mem, maxsize);

  ret = DWLMallocLinear (dwl->dwl, maxsize, &dwlmem->linearmem);
  if (DWL_FAILED (ret)) {
    GST_ERROR_OBJECT (dwl, "Unable to allocate buffer of size %d, reason: %d",
        size, ret);
    g_free (dwlmem);
    dwlmem = NULL;
    mem = NULL;
    goto exit;
  }

  /* Initialize GstMemory */
  gst_memory_init (mem, GST_MEMORY_FLAG_PHYSICALLY_CONTIGUOUS, _dwl_allocator,
      NULL, maxsize, 0, params->prefix, size);

  g1mem = (GstG1Memory *) dwlmem;
  g1mem->virtaddress = dwlmem->linearmem.virtualAddress;
  g1mem->physaddress = dwlmem->linearmem.busAddress;

exit:
  {
    return mem;
  }
}

static void
gst_dwl_allocator_free (GstAllocator * allocator, GstMemory * mem)
{
  GstDwlAllocator *dwl;
  GstDwlMemory *dwlmem;

  dwl = GST_DWL_ALLOCATOR (allocator);
  dwlmem = (GstDwlMemory *) mem;

  g_return_if_fail (GST_IS_DWL_ALLOCATOR (mem->allocator));

  GST_LOG ("Freeing slice %p", mem);

  DWLFreeLinear (dwl->dwl, &dwlmem->linearmem);
  g_slice_free (GstDwlMemory, dwlmem);
}
