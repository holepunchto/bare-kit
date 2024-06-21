cmake_minimum_required(VERSION 3.25)

project(bare_kit LANGUAGES C VERSION 1.0)

include(bare)

install_node_modules(LOCKFILE)

if(APPLE)
  enable_language(OBJC)
endif()

if(NOT TARGET bare)
  add_subdirectory(vendor/bare EXCLUDE_FROM_ALL)
endif()

add_bare_bundle(
  ENTRY src/worklet.js
  OUT src/worklet.bundle.h
  NAME worklet_bundle
  TARGET c
)

add_library(bare_worklet OBJECT)

set_target_properties(
  bare_worklet
  PROPERTIES
  C_STANDARD 99
  POSITION_INDEPENDENT_CODE ON
)

target_sources(
  bare_worklet
  PRIVATE
    src/worklet.c
    src/worklet.bundle.h
    src/worklet.h
)

target_link_libraries(
  bare_worklet
  PUBLIC
    $<LINK_LIBRARY:WHOLE_ARCHIVE,bare_static>
)

link_bare_modules(bare_worklet)

if(APPLE)
  add_subdirectory(src/apple apple)
endif()
