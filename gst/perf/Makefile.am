plugin_LTLIBRARIES = libgstperf.la

# sources used to compile this plug-in
libgstperf_la_SOURCES = gstperf.c

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstperf_la_CFLAGS = $(GST_CFLAGS)
libgstperf_la_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS) 
libgstperf_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstperf_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstperf.h