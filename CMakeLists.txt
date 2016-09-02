cmake_minimum_required(VERSION 3.6)
find_package(PkgConfig)
project(unison-fsmonitor-macos)

find_package(Boost 1.61 REQUIRED filesystem iostreams system)

#
# Find glib-2.0
#
pkg_check_modules(PC_GLIB2 REQUIRED glib-2.0)
# Load each of the sublibraries
foreach(_library ${PC_GLIB2_LIBRARIES})
  find_library(${_library}_LIBRARIES NAMES ${_library} "lib${_library}" HINTS ${PC_GLIB2_LIBRARY_DIRS})
  list(APPEND GLIB2_LIBRARIES ${${_library}_LIBRARIES})
endforeach()
set(GLIB2_INCLUDE_DIRS ${PC_GLIB2_INCLUDE_DIRS})

#
# Find the fswatch library
#
find_path(FSWATCH_INCLUDE_DIRS libfswatch/c++/monitor.hpp)
find_library(FSWATCH_LIBRARIES NAMES fswatch libfswatch)

file(GLOB SOURCES "src/*.cc" "src/*.hpp" "src/*.h")

add_executable(unison-fsmonitor ${SOURCES})
target_include_directories(unison-fsmonitor PUBLIC ${GLIB2_INCLUDE_DIRS}
                                                   ${FSWATCH_INCLUDE_DIRS}
                                                   ${Boost_INCLUDE_DIRS})
target_link_libraries(unison-fsmonitor ${GLIB2_LIBRARIES}
                                       ${FSWATCH_LIBRARIES}
                                       ${Boost_LIBRARIES})
target_compile_features(unison-fsmonitor PRIVATE cxx_lambdas cxx_unicode_literals cxx_alias_templates)

