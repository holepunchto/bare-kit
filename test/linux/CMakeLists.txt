add_executable(segfault segfault.c)

target_link_libraries(
  segfault
  PRIVATE
    bare_kit
)

add_test(
  NAME segfault
  COMMAND segfault
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

set_target_properties(
  segfault
  PROPERTIES
  TIMEOUT 120
)
