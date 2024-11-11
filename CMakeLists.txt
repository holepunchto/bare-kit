cmake_minimum_required(VERSION 3.25)

find_package(cmake-bare-bundle REQUIRED PATHS node_modules/cmake-bare-bundle)
find_package(cmake-fetch REQUIRED PATHS node_modules/cmake-fetch)
find_package(cmake-npm REQUIRED PATHS node_modules/cmake-npm)

project(bare_kit LANGUAGES C VERSION 1.0)

set(BARE_USE_SYSTEM_LOG ON CACHE BOOL "Use system logging" FORCE)

install_node_modules()

if(APPLE)
  enable_language(OBJC)
endif()

fetch_package("github:holepunchto/bare")
fetch_package("github:holepunchto/librpc")

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
