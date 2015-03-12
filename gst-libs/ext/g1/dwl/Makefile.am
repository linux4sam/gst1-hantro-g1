lib_LTLIBRARIES = libgstdwlallocator-@GST_API_VERSION@.la

libgstdwlallocator_@GST_API_VERSION@_la_SOURCES = \
	gstdwlallocator.c

libgstdwlallocator_@GST_API_VERSION@includedir = $(includedir)/gstreamer-@GST_API_VERSION@/gst/g1/
libgstdwlallocator_@GST_API_VERSION@include_HEADERS = \
	gstdwlallocator.h

libgstdwlallocator_@GST_API_VERSION@_la_CFLAGS = $(G1_CFLAGS) $(GST_CFLAGS) 	\
			-I$(top_builddir)/gst-libs/ext/g1/memalloc/
libgstdwlallocator_@GST_API_VERSION@_la_LIBADD = $(G1_LIBS) $(GST_LIBS) 				\
			$(top_builddir)/gst-libs/ext/g1/memalloc/libgstg1allocator-@GST_API_VERSION@.la 