add_executable(worklet_test_ios
    worklet.m
)

set_target_properties(worklet_test_ios PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
    CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${CMAKE_CURRENT_SOURCE_DIR}/codesign.entitlements
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

add_subdirectory(extension)

add_dependencies(worklet_test_ios NotificationServiceExtension)

add_custom_command(
    TARGET worklet_test_ios
    POST_BUILD
    COMMAND mkdir -p "$<TARGET_FILE_DIR:worklet_test_ios>/PlugIns"
    COMMAND cp -R "$<TARGET_BUNDLE_DIR:NotificationServiceExtension>" "$<TARGET_FILE_DIR:worklet_test_ios>/PlugIns"
    COMMENT "Embedding NotificationServiceExtension into the app bundle"
)

set(APPLE_TEAM_ID "W4MF6H9XZ6")
  
add_custom_command(
    TARGET worklet_test_ios
    POST_BUILD
    COMMAND codesign --force --sign "${APPLE_TEAM_ID}" "$<TARGET_FILE:worklet_test_ios>" --entitlements "${CMAKE_CURRENT_SOURCE_DIR}/codesign.entitlements"
    COMMENT "Copying push.js into NotificationServiceExtension.appex"
)

