add_bare_bundle(
  bare_kit_bundle
  ENTRY src/worklet.js
  OUT src/worklet.bundle.h
  BUILTINS src/builtins.json
  WORKING_DIRECTORY ..
)

add_library(bare_kit OBJECT)

set_target_properties(
  bare_kit
  PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)

target_sources(
  bare_kit
  INTERFACE
    ../include/bare-kit.h
  PRIVATE
    ipc.h
    ipc.c
    platform.h
    queue.h
    queue.c
    resources.h
    suspension.h
    worklet.c
    worklet.bundle.h
    worklet.h
)

target_include_directories(
  bare_kit
  PUBLIC
    ../include
    $<TARGET_PROPERTY:bare,INTERFACE_INCLUDE_DIRECTORIES>
)

if(APPLE)
  target_sources(
    bare_kit
    PRIVATE
      apple/ipc.m
      apple/resources.c
  )

  target_link_libraries(
    bare_kit
    PUBLIC
      "-framework CoreFoundation"
  )
else()
  target_sources(
    bare_kit
    PRIVATE
      standalone/ipc.c
  )
endif()

if(ANDROID)
  target_sources(
    bare_kit
    PRIVATE
      android/jvm.h
      android/platform.c
      android/resources.c
  )

  target_link_libraries(
    bare_kit
    PUBLIC
      android
  )
else()
  target_sources(
    bare_kit
    PRIVATE
      platform.c
  )
endif()

if(NOT APPLE AND NOT ANDROID)
  target_sources(
    bare_kit
    PRIVATE
      resources.c
  )
endif()

if(IOS)
  target_sources(
    bare_kit
    PRIVATE
      ios/suspension.m
  )

  target_link_libraries(
    bare_kit
    PUBLIC
      "-framework UIKit"
  )

  target_compile_definitions(
    bare_kit
    PRIVATE
      BARE_KIT_IOS
  )
else()
  target_sources(
    bare_kit
    PRIVATE
      suspension.c
  )
endif()

target_link_libraries(
  bare_kit
  INTERFACE
    $<LINK_LIBRARY:WHOLE_ARCHIVE,bare_static>
  PUBLIC
    rlimit_static
)

link_bare_modules(bare_kit WORKING_DIRECTORY ..)

add_library(bare_kit_shared SHARED)

set_target_properties(
  bare_kit_shared
  PROPERTIES
  OUTPUT_NAME bare-kit
)

if(WIN32)
  target_sources(
    bare_kit_shared
    PUBLIC
      win32/bare-kit.def
  )
endif()

target_include_directories(
  bare_kit_shared
  INTERFACE
    $<TARGET_PROPERTY:bare_kit,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(
  bare_kit_shared
  PRIVATE
    bare_kit
)

if(ANDROID)
  target_link_options(
    bare_kit_shared
    PRIVATE
      "-Wl,-z,global"
  )
endif()

if(LINUX)
  target_link_options(
    bare_kit_shared
    PUBLIC
      "-Wl,-z,now" # Use eager binding
  )
endif()

add_library(bare_kit_static STATIC)

set_target_properties(
  bare_kit_static
  PROPERTIES
  OUTPUT_NAME bare-kit
  PREFIX lib
)

if(WIN32)
  target_sources(
    bare_kit_static
    PUBLIC
      win32/bare-kit.def
  )
endif()

target_include_directories(
  bare_kit_static
  INTERFACE
    $<TARGET_PROPERTY:bare_kit,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(
  bare_kit_static
  PRIVATE
    bare_kit
)

install(TARGETS bare_kit_shared bare_kit_static)

install(FILES ../include/bare-kit.h DESTINATION include)
