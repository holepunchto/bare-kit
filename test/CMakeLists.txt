list(APPEND tests
  queue
  queue-threads
  queue-signal-lock
  worklet-exit-code
  worklet-ipc
  worklet-ipc-after-destroy
  worklet-ipc-destroy-in-callback
  worklet-ipc-exit
  worklet-ipc-half-close
  worklet-ipc-host-end
  worklet-ipc-short-read
  worklet-ipc-single
  worklet-ipc-write-after-close
  worklet-suspend-resume
  worklet-teardown
  worklet-terminate-before-start
)

foreach(test IN LISTS tests)
  add_executable(test_${test} ${test}.c)

  target_link_libraries(
    test_${test}
    PRIVATE
      bare_kit
  )

  add_test(
    NAME ${test}
    COMMAND test_${test}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )

  set_tests_properties(
    ${test}
    PROPERTIES
    TIMEOUT 120
  )
endforeach()

add_subdirectory(fixtures/addon)

add_executable(test_worklet-addon-static worklet-addon.c)

# Exported so that an addon resolves the symbols of Bare from the executable.
set_target_properties(
  test_worklet-addon-static
  PROPERTIES
  ENABLE_EXPORTS ON
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_link_libraries(
  test_worklet-addon-static
  PRIVATE
    bare_kit
)

add_executable(test_worklet-addon-shared worklet-addon.c)

target_link_libraries(
  test_worklet-addon-shared
  PRIVATE
    bare_kit_shared
)

# Links Bare Kit statically into a shared library of its own, like a language
# binding would.
add_library(test_embedder SHARED)

target_link_libraries(
  test_embedder
  PRIVATE
    bare_kit
    bare_kit_exports
)

if(ANDROID)
  target_link_options(
    test_embedder
    PRIVATE
      "-Wl,-z,global"
  )
endif()

add_executable(test_worklet-addon-dynamic worklet-addon-dynamic.c)

target_compile_definitions(
  test_worklet-addon-dynamic
  PRIVATE
    BARE_KIT="$<TARGET_FILE:bare_kit_shared>"
)

add_dependencies(test_worklet-addon-dynamic bare_kit_shared)

add_executable(test_worklet-addon-embedded worklet-addon-dynamic.c)

target_compile_definitions(
  test_worklet-addon-embedded
  PRIVATE
    BARE_KIT="$<TARGET_FILE:test_embedder>"
)

add_dependencies(test_worklet-addon-embedded test_embedder)

foreach(test IN ITEMS worklet-addon-dynamic worklet-addon-embedded)
  target_include_directories(
    test_${test}
    PRIVATE
      ../include
  )

  if(NOT WIN32)
    target_link_libraries(
      test_${test}
      PRIVATE
        ${CMAKE_DL_LIBS}
    )
  endif()
endforeach()

if(APPLE OR LINUX)
  add_executable(test_worklet-global-scope worklet-global-scope.c)

  target_include_directories(
    test_worklet-global-scope
    PRIVATE
      ../include
  )

  target_compile_definitions(
    test_worklet-global-scope
    PRIVATE
      BARE_KIT="$<TARGET_FILE:bare_kit_shared>"
  )

  target_link_libraries(
    test_worklet-global-scope
    PRIVATE
      ${CMAKE_DL_LIBS}
  )

  add_dependencies(test_worklet-global-scope bare_kit_shared)

  add_test(
    NAME worklet-global-scope
    COMMAND test_worklet-global-scope
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )

  set_tests_properties(
    worklet-global-scope
    PROPERTIES
    TIMEOUT 120
  )
endif()

foreach(test IN ITEMS worklet-addon-static worklet-addon-shared worklet-addon-dynamic worklet-addon-embedded)
  target_compile_definitions(
    test_${test}
    PRIVATE
      ADDON="$<IF:$<BOOL:${WIN32}>,file:///,file://>$<TARGET_FILE:${addon}_module>"
  )

  add_dependencies(test_${test} ${addon}_module)

  add_test(
    NAME ${test}
    COMMAND test_${test}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )

  set_tests_properties(
    ${test}
    PROPERTIES
    TIMEOUT 120
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:bare_kit_shared>"
  )
endforeach()
