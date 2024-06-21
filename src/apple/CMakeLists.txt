add_library(bare_kit STATIC)

set_target_properties(
  bare_kit
  PROPERTIES
  OBJC_STANDARD 99
  FRAMEWORK TRUE
  OUTPUT_NAME BareKit
  PUBLIC_HEADER BareKit/BareKit.h

  MACOSX_FRAMEWORK_IDENTIFIER "to.holepunch.bare"
  MACOSX_FRAMEWORK_BUNDLE_VERSION ${PROJECT_VERSION}
  MACOSX_FRAMEWORK_SHORT_VERSION_STRING ${PROJECT_VERSION}
)

target_sources(
  bare_kit
  INTERFACE
    BareKit/BareKit.h
  PRIVATE
    BareKit/BareKit.m
)

target_link_libraries(
  bare_kit
  INTERFACE
    $<LINK_LIBRARY:WHOLE_ARCHIVE,bare_static>
  PUBLIC
    "-framework Foundation"
  PRIVATE
    bare_worklet
)
