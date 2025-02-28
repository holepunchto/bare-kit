add_library(bare_kit SHARED)

set_target_properties(
  bare_kit
  PROPERTIES
  OBJC_STANDARD 11
  FRAMEWORK TRUE
  OUTPUT_NAME BareKit
  PUBLIC_HEADER BareKit/BareKit.h

  MACOSX_FRAMEWORK_BUNDLE_NAME "BareKit"
  MACOSX_FRAMEWORK_IDENTIFIER "to.holepunch.bare.kit"
  MACOSX_FRAMEWORK_BUNDLE_VERSION ${PROJECT_VERSION}
  MACOSX_FRAMEWORK_SHORT_VERSION_STRING ${PROJECT_VERSION}
)

if(IOS)
  set_target_properties(
    bare_kit
    PROPERTIES
    MACOSX_FRAMEWORK_INFO_PLIST "${CMAKE_CURRENT_LIST_DIR}/BareKit/iOS.plist"
  )
else()
  set_target_properties(
    bare_kit
    PROPERTIES
    MACOSX_FRAMEWORK_INFO_PLIST "${CMAKE_CURRENT_LIST_DIR}/BareKit/macOS.plist"
  )
endif()

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
  PUBLIC
    "-framework Foundation"
    "-framework UserNotifications"
  PUBLIC
    bare_worklet
)

if(IOS)
  target_link_libraries(
    bare_kit
    PUBLIC
      "-framework UIKit"
  )
endif()
