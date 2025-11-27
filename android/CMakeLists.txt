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
  PRIVATE
    src/main/jni/IPC.c
    src/main/jni/Worklet.c
)

target_link_libraries(
  bare_kit
  PRIVATE
    android
  PUBLIC
    bare_worklet
)

target_link_options(
  bare_kit
  PRIVATE
    "-Wl,-z,global"
)
