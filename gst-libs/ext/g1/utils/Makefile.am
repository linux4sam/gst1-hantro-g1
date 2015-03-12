lib_LTLIBRARIES = libgstg1utils-@GST_API_VERSION@.la

libgstg1utils_@GST_API_VERSION@_la_SOURCES = 	\
	gstg1result.c 				\
	gstg1format.c				\
	gstg1enum.c

libgstg1utils_@GST_API_VERSION@includedir = $(includedir)/gstreamer-@GST_API_VERSION@/gst/g1
libgstg1utils_@GST_API_VERSION@include_HEADERS = 	\
	gstg1result.h					\
	gstg1format.h					\
	gstg1enum.h

libgstg1utils_@GST_API_VERSION@_la_CFLAGS = $(G1_CFLAGS) $(GST_CFLAGS) 
libgstg1utils_@GST_API_VERSION@_la_LIBADD = $(G1_LIBS) $(GST_LIBS) 