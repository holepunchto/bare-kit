project(NotificationServiceExtension LANGUAGES OBJC)

add_executable(NotificationServiceExtension MACOSX_BUNDLE
    NotificationService.m
    NotificationService.h
    Info.plist
    NotificationServiceExtension.entitlements
)

set_target_properties(NotificationServiceExtension PROPERTIES
    BUNDLE_EXTENSION appex
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/NotificationServiceExtension.entitlements"
)

target_link_libraries(NotificationServiceExtension
    PRIVATE
    bare_kit
    "-framework Foundation"
    "-e _NSExtensionMain"
)

add_custom_command(
    TARGET NotificationServiceExtension
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/push.js" "${CMAKE_CURRENT_BINARY_DIR}/NotificationServiceExtension.appex/push.js"
    COMMENT "✅ Copying push-notif.js into NotificationServiceExtension.appex"
)

# set(APPLE_TEAM_ID "W4MF6H9XZ6")
# add_custom_command(
#     TARGET NotificationServiceExtension
#     POST_BUILD
#     COMMAND codesign --force --sign "${APPLE_TEAM_ID}" "$<TARGET_FILE:NotificationServiceExtension>" --entitlements "${CMAKE_CURRENT_SOURCE_DIR}/NotificationServiceExtension.entitlements"
#     COMMENT "✅ Signing NotificationServiceExtension.appex"
# )


