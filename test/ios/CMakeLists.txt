add_executable(worklet_test_ios
    worklet.m
)

set_target_properties(worklet_test_ios PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
)

target_link_libraries(worklet_test_ios
    PRIVATE
    bare_kit
    "-framework UIKit"
    "-framework Foundation"
)

add_test(
    NAME worklet_test_ios
    COMMAND worklet_test_ios
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

set_tests_properties(
    worklet_test_ios
    PROPERTIES
    TIMEOUT 120
) 

