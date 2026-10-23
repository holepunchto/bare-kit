list(APPEND tests
  ipc-close-releases
  ipc-handler-clears-writable
  ipc-release-during-handler
  ipc-retains-worklet
  worklet-ipc
  worklet-ipc-eof
  worklet-ipc-large-write
  worklet-push
  worklet-push-error
  worklet-push-releases
  worklet-terminated
)

foreach(test IN LISTS tests)
  add_executable(${test} ${test}.m)

  target_link_libraries(
    ${test}
    PRIVATE
      bare_kit
  )

  add_test(
    NAME ${test}
    COMMAND ${test}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )

  set_tests_properties(
    ${test}
    PROPERTIES
    TIMEOUT 120
  )
endforeach()

