plugin_LTLIBRARIES = libgstg1kmssink.la

libgstg1kmssink_la_SOURCES = 			\
	gstg1kmssink.c				\
	gstkmsutils.c				\
	gstkmsallocator.c			\
	gstkmsbufferpool.c			\
	$(NULL)

libgstg1kmssink_la_CFLAGS = 			\
	$(GST_PLUGINS_BASE_CFLAGS) 		\
	$(GST_BASE_CFLAGS) 			\
	$(GST_VIDEO_CFLAGS)			\
	$(GST_ALLOCATORS_CFLAGS)		\
	$(GST_CFLAGS) 				\
	$(KMS_DRM_CFLAGS) 			\
	$(NULL)

libgstg1kmssink_la_LIBADD = 			\
	$(GST_PLUGINS_BASE_LIBS) 		\
	$(GST_BASE_LIBS) 			\
	$(GST_VIDEO_LIBS)			\
	$(GST_ALLOCATORS_LIBS)			\
	$(GST_LIBS) 				\
	$(KMS_DRM_LIBS)				\
	$(NULL)

libgstg1kmssink_la_LDFLAGS = 			\
	$(GST_PLUGIN_LDFLAGS)			\
	$(NULL)

noinst_HEADERS = 				\
	gstg1kmssink.h 				\
	gstkmsutils.h				\
	gstkmsallocator.h			\
	gstkmsbufferpool.h			\
	$(NULL)
