plugin_LTLIBRARIES = libgstg1.la

libgstg1_la_SOURCES = 			\
	gstg1.c 			\
	gstg1basedec.h gstg1basedec.c	\
	gstg1h264dec.h gstg1h264dec.c	\
	gstg1vp8dec.h gstg1vp8dec.c	\
	gstg1jpegdec.h gstg1jpegdec.c	\
	gstg1mp4dec.h gstg1mp4dec.c

FIFO_DATATYPE="i32"
DEFINES = -DFIFO_DATATYPE=$(FIFO_DATATYPE)

libgstg1_la_CFLAGS = 	$(G1_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS) 	\
			$(GST_BASE_CFLAGS) $(GST_CFLAGS) $(DEFINES) \
			-I$(top_builddir)/gst-libs/ext/g1/memalloc/ 	\
			-I$(top_builddir)/gst-libs/ext/g1/dwl/ 		\
			-I$(top_builddir)/gst-libs/ext/g1/utils/

libgstg1_la_LIBADD = 	$(G1_LIBS) $(GST_PLUGINS_BASE_LIBS) \
			-lgstvideo-$(GST_API_VERSION) \
			$(GST_BASE_LIBS) $(GST_LIBS) \
			$(top_builddir)/gst-libs/ext/g1/dwl/libgstdwlallocator-@GST_API_VERSION@.la \
			$(top_builddir)/gst-libs/ext/g1/utils/libgstg1utils-@GST_API_VERSION@.la \
			$(top_builddir)/gst-libs/ext/g1/memalloc/libgstg1allocator-@GST_API_VERSION@.la
		 

libgstg1_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstg1_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS =  	\
	gstg1basedec.h 	\
	gstg1mp4dec.h  \
	gstg1vp8dec.h  \
	gstg1jpegdec.h  \
	gstg1h264dec.h
