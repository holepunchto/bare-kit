add_library(bare_kit SHARED)

set_target_properties(
  bare_kit
  PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
  OUTPUT_NAME bare-kit
)

target_link_libraries(
  bare_kit
  PRIVATE
    bare_worklet
)

target_include_directories(
  bare_kit
  INTERFACE
    $<TARGET_PROPERTY:bare,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_options(
  bare_kit
  PUBLIC
    "-Wl,-z,now" # Use eager binding
)
