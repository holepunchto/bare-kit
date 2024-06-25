cmake_minimum_required(VERSION 3.25)

project(bare_kit C)

include(bare)

set(bare_kit_root ../../../..)

add_subdirectory(${bare_kit_root} bare-kit EXCLUDE_FROM_ALL)

add_library(bare_kit SHARED)

set_target_properties(
  bare_kit
  PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)

target_sources(
  bare_kit
  PRIVATE
    Worklet.c
)

target_link_libraries(
  bare_kit
  PUBLIC
    bare_worklet
)
