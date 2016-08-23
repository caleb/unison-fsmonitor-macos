#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([unison-fswatchman], [0.0.1], [caleb@land.fm], [unison-fswatchman])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([dist-xz subdir-objects foreign])
AC_CONFIG_MACRO_DIRS([m4])

CXXFLAGS="$CXXFLAGS -Wall -std=c99 -stdc++=c++14 -D_XOPEN_SOURCE -D_BSD_SOURCE"

AC_PROG_CXX
AC_PROG_CPP
AC_PROG_LIBTOOL
AM_PROG_CC_C_O

AC_CHECK_HEADER([watchman.h], [], AC_MSG_ERROR([unable to find watchman]))

LT_INIT()

AC_SEARCH_LIBS([watchman_connect], [watchman], [], AC_MSG_ERROR([unable to find watchman_connect()]))

AC_CONFIG_HEADER([config.h])
AC_CONFIG_SRCDIR([src/main.cc])
AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
