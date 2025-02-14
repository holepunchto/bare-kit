message(STATUS "Building for Apple")
if(IOS)
    message(STATUS "Building for iOS")
    add_subdirectory(ios)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message(STATUS "Building for macOS (Darwin)")
    add_subdirectory(macos)
endif()

