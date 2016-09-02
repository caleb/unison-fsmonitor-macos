cmake_minimum_required(VERSION 3.6)
include(FindPkgConfig)
project(unison-fsmonitor-macos)

find_package(Boost)

#
# Find glib-2.0
#
pkg_check_modules(PC_GLIB2 REQUIRED glib-2.0)
set(GLIB2_DEFINITIONS ${PC_GLIB2_CFLAGS_OTHER})
find_path(GLIB2_INCLUDE_DIR glib.h HINTS ${PC_GLIB2_INCLUDEDIR} ${PC_GLIB2_INCLUDE_DIRS})
find_library(GLIB2_LIBRARY NAMES glib-2.0 libglib-2.0 HINTS ${PC_GLIB2_LIBDIR} ${PC_GLIB2_LIBRARY_DIRS})

file(GLOB SOURCES "src/*.cc" "src/*.hpp" "src/*.h")

add_executable(unison-fsmonitor ${SOURCES})
target_compile_features(unison-fsmonitor PRIVATE cxx_lambdas cxx_unicode_literals cxx_alias_templates)


