AC_PREREQ([2.69])

dnl please read gstreamer/docs/random/autotools before changing this file

dnl initialize autoconf
dnl releases only do -Wall, git and prerelease does -Werror too
dnl use a three digit version number for releases, and four for git/pre
AC_INIT([GStreamer Hantro G1 Decoder Plug-in],[0.0.0.1],[support@ridgerun.com],[gst-hantro-g1],[www.ridgerun.com])

AG_GST_INIT

dnl initialize automake
AM_INIT_AUTOMAKE([-Wno-portability 1.11 no-dist-gzip dist-xz tar-ustar foreign])

dnl define PACKAGE_VERSION_* variables
AS_VERSION

dnl check if this is a release version
AS_NANO(GST_GIT="no", GST_GIT="yes")

dnl can autoconf find the source ?
AC_CONFIG_SRCDIR([ext/g1/gstg1.c])

dnl define the output header for config
AM_CONFIG_HEADER([config.h])

dnl AM_MAINTAINER_MODE only provides the option to configure to enable it
AM_MAINTAINER_MODE([enable])

dnl sets host_* variables
AC_CANONICAL_HOST

dnl use pretty build output with automake >= 1.11
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])],
  [AM_DEFAULT_VERBOSITY=1
   AC_SUBST(AM_DEFAULT_VERBOSITY)])

dnl our libraries and install dirs use GST_API_VERSION in the filename
dnl to allow side-by-side installation of different API versions
GST_API_VERSION=1.0
AC_SUBST(GST_API_VERSION)
AC_DEFINE_UNQUOTED(GST_API_VERSION, "$GST_API_VERSION",
  [GStreamer API Version])

AG_GST_LIBTOOL_PREPARE
AS_LIBTOOL(GST, 5, 0, 5)

dnl *** required versions of GStreamer stuff ***
GST_REQ=1.0.0
GSTPB_REQ=1.0.0

dnl *** autotools stuff ****

dnl allow for different autotools
AS_AUTOTOOLS_ALTERNATE

dnl Add parameters for aclocal
AC_SUBST(ACLOCAL_AMFLAGS, "-I m4 -I common/m4")

dnl *** check for arguments to configure ***

AG_GST_ARG_DISABLE_FATAL_WARNINGS

AG_GST_ARG_DEBUG
AG_GST_ARG_PROFILING
AG_GST_ARG_VALGRIND
AG_GST_ARG_GCOV

AG_GST_ARG_EXAMPLES

AG_GST_ARG_WITH_PKG_CONFIG_PATH
AG_GST_ARG_WITH_PACKAGE_NAME
AG_GST_ARG_WITH_PACKAGE_ORIGIN

AG_GST_ARG_WITH_PLUGINS

AG_GST_ARG_ENABLE_EXTERNAL

AG_GST_ARG_ENABLE_EXPERIMENTAL

dnl *** checks for platform ***

dnl * hardware/architecture *

dnl common/m4/gst-arch.m4
dnl check CPU type
AG_GST_ARCH

dnl Determine endianness
AC_C_BIGENDIAN

dnl *** checks for programs ***

dnl find a compiler
AC_PROG_CC
AC_PROG_CC_STDC

dnl determine c++ compiler
AC_PROG_CXX
dnl determine if c++ is available on this system
AC_CHECK_PROG(HAVE_CXX, $CXX, yes, no)

dnl determine c++ preprocessor
dnl FIXME: do we need this ?
AC_PROG_CXXCPP

AC_PROG_OBJC

dnl check if the compiler supports '-c' and '-o' options
AM_PROG_CC_C_O

dnl find an assembler
AM_PROG_AS

dnl check if the compiler supports do while(0) macros
AG_GST_CHECK_DOWHILE_MACROS

AC_PATH_PROG(VALGRIND_PATH, valgrind, no)
AM_CONDITIONAL(HAVE_VALGRIND, test ! "x$VALGRIND_PATH" = "xno")

