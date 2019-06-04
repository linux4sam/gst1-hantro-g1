plugin_LTLIBRARIES = libgstg1kms.la
libgstg1kms_la_SOURCES = 			\
	gstg1kmssink.c				\
	gstkmsutils.c				\
	gstkmsallocator.c			\
	gstkmsbufferpool.c			\
	$(NUL)

libgstg1kms_la_CFLAGS = 			\
	$(GST_PLUGINS_BASE_CFLAGS) 		\
	$(GST_BASE_CFLAGS) 			\
	$(GST_VIDEO_CFLAGS)			\
	$(GST_ALLOCATORS_CFLAGS)		\
	$(GST_CFLAGS) 				\
	$(KMS_DRM_CFLAGS) 			\
       -I$(top_builddir)/gst-libs/ext/g1/memalloc \
       -I$(top_builddir)/gst-libs/ext/g1/bus \
	$(NULL)

libgstg1kms_la_LIBADD = 			\
	$(GST_PLUGINS_BASE_LIBS) 		\
	$(GST_BASE_LIBS) 			\
	$(GST_VIDEO_LIBS)			\
	$(GST_ALLOCATORS_LIBS)			\
	$(GST_LIBS) 				\
	$(KMS_DRM_LIBS)		\
	-lgstvideo-$(GST_API_VERSION) -lrt	\
	-lgstallocators-$(GST_API_VERSION)	\
	$(top_builddir)/gst-libs/ext/g1/bus/libgstbusallocator-@GST_API_VERSION@.la \
	$(NULL)

libgstg1kms_la_LDFLAGS = 			\
	$(GST_PLUGIN_LDFLAGS)			\
	$(NULL)

noinst_HEADERS = 				\
	gstg1kmssink.h 				\
	gstkmsutils.h				\
	gstkmsallocator.h			\
	gstkmsbufferpool.h			\
	$(NULL)
