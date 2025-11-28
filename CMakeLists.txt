cmake_minimum_required(VERSION 4.0)

find_package(cmake-bare REQUIRED PATHS node_modules/cmake-bare)
find_package(cmake-bare-bundle REQUIRED PATHS node_modules/cmake-bare-bundle)
find_package(cmake-fetch REQUIRED PATHS node_modules/cmake-fetch)
find_package(cmake-napi REQUIRED PATHS node_modules/cmake-napi)

project(bare_kit LANGUAGES C CXX VERSION 1.13.2)

include(overrides.cmake)

if(APPLE)
  enable_language(OBJC)
endif()

fetch_package("github:holepunchto/bare@1.24.5")

add_subdirectory(shared)

if(APPLE)
  add_subdirectory(apple)
endif()

if(ANDROID)
  add_subdirectory(android)
endif()

if(LINUX)
  add_subdirectory(linux)
endif()

if(PROJECT_IS_TOP_LEVEL)
  enable_testing()

  add_subdirectory(test)
endif()