dnl check for documentation tools
GTK_DOC_CHECK([1.12])
AS_PATH_PYTHON([2.1])
AG_GST_PLUGIN_DOCS([1.3],[2.1])

dnl check for G1 decoder package
AC_G1_CHECK

dnl *** checks for libraries ***

dnl *** checks for header files ***

dnl check if we have ANSI C header files
AC_HEADER_STDC

dnl *** checks for types/defines ***

dnl *** checks for structures ***

dnl *** checks for compiler characteristics ***

dnl *** checks for library functions ***

dnl *** checks for dependency libraries ***

dnl GLib
GLIB_REQ=2.32.0
AG_GST_GLIB_CHECK([$GLIB_REQ])

dnl checks for gstreamer
dnl uninstalled is selected preferentially -- see pkg-config(1)
AG_GST_CHECK_GST($GST_API_VERSION, [$GST_REQ], yes)
AG_GST_CHECK_GST_BASE($GST_API_VERSION, [$GST_REQ], yes)
AG_GST_CHECK_GST_CONTROLLER($GST_API_VERSION, [$GST_REQ], yes) 
AG_GST_CHECK_GST_CHECK($GST_API_VERSION, [$GST_REQ], no)

GST_TOOLS_DIR=`$PKG_CONFIG --variable=toolsdir gstreamer-$GST_API_VERSION`
if test -z $GST_TOOLS_DIR; then
  AC_MSG_ERROR([no tools dir defined in GStreamer pkg-config file; core upgrade needed.])
fi
AC_SUBST(GST_TOOLS_DIR)

AC_MSG_NOTICE(Using GStreamer Core Plugins in $GST_PLUGINS_DIR)
AC_MSG_NOTICE(Using GStreamer Base Plugins in $GSTPB_PLUGINS_DIR)

AM_CONDITIONAL(HAVE_GST_CHECK, test "x$HAVE_GST_CHECK" = "xyes")

dnl Check for documentation xrefs
GLIB_PREFIX="`$PKG_CONFIG --variable=prefix glib-2.0`"
GST_PREFIX="`$PKG_CONFIG --variable=prefix gstreamer-$GST_API_VERSION`"
GSTPB_PREFIX="`$PKG_CONFIG --variable=prefix gstreamer-plugins-base-$GST_API_VERSION`"
AC_SUBST(GLIB_PREFIX)
AC_SUBST(GST_PREFIX)
AC_SUBST(GSTPB_PREFIX)

dnl GTK is optional and used in examples
HAVE_GTK=no
GTK_REQ=3.0.0
if test "x$BUILD_EXAMPLES" = "xyes"; then
  PKG_CHECK_MODULES(GTK, gtk+-3.0 >= $GTK_REQ, HAVE_GTK=yes, HAVE_GTK=no)
  dnl some examples need gtk+-x11
  PKG_CHECK_MODULES(GTK_X11, gtk+-x11-3.0 >= $GTK_REQ, HAVE_GTK_X11=yes, HAVE_GTK_X11=no)
  AC_SUBST(GTK_LIBS)
  AC_SUBST(GTK_CFLAGS)
fi
AM_CONDITIONAL(HAVE_GTK, test "x$HAVE_GTK" = "xyes")
AM_CONDITIONAL(HAVE_GTK_X11, test "x$HAVE_GTK_X11" = "xyes")

dnl *** set variables based on configure arguments ***

dnl set license and copyright notice
GST_LICENSE="LGPL"
AC_DEFINE_UNQUOTED(GST_LICENSE, "$GST_LICENSE", [GStreamer license])
AC_SUBST(GST_LICENSE)

dnl set location of plugin directory
AG_GST_SET_PLUGINDIR

dnl set release date/time
AG_GST_SET_PACKAGE_RELEASE_DATETIME_WITH_NANO([$PACKAGE_VERSION_NANO],
  ["${srcdir}/gst-hantro-g1.doap"],
  [$PACKAGE_VERSION_MAJOR.$PACKAGE_VERSION_MINOR.$PACKAGE_VERSION_MICRO])

