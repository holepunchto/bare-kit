add_library(bare_kit SHARED)

set_target_properties(
  bare_kit
  PROPERTIES
  OBJC_STANDARD 11
  OUTPUT_NAME bare-kit
)

target_sources(
  bare_kit
  PUBLIC
    BareKit/BareKit.h
  PRIVATE
    BareKit/BareKit.m
)

target_include_directories(
  bare_kit
  PUBLIC
    .
)

target_link_libraries(
  bare_kit
  PUBLIC
    "-framework Foundation"
  PUBLIC
    bare_worklet
  PRIVATE
    rpc_static
)
