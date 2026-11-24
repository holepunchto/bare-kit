cmake_minimum_required(VERSION 4.0)

find_package(cmake-bare REQUIRED PATHS node_modules/cmake-bare)
find_package(cmake-bare-bundle REQUIRED PATHS node_modules/cmake-bare-bundle)
find_package(cmake-fetch REQUIRED PATHS node_modules/cmake-fetch)
find_package(cmake-napi REQUIRED PATHS node_modules/cmake-napi)
find_package(cmake-prelude REQUIRED PATHS node_modules/cmake-prelude)
find_package(cmake-test REQUIRED PATHS node_modules/cmake-test)

project(bare_kit LANGUAGES C CXX VERSION 3.3.0)

if(APPLE)
  enable_language(OBJC)
endif()

fetch_package("github:holepunchto/bare@1.38.0")

add_bare_bundle(
  bare_kit_bundle
  ENTRY src/worklet.js
  OUT src/worklet.bundle.h
  BUILTINS src/builtins.json
  COMPRESSED
)

set(sources
  src/ipc.c
  src/ipc.h
  src/platform.h
  src/queue.c
  src/queue.h
  src/resources.h
  src/suspension.h
  src/symbols.h
  src/worklet.bundle.h
  src/worklet.c
  src/worklet.h
)

set(libraries)

set(exports)

if(APPLE)
  list(APPEND sources
    src/apple/ipc.m
    src/apple/resources.c
  )

  list(APPEND libraries "-framework CoreFoundation")
else()
  list(APPEND sources src/standalone/ipc.c)
endif()

if(ANDROID)
  list(APPEND sources
    src/android/jvm.h
    src/android/platform.c
    src/android/resources.c
  )

  list(APPEND libraries android)

  list(APPEND exports JNI_OnLoad)
else()
  list(APPEND sources src/platform.c)
endif()

if(NOT APPLE AND NOT ANDROID)
  list(APPEND sources src/resources.c)
endif()

if(IOS)
  list(APPEND sources src/ios/suspension.m)

  list(APPEND libraries "-framework UIKit")
else()
  list(APPEND sources src/suspension.c)
endif()

if(APPLE OR LINUX)
  list(APPEND sources src/unix/symbols.c)

  list(APPEND libraries ${CMAKE_DL_LIBS})
else()
  list(APPEND sources src/symbols.c)
endif()

add_library_targets(
  bare_kit
  HEADERS
    include/bare-kit.h
  FEATURES
    c_std_11
  SOURCES
    ${sources}
  LINK
    PRIVATE
      $<LINK_LIBRARY:WHOLE_ARCHIVE,bare::static>
      rlimit::static
      ${libraries}
  EXPORTS
    ${exports}
  EXPORTS_FROM
    src/exports.json
  STRIP ON
  EXPORT OFF
)

link_bare_modules(bare_kit::<TYPE> FROM src/builtins.json EXCLUDE_FROM bare::static)

target_link_libraries(
  bare_kit_exports
  INTERFACE
    bare::exports
)

set_target_properties(
  bare_kit_static
  bare_kit_shared
  PROPERTIES
  OUTPUT_NAME bare-kit
)

if(ANDROID)
  target_link_options(
    bare_kit_shared
    PRIVATE
      "-Wl,-z,global" # Resolve addons against the library when loaded locally
  )
endif()

if(LINUX)
  target_link_options(
    bare_kit_shared
    PUBLIC
      "-Wl,-z,now" # Use eager binding
  )
endif()

add_test_subdirectory(test)
