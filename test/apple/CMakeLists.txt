list(APPEND tests
  worklet
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

