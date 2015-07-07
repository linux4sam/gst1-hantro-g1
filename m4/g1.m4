dnl 
dnl G1 decoder specific configuration checks
dnl 
dnl 

dnl AC_G1_CHECK()
dnl sets G1_CFLAGS and G1_LIBS
dnl
dnl AC_G1_CHECK_LIBRARY(name, library)
dnl helper function to ask for a library in the G1 fashion
dnl   name: The printable name of the library
dnl   library: default library name
dnl

# AC_G1_CHECK()
# sets G1_CFLAGS and G1_LIBS
# ----------------------------------
AC_DEFUN([AC_G1_CHECK],[
  AC_ARG_VAR([G1_CFLAGS], [compiler flags for the G1 decoder package])dnl
  AC_ARG_VAR([G1_LIBS], [linker flags for the G1 decoder package])dnl

  AC_G1_CHECK_DWL  
  AC_G1_CHECK_H264
  AC_G1_CHECK_MPEG4
  AC_G1_CHECK_JPEG
  AC_G1_CHECK_VP8
  AC_G1_CHECK_PP
]) # AC_G1_CHECK


# AC_G1_CHECK_LIBRARY(name, library)
# helper function to ask for a library in the G1 fashion
#   name: The printable name of the library
#   library: default library name
#   function: function to test
#   header: header to test
# ----------------------------------
AC_DEFUN([AC_G1_CHECK_LIBRARY],[

dnl Save the old cflags to restore them later
OLD_FLAGS=$CFLAGS
OLD_LIBS=$LIBS
OLD_CPPFLAGS=$CPPFLAGS

dnl Append G1 flags to build this package
CFLAGS="$CFLAGS $G1_CFLAGS"
LIBS="$LIBS $G1_LIBS"
CPPFLAGS="$CPPFLAGS $G1_CFLAGS"

dnl DWL general utils library
AC_ARG_WITH(g1-$1-path,[
AS_HELP_STRING([--with-g1-$1-path=PATH], [Path to an alternative library])],
  [G1_LIB=:$with_g1_$1_path],
  [G1_LIB=$2])

AC_CHECK_LIB([$G1_LIB], [$3], [G1_LIBS="-l$G1_LIB $G1_LIBS"],
AC_MSG_ERROR([The $1 library was not found or is unusable. If the library is in a non-standard location
specify it via G1_LIBS=-Lpath/to/$1/lib/ or --with-g1-$1-path=path/to/$1/lib/lib$2.a]),[$5])

AC_CHECK_HEADER([$4], [], 
AC_MSG_ERROR([Unable to find $4. If the header is in a non-standard location 
specify it via G1_CFLAGS=-Ipath/to/$1/include/]))

AC_SUBST(G1_CFLAGS)
AC_SUBST(G1_LIBS)

dnl Restore CFLAGS and LIBS
CFLAGS=$OLD_CFLAGS
LIBS=$OLD_LIBS
CPPFLAGS=$OLD_CPPFLAGS
])# AC_G1_CHECK_LIBRARY

AC_DEFUN([AC_G1_CHECK_DWL],[
  AC_G1_CHECK_LIBRARY([dwl], [dwlx170], [DWLMallocLinear], [dwl.h], [])
])# AC_G1_CHECK_DWL

AC_DEFUN([AC_G1_CHECK_H264],[
  AC_G1_CHECK_LIBRARY([h264], [decx170h], [H264DecInit], [h264decapi.h], [-pthread])
])# AC_G1_CHECK_H264

AC_DEFUN([AC_G1_CHECK_MPEG4],[
  AC_G1_CHECK_LIBRARY([mpeg4], [decx170m], [MP4DecInit], [mp4decapi.h], [-pthread])
])# AC_G1_CHECK_H264

AC_DEFUN([AC_G1_CHECK_JPEG],[
  AC_G1_CHECK_LIBRARY([jpeg], [x170j], [JpegDecInit], [jpegdecapi.h], [-pthread])
])# AC_G1_CHECK_H264

AC_DEFUN([AC_G1_CHECK_VP8],[
  # VP8 lib leaves some undefined symbols for the user to implement. 
  # Ignore them for the purposes of this check.
  AC_G1_CHECK_LIBRARY([vp8], [decx170vp8], [VP8DecInit], [vp8decapi.h], 
    [-Wl,--unresolved-symbols=ignore-all -pthread])
])# AC_G1_CHECK_H264

AC_DEFUN([AC_G1_CHECK_PP],[
  AC_G1_CHECK_LIBRARY([pp], [decx170p], [PPInit], [ppapi.h])
])# AC_G1_CHECK_PP
