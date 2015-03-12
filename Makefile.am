DISTCHECK_CONFIGURE_FLAGS=--enable-gtk-doc

ALWAYS_SUBDIRS =		\
	gst-libs		\
	ext			\
	gst			\
	sys			\
	common

SUBDIRS = $(ALWAYS_SUBDIRS)

DIST_SUBDIRS = $(ALWAYS_SUBDIRS)

EXTRA_DIST = \
	depcomp \
	AUTHORS COPYING NEWS README \
	ChangeLog autogen.sh 

DISTCLEANFILES = 

noinst_HEADERS = 

ACLOCAL_AMFLAGS = -I m4 -I common/m4

include $(top_srcdir)/common/release.mak
include $(top_srcdir)/common/po.mak

check-valgrind:
	cd tests/check && make check-valgrind

if HAVE_GST_CHECK
check-torture:
	cd tests/check && make torture
else
check-torture:
	true
endif

include $(top_srcdir)/common/coverage/lcov.mak

# cruft: plugins that have been merged or moved or renamed

CRUFT_FILES = \
	$(top_builddir)/common/shave \
	$(top_builddir)/common/shave-libtool 

CRUFT_DIRS = 

include $(top_srcdir)/common/cruft.mak

all-local: check-cruft
