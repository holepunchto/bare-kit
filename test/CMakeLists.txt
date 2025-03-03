add_subdirectory(shared)

if(APPLE)
  add_subdirectory(apple)
endif()

if(IOS)
  add_subdirectory(ios)
endif()

