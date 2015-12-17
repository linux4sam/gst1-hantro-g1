plugin_LTLIBRARIES = libgstdrmsink.la
#LIBDRM_CFLAGS = ../../../libdrm/2.4.59-r0/libdrm-2.4.59/include/drm ../../../libdrm/2.4.59-r0/libdrm-2.4.59/include ../../../libdrm/2.4.59-r0/libdrm-2.4.59/ 
LIBDRM_CFLAGS = $(GST_LIBDRM)/usr/include/libdrm $(GST_LIBDRM)/usr/include/drm
libgstdrmsink_la_SOURCES = gstdrmsink.c gstframebuffersink.c
libgstdrmsink_la_CFLAGS = \
        $(GST_PLUGINS_BASE_CFLAGS) \
        $(GST_CFLAGS) \
        $(LIBFBDEV_CFLAGS) \
       -I$(top_builddir)/gst-libs/ext/g1/memalloc/      \
       -I$(top_builddir)/gst-libs/ext/g1/bus/            \
       -I$(LIBDRM_CFLAGS) \
       -I$(GST_LIBDRM)/usr/include/libkms    
                 
libgstdrmsink_la_LIBADD = \
        $(GST_PLUGINS_BASE_LIBS) -lgstvideo-$(GST_API_VERSION) -lrt \
        $(GST_BASE_LIBS) \
        $(LIBFBDEV_LIBS) \
        $(top_builddir)/gst-libs/ext/g1/bus/libgstbusallocator-@GST_API_VERSION@.la \
        $(GST_LIBS) -ldrm -lkms 
              
libgstdrmsink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstdrmsink_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS) 
noinst_HEADERS = gstdrmsink.h gstframebuffersink.h 
