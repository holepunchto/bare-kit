add_bare_bundle(
  ENTRY worklet.js
  OUT worklet.bundle.h
  NAME worklet_bundle
  TARGET c
)

add_library(bare_worklet OBJECT)

set_target_properties(
  bare_worklet
  PROPERTIES
  C_STANDARD 99
  POSITION_INDEPENDENT_CODE ON
)

target_sources(
  bare_worklet
  PRIVATE
    worklet.c
    worklet.bundle.h
    worklet.h
)

target_link_libraries(
  bare_worklet
  PUBLIC
    $<LINK_LIBRARY:WHOLE_ARCHIVE,bare_static>
)

link_bare_modules(bare_worklet WORKING_DIRECTORY ..)
