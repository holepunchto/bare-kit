add_library(bare_kit SHARED)

set_target_properties(
  bare_kit
  PROPERTIES
  C_STANDARD 99
  POSITION_INDEPENDENT_CODE ON
  OUTPUT_NAME bare-kit
)

target_link_libraries(
  bare_kit
  PUBLIC
    bare_worklet
)
