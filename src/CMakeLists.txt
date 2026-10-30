add_bare_bundle(
  bare_kit_bundle
  ENTRY src/worklet.js
  OUT src/worklet.bundle.h
  BUILTINS src/builtins.json
  WORKING_DIRECTORY ..
)

add_library(bare_kit OBJECT)

add_library(bare::kit ALIAS bare_kit)

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
    symbols.h
    worklet.c
    worklet.bundle.h
    worklet.h
)

target_include_directories(
  bare_kit
  PUBLIC
    ../include
    $<TARGET_PROPERTY:bare::headers,INTERFACE_INCLUDE_DIRECTORIES>
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
    $<LINK_LIBRARY:WHOLE_ARCHIVE,bare::static>
  PUBLIC
    rlimit_static
)

if(APPLE OR LINUX)
  target_sources(
    bare_kit
    PRIVATE
      unix/symbols.c
  )

  target_link_libraries(
    bare_kit
    PUBLIC
      ${CMAKE_DL_LIBS}
  )
else()
  target_sources(
    bare_kit
    PRIVATE
      symbols.c
  )
endif()

link_bare_modules(
  bare_kit
  WORKING_DIRECTORY ..
  EXCLUDE
    bare-buffer
    bare-hrtime
    bare-inspect
    bare-logger
    bare-module
    bare-module-lexer
    bare-path
    bare-structured-clone
    bare-system-logger
    bare-timers
    bare-type
    bare-type-stripper
    bare-url
)

file(READ exports.json exports_json)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS exports.json)

set(exports)

string(JSON symbols LENGTH "${exports_json}")

math(EXPR last_symbol "${symbols} - 1")

foreach(i RANGE ${last_symbol})
  string(JSON symbol GET "${exports_json}" ${i})

  list(APPEND exports ${symbol})
endforeach()

if(ANDROID)
  list(APPEND exports JNI_OnLoad)
endif()

add_library(bare_kit_exports INTERFACE)

add_library(bare::kit::exports ALIAS bare_kit_exports)

if(WIN32)
  set(exports_file "${CMAKE_CURRENT_BINARY_DIR}/bare-kit.def")

  list(TRANSFORM exports PREPEND "  ")
  list(JOIN exports "\n" exports)

  file(CONFIGURE OUTPUT "${exports_file}" CONTENT "EXPORTS\n${exports}\n" @ONLY)

  target_sources(
    bare_kit_exports
    INTERFACE
      "${exports_file}"
  )
elseif(APPLE)
  set(exports_file "${CMAKE_CURRENT_BINARY_DIR}/bare-kit.exp")

  list(TRANSFORM exports PREPEND "_")
  list(JOIN exports "\n" exports)

  file(CONFIGURE OUTPUT "${exports_file}" CONTENT "${exports}\n" @ONLY)

  target_link_options(
    bare_kit_exports
    INTERFACE
      "-Wl,-exported_symbols_list,${exports_file}"
  )
else()
  set(exports_file "${CMAKE_CURRENT_BINARY_DIR}/bare-kit.map")

  list(TRANSFORM exports PREPEND "    ")
  list(JOIN exports ";\n" exports)

  file(CONFIGURE OUTPUT "${exports_file}" CONTENT "{\n  global:\n${exports};\n\n  local:\n    *;\n};\n" @ONLY)

  target_link_options(
    bare_kit_exports
    INTERFACE
      "-Wl,--version-script=${exports_file}"
  )
endif()

set_target_properties(
  bare_kit_exports
  PROPERTIES
  INTERFACE_LINK_DEPENDS "${exports_file}"
)

target_link_libraries(
  bare_kit_exports
  INTERFACE
    bare::exports
)

add_library(bare_kit_shared SHARED)

add_library(bare::kit::shared ALIAS bare_kit_shared)

set_target_properties(
  bare_kit_shared
  PROPERTIES
  OUTPUT_NAME bare-kit
)

if(WIN32)
  target_link_libraries(
    bare_kit_shared
    PUBLIC
      bare_kit_exports
  )
else()
  target_link_libraries(
    bare_kit_shared
    PRIVATE
      bare_kit_exports
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

if(APPLE)
  target_link_options(
    bare_kit_shared
    PRIVATE
      $<$<CONFIG:Release>:-Wl,-dead_strip,-S,-x>
  )
elseif(ANDROID OR LINUX)
  target_link_options(
    bare_kit_shared
    PRIVATE
      $<$<CONFIG:Release>:-Wl,--gc-sections,--strip-all>
  )
endif()

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

add_library(bare_kit_static STATIC)

add_library(bare::kit::static ALIAS bare_kit_static)

set_target_properties(
  bare_kit_static
  PROPERTIES
  OUTPUT_NAME bare-kit
  PREFIX lib
)

if(WIN32)
  target_link_libraries(
    bare_kit_static
    PUBLIC
      bare_kit_exports
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
