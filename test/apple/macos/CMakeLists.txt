add_executable(worklet_test_macos
    worklet.m
)

target_link_libraries(worklet_test_macos
    PRIVATE
    bare_kit
)

add_test(
    NAME worklet_test_macos
    COMMAND worklet_test_macos
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

set_tests_properties(
    worklet_test_macos
    PROPERTIES
    TIMEOUT 120
) 