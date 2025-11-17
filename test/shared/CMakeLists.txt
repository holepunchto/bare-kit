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
  add_executable(${test} ${test}.c)

  if(WIN32)
    set_target_properties(
      ${test}
      PROPERTIES
      LINK_FLAGS "/STACK:10485760"
    )
  endif()

  target_link_libraries(
    ${test}
    PRIVATE
      bare_worklet
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
