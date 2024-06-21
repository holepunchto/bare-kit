set_target_properties(
  bare_kit
  PROPERTIES
  OBJC_STANDARD 99
)

target_sources(
  bare_kit
  INTERFACE
    BareKit/BareKit.h
  PRIVATE
    BareKit/BareKit.m
)

target_include_directories(
  bare_kit
  INTERFACE
    .
)

target_link_libraries(
  bare_kit
  INTERFACE
    $<LINK_LIBRARY:WHOLE_ARCHIVE,bare_static>
  PUBLIC
    "-framework Foundation"
)

add_library(bare_kit_framework SHARED)

set_target_properties(
  bare_kit_framework
  PROPERTIES
  FRAMEWORK TRUE
  OUTPUT_NAME BareKit
  PUBLIC_HEADER BareKit/BareKit.h
)

target_sources(
  bare_kit_framework
  PUBLIC
    BareKit/BareKit.h
)

target_link_libraries(
  bare_kit_framework
  PUBLIC
    bare_kit
)

link_bare_modules(bare_kit_framework WORKING_DIRECTORY ../..)