# set by AG_GST_PARSE_SUBSYSTEM_DISABLES above
dnl make sure it doesn't complain about unused variables if debugging is disabled
NO_WARNINGS=""
AG_GST_CHECK_GST_DEBUG_DISABLED([NO_WARNINGS="-Wno-unused"], [NO_WARNINGS=""])

dnl define an ERROR_CFLAGS Makefile variable
dnl -Wundef: too many broken headers
AG_GST_SET_ERROR_CFLAGS($FATAL_WARNINGS, [
      -Wmissing-declarations -Wmissing-prototypes -Wredundant-decls
      -Wwrite-strings -Wold-style-definition -Waggregate-return
      -Winit-self -Wmissing-include-dirs -Waddress -Wno-multichar
      -Wnested-externs $NO_WARNINGS])

dnl define an ERROR_CXXFLAGS Makefile variable
AG_GST_SET_ERROR_CXXFLAGS($FATAL_WARNINGS, [
        -Wmissing-declarations -Wredundant-decls
        -Wwrite-strings
        -Winit-self -Wmissing-include-dirs -Waddress -Wno-multichar
        $NO_WARNINGS])

dnl define an ERROR_OBJCFLAGS Makefile variable
AG_GST_SET_ERROR_OBJCFLAGS($FATAL_WARNINGS, [
      -Wmissing-declarations -Wmissing-prototypes -Wredundant-decls
      -Wwrite-strings -Wold-style-definition
      -Winit-self -Wmissing-include-dirs -Wno-multichar
      -Wnested-externs $NO_WARNINGS])

dnl define correct level for debugging messages
AG_GST_SET_LEVEL_DEFAULT($GST_GIT)

dnl used in examples
AG_GST_DEFAULT_ELEMENTS

dnl *** plug-ins to include ***

dnl these are all the gst plug-ins, compilable without additional libs
dnl AG_GST_CHECK_PLUGIN(pluginname)

dnl *** sys plug-ins ***

dnl *** ext plug-ins ***
dnl keep this list sorted alphabetically !

if test "x$BUILD_EXTERNAL" = "xyes"; then

echo
AC_MSG_NOTICE([Checking libraries for plugins in ext/])
echo

dnl *** g1 decoder ***
translit(dnm, m, l) AM_CONDITIONAL(USE_G1, true)
AG_GST_CHECK_FEATURE(G1, [Hantro G1 Decoder], [], [
HAVE_G1="yes"
])

else

dnl not building plugins with external dependencies,
dnl but we still need to set the conditionals
AM_CONDITIONAL(USE_G1, false)

fi dnl of EXT plugins

dnl *** finalize CFLAGS, LDFLAGS, LIBS

dnl Overview:
dnl GST_OPTION_CFLAGS:  common flags for profiling, debugging, errors, ...
dnl GST_*:              flags shared by all built objects
dnl GST_ALL_LDFLAGS:    linker flags shared by all
dnl GST_LIB_LDFLAGS:    not needed, we don't install libraries
dnl GST_LT_LDFLAGS:     library versioning of our libraries
dnl GST_PLUGIN_LDFLAGS: flags to be used for all plugins

dnl GST_OPTION_CFLAGS
if test "x$USE_DEBUG" = xyes; then
   PROFILE_CFLAGS="-g"
fi
AC_SUBST(PROFILE_CFLAGS)

if test "x$PACKAGE_VERSION_NANO" = "x1"; then
  dnl Define _only_ for git (not pre-releases or releases)
  DEPRECATED_CFLAGS="-DGST_DISABLE_DEPRECATED"
else
  DEPRECATED_CFLAGS=""
fi
AC_SUBST(DEPRECATED_CFLAGS)

