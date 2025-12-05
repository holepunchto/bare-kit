list(APPEND tests
  worklet-assets
  worklet-destroy-before-start
  worklet-ipc
  worklet-ipc-large-write
  worklet-suspend-resume
  worklet-terminate-after-suspend
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
