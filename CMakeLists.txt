cmake_minimum_required(VERSION 3.25)

project(bare_kit LANGUAGES C VERSION 1.0)

include(bare)

install_node_modules(LOCKFILE)

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
