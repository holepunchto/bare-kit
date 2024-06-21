cmake_minimum_required(VERSION 3.25)

project(bare_kit C)

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

add_library(bare_kit OBJECT)

set_target_properties(
  bare_kit
  PROPERTIES
  C_STANDARD 99
  POSITION_INDEPENDENT_CODE ON
)

target_sources(
  bare_kit
  PRIVATE
    src/worklet.c
    src/worklet.bundle.h
    src/worklet.h
)

target_include_directories(
  bare_kit
  PUBLIC
    $<TARGET_PROPERTY:bare,INTERFACE_INCLUDE_DIRECTORIES>
)

link_bare_modules(bare_kit)

if(APPLE)
  add_subdirectory(src/apple apple)
endif()
