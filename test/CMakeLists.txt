add_test_executables(
  queue.c
  queue-threads.c
  queue-signal-lock.c
  TIMEOUT 120
  LINK
    bare::headers
    bare_kit::static
)

add_test_executables(
  worklet-abandon.c
  worklet-clear-callback.c
  worklet-exit-code.c
  worklet-ipc.c
  worklet-ipc-after-destroy.c
  worklet-ipc-destroy-in-callback.c
  worklet-ipc-exit.c
  worklet-ipc-half-close.c
  worklet-ipc-host-end.c
  worklet-ipc-short-read.c
  worklet-ipc-single.c
  worklet-ipc-write-after-close.c
  worklet-suspend-resume.c
  worklet-teardown.c
  worklet-terminate-before-start.c
  TIMEOUT 120
  LINK
    bare::headers
    bare_kit::<TYPE>
)

add_subdirectory(fixtures/addon)

add_test_executable(
  worklet-addon-static
  worklet-addon.c
  TIMEOUT 120
  LINK
    bare::headers
    $<LINK_LIBRARY:WHOLE_ARCHIVE,bare_kit::static>
    bare_kit::exports
)

add_test_executable(
  worklet-addon-shared
  worklet-addon.c
  TIMEOUT 120
  LINK
    bare::headers
    bare_kit::shared
)

add_library(embedder SHARED fixtures/embedder.c)

target_link_libraries(
  embedder
  PRIVATE
    $<LINK_LIBRARY:WHOLE_ARCHIVE,bare_kit::static>
    bare_kit::exports
)

if(ANDROID)
  target_link_options(
    embedder
    PRIVATE
      "-Wl,-z,global"
  )
endif()

add_test_executable(
  worklet-addon-dynamic
  worklet-addon-dynamic.c
  TIMEOUT 120
  LINK
    bare_kit::headers
    ${CMAKE_DL_LIBS}
)

target_compile_definitions(
  worklet-addon-dynamic
  PRIVATE
    BARE_KIT="$<TARGET_FILE:bare_kit::shared>"
)

add_dependencies(worklet-addon-dynamic bare_kit::shared)

add_test_executable(
  worklet-addon-embedded
  worklet-addon-dynamic.c
  TIMEOUT 120
  LINK
    bare_kit::headers
    ${CMAKE_DL_LIBS}
)

target_compile_definitions(
  worklet-addon-embedded
  PRIVATE
    BARE_KIT="$<TARGET_FILE:embedder>"
)

add_dependencies(worklet-addon-embedded embedder)

foreach(test IN ITEMS worklet-addon-static worklet-addon-shared worklet-addon-dynamic worklet-addon-embedded)
  target_compile_definitions(
    ${test}
    PRIVATE
      ADDON="$<IF:$<BOOL:${WIN32}>,file:///,file://>$<TARGET_FILE:${addon}_module>"
  )

  add_dependencies(${test} ${addon}_module)
endforeach()

if(APPLE OR LINUX)
  add_test_executable(
    worklet-global-scope
    worklet-global-scope.c
    TIMEOUT 120
    LINK
      bare_kit::headers
      ${CMAKE_DL_LIBS}
  )

  target_compile_definitions(
    worklet-global-scope
    PRIVATE
      BARE_KIT="$<TARGET_FILE:bare_kit::shared>"
  )

  add_dependencies(worklet-global-scope bare_kit::shared)
endif()
