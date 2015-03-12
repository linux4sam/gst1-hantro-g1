plugin_LTLIBRARIES = libgstg1fbdevsink.la

libgstg1fbdevsink_la_SOURCES = gstg1fbdevsink.c
libgstg1fbdevsink_la_CFLAGS = \
	$(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_CFLAGS) \
	$(LIBFBDEV_CFLAGS) \
       -I$(top_builddir)/gst-libs/ext/g1/memalloc/ 	\
       -I$(top_builddir)/gst-libs/ext/g1/bus/
libgstg1fbdevsink_la_LIBADD = \
	$(GST_PLUGINS_BASE_LIBS) -lgstvideo-$(GST_API_VERSION) -lrt \
	$(GST_BASE_LIBS) \
	$(LIBFBDEV_LIBS) \
	$(top_builddir)/gst-libs/ext/g1/bus/libgstbusallocator-@GST_API_VERSION@.la 
libgstg1fbdevsink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstg1fbdevsink_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = gstg1fbdevsink.h
