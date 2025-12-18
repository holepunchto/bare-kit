add_bare_bundle(
  bare_worklet_bundle
  ENTRY shared/worklet.js
  OUT shared/worklet.bundle.h
  BUILTINS shared/builtins.json
  WORKING_DIRECTORY ..
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
    ipc.h
    ipc.c
    worklet.c
    worklet.bundle.h
    worklet.h
)

target_include_directories(
  bare_worklet
  PUBLIC
    $<TARGET_PROPERTY:bare,INTERFACE_INCLUDE_DIRECTORIES>
)

if(APPLE)
  target_sources(
    bare_worklet
    PRIVATE
      posix/ipc.h
      posix/ipc.c
      apple/ipc.h
      apple/ipc.m
      apple/suspension.h
      apple/suspension.m
  )

  if(IOS)
    target_link_libraries(
      bare_worklet
      PUBLIC
        "-framework UIKit"
    )

    target_compile_definitions(
      bare_worklet
      PUBLIC
        BARE_KIT_IOS
    )
  else()
    target_compile_definitions(
      bare_worklet
      PUBLIC
        BARE_KIT_DARWIN
    )
  endif()
endif()

if(ANDROID)
  target_sources(
    bare_worklet
    PRIVATE
      posix/ipc.h
      posix/ipc.c
      android/ipc.c
      android/ipc.h
      android/suspension.c
      android/suspension.h
  )

  target_link_libraries(
    bare_worklet
    PRIVATE
      android
  )

  target_compile_definitions(
    bare_worklet
    PUBLIC
      BARE_KIT_ANDROID
  )
endif()

if(LINUX)
  target_sources(
    bare_worklet
    PRIVATE
      posix/ipc.h
      posix/ipc.c
      linux/ipc.c
      linux/ipc.h
      linux/suspension.c
      linux/suspension.h
  )

  target_compile_definitions(
    bare_worklet
    PUBLIC
      BARE_KIT_LINUX
  )
endif()

if(WIN32)
  target_sources(
    bare_worklet
    PRIVATE
      win32/ipc.c
      win32/ipc.h
      win32/suspension.c
      win32/suspension.h
  )

  target_compile_definitions(
    bare_worklet
    PUBLIC
      BARE_KIT_WINDOWS
  )
endif()

target_link_libraries(
  bare_worklet
  INTERFACE
    $<LINK_LIBRARY:WHOLE_ARCHIVE,bare_static>
  PUBLIC
    rlimit_static
)

link_bare_modules(bare_worklet WORKING_DIRECTORY ..)
