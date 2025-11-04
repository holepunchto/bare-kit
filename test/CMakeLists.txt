add_subdirectory(shared)

if(APPLE)
  add_subdirectory(apple)
endif()

if(LINUX)
  add_subdirectory(linux)
endif()
