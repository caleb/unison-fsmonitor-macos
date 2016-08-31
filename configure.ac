#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([unison-macos-fsmonitor], [0.0.1], [caleb@land.fm], [unison-macos-fsmonitor])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([dist-xz subdir-objects foreign])
AC_CONFIG_MACRO_DIRS([m4])

AX_CHECK_ENABLE_DEBUG([no], [DEBUG])

CXXFLAGS="$CXXFLAGS -Wall -D_XOPEN_SOURCE -D_BSD_SOURCE"
LDFLAGS="$LDFLAGS -lboost_system -lboost_filesystem -lfswatch -lboost_iostreams"

AX_CXX_COMPILE_STDCXX_14([noext],[mandatory])

AC_PROG_CXX
AC_PROG_CPP
AC_PROG_LIBTOOL
AM_PROG_CC_C_O

AX_BOOST_BASE
AX_BOOST_FILESYSTEM

AC_LANG_PUSH([C++])
AC_CHECK_HEADER([libfswatch/c++/monitor.hpp], [], AC_MSG_ERROR([unable to find json.hpp]))
AC_LANG_POP([C++])

PKG_CHECK_MODULES([GLIB], [glib-2.0])

LT_INIT()

AC_CONFIG_HEADER([config.h])
AC_CONFIG_SRCDIR([src/main.cc])
AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
