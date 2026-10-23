list(APPEND tests
  queue
  queue-threads
  queue-signal-lock
  worklet-assets
  worklet-destroy-before-start
  worklet-exit-eof
  worklet-ipc
  worklet-ipc-after-destroy
  worklet-ipc-destroy-in-callback
  worklet-ipc-half-close
  worklet-ipc-large-write
  worklet-ipc-short-read
  worklet-ipc-single
  worklet-ipc-write-after-close
  worklet-jsend-eof
  worklet-push-after-exit
  worklet-push-terminate
  worklet-suspend-resume
  worklet-suspend-resume-suspend
  worklet-terminate-after-suspend
  worklet-terminate-before-start
)

execute_process(
  COMMAND node fixtures/generate.js
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
)

foreach(test IN LISTS tests)
  add_executable(shared_${test} ${test}.c)

  target_link_libraries(
    shared_${test}
    PRIVATE
      bare_worklet
  )

  add_test(
    NAME ${test}
    COMMAND shared_${test}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )

  set_tests_properties(
    ${test}
    PROPERTIES
    TIMEOUT 120
  )
endforeach()
