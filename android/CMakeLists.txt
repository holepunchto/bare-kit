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
    src/main/jni/Worklet.c
)

target_link_libraries(
  bare_kit
  PUBLIC
    bare_worklet
  PRIVATE
    nativehelper # For `java.io.FileDescriptor` APIs
)
