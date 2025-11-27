add_library(bare_kit SHARED)

set_target_properties(
  bare_kit
  PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
  OUTPUT_NAME bare-kit
)

target_sources(
  bare_kit
  PUBLIC
    ${bare_SOURCE_DIR}/src/bare.def
    ${bare_SOURCE_DIR}/src/js.def
    ${bare_SOURCE_DIR}/src/napi.def
  PUBLIC
    ../shared/windows/ipc.def
    ../shared/windows/suspension.def
    ../shared/windows/worklet.def
)

target_link_libraries(
  bare_kit
  PUBLIC
    bare_worklet
)