dnl every flag in GST_OPTION_CFLAGS, GST_OPTION_CXXFLAGS and GST_OPTION_OBJCFLAGS can be overridden
dnl at make time with e.g. make ERROR_CFLAGS=""
GST_OPTION_CFLAGS="\$(WARNING_CFLAGS) \$(ERROR_CFLAGS) \$(DEBUG_CFLAGS) \$(PROFILE_CFLAGS) \$(GCOV_CFLAGS) \$(OPT_CFLAGS) \$(DEPRECATED_CFLAGS)"
GST_OPTION_CXXFLAGS="\$(WARNING_CXXFLAGS) \$(ERROR_CXXFLAGS) \$(DEBUG_CFLAGS) \$(PROFILE_CFLAGS) \$(GCOV_CFLAGS) \$(OPT_CFLAGS) \$(DEPRECATED_CFLAGS)"
GST_OPTION_OBJCFLAGS="\$(WARNING_OBJCFLAGS) \$(ERROR_OBJCFLAGS) \$(DEBUG_CFLAGS) \$(PROFILE_CFLAGS) \$(GCOV_CFLAGS) \$(OPT_CFLAGS) \$(DEPRECATED_CFLAGS)"
AC_SUBST(GST_OPTION_CFLAGS)
AC_SUBST(GST_OPTION_CXXFLAGS)
AC_SUBST(GST_OPTION_OBJCFLAGS)

dnl our libraries need to be versioned correctly
AC_SUBST(GST_LT_LDFLAGS)

dnl FIXME: do we want to rename to GST_ALL_* ?
dnl prefer internal headers to already installed ones
dnl also add builddir include for enumtypes and marshal
dnl add ERROR_CFLAGS, but overridable
GST_CFLAGS="$GST_CFLAGS -DGST_USE_UNSTABLE_API"
GST_CXXFLAGS="-I\$(top_srcdir)/gst-libs $GST_CFLAGS $GLIB_EXTRA_CFLAGS \$(GST_OPTION_CXXFLAGS)"
GST_OBJCFLAGS="-I\$(top_srcdir)/gst-libs $GST_CFLAGS $GLIB_EXTRA_CFLAGS \$(GST_OPTION_OBJCFLAGS)"
GST_CFLAGS="-I\$(top_srcdir)/gst-libs $GST_CFLAGS $GLIB_EXTRA_CFLAGS \$(GST_OPTION_CFLAGS)"
AC_SUBST(GST_CFLAGS)
AC_SUBST(GST_CXXFLAGS)
AC_SUBST(GST_OBJCFLAGS)
AC_SUBST(GST_LIBS)

dnl LDFLAGS really should only contain flags, not libs - they get added before
dnl whatevertarget_LIBS and -L flags here affect the rest of the linking
GST_ALL_LDFLAGS="-no-undefined"
AC_SUBST(GST_ALL_LDFLAGS)

dnl this really should only contain flags, not libs - they get added before
dnl whatevertarget_LIBS and -L flags here affect the rest of the linking
GST_PLUGIN_LDFLAGS="-module -avoid-version -export-symbols-regex '^[_]*gst_plugin_desc.*' $GST_ALL_LDFLAGS"
AC_SUBST(GST_PLUGIN_LDFLAGS)

dnl *** output files ***

dnl keep this alphabetic per directory, please
AC_CONFIG_FILES(
Makefile
ext/Makefile
ext/g1/Makefile
sys/Makefile
sys/fbdev/Makefile
sys/drmsink/Makefile
gst-libs/Makefile
gst-libs/ext/Makefile
gst-libs/ext/g1/Makefile
gst-libs/ext/g1/memalloc/Makefile
gst-libs/ext/g1/dwl/Makefile
gst-libs/ext/g1/bus/Makefile
gst-libs/ext/g1/utils/Makefile
gst/Makefile
gst/perf/Makefile
common/Makefile
common/m4/Makefile
)

AC_OUTPUT

AG_GST_OUTPUT_PLUGINS

