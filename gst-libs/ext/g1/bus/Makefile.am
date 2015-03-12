lib_LTLIBRARIES = libgstbusallocator-@GST_API_VERSION@.la

libgstbusallocator_@GST_API_VERSION@_la_SOURCES = \
	gstbusallocator.c

libgstbusallocator_@GST_API_VERSION@includedir = $(includedir)/gstreamer-@GST_API_VERSION@/gst/g1/
libgstbusallocator_@GST_API_VERSION@include_HEADERS = \
	gstbusallocator.h

libgstbusallocator_@GST_API_VERSION@_la_CFLAGS = $(G1_CFLAGS) $(GST_CFLAGS) 	\
			-I$(top_builddir)/gst-libs/ext/g1/memalloc/
libgstbusallocator_@GST_API_VERSION@_la_LIBADD = $(G1_LIBS) $(GST_LIBS) 				\
			$(top_builddir)/gst-libs/ext/g1/memalloc/libgstg1allocator-@GST_API_VERSION@.la 