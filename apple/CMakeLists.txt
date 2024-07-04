add_library(bare_kit STATIC)

set_target_properties(
  bare_kit
  PROPERTIES
  OBJC_STANDARD 11
  FRAMEWORK TRUE
  OUTPUT_NAME BareKit
  PUBLIC_HEADER BareKit/BareKit.h

  MACOSX_FRAMEWORK_IDENTIFIER "to.holepunch.bare.kit"
  MACOSX_FRAMEWORK_BUNDLE_VERSION ${PROJECT_VERSION}
  MACOSX_FRAMEWORK_SHORT_VERSION_STRING ${PROJECT_VERSION}
)

target_sources(
  bare_kit
  PUBLIC
    BareKit/BareKit.h
  PRIVATE
    BareKit/BareKit.m
)

target_include_directories(
  bare_kit
  PUBLIC
    .
)

target_link_libraries(
  bare_kit
  INTERFACE
    "-framework Foundation"
  PUBLIC
    bare_worklet
  PRIVATE
    rpc_static
)
