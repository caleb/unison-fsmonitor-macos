cmake_minimum_required(VERSION 3.6)
include(FindPkgConfig)
project(unison-fsmonitor-macos)

find_package(Boost)
pkg_check_modules(GLIB2 REQUIRED glib-2.0)

file(GLOB SOURCES "src/*.cc" "src/*.hpp" "src/*.h")

add_executable(unison-fsmonitor ${SOURCES})
target_compile_features(unison-fsmonitor PRIVATE cxx_lambdas cxx_unicode_literals cxx_alias_templates)


