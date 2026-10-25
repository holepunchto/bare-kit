list(APPEND tests
  queue
  queue-threads
  queue-signal-lock
  worklet-assets
  worklet-destroy-before-start
  worklet-exit-code
  worklet-exit-destroy
  worklet-exit-eof
  worklet-ipc
  worklet-ipc-after-destroy
  worklet-ipc-destroy-in-callback
  worklet-ipc-exit
  worklet-ipc-half-close
  worklet-ipc-large-write
  worklet-ipc-short-read
  worklet-ipc-single
  worklet-ipc-write-after-close
  worklet-jsend-eof
  worklet-suspend-resume
  worklet-suspend-resume-suspend
  worklet-teardown
  worklet-terminate-after-suspend
  worklet-terminate-before-start
)

execute_process(
  COMMAND node fixtures/generate.js
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
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
