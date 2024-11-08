cmake_minimum_required(VERSION 3.25)

find_package(cmake-bare-bundle REQUIRED PATHS node_modules/cmake-bare-bundle)
find_package(cmake-drive REQUIRED PATHS node_modules/cmake-drive)
find_package(cmake-napi REQUIRED PATHS node_modules/cmake-napi)
find_package(cmake-npm REQUIRED PATHS node_modules/cmake-npm)

project(bare_kit LANGUAGES C VERSION 1.0)

set(BARE_USE_SYSTEM_LOG ON CACHE BOOL "Use system logging" FORCE)

install_node_modules()

if(APPLE)
  enable_language(OBJC)
endif()

if(NOT TARGET bare)
  add_subdirectory(vendor/bare EXCLUDE_FROM_ALL)
endif()

if(NOT TARGET rpc)
  add_subdirectory(vendor/librpc EXCLUDE_FROM_ALL)
endif()

add_subdirectory(shared)

if(APPLE)
  add_subdirectory(apple)
endif()

if(ANDROID)
  add_subdirectory(android)
endif()

if(PROJECT_IS_TOP_LEVEL)
  enable_testing()

  add_subdirectory(test)
endif()
