add_bare_bundle(
  bare_worklet_bundle
  ENTRY shared/worklet.js
  OUT shared/worklet.bundle.h
  BUILTINS shared/builtins.json
  WORKING_DIRECTORY ..
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
    ipc.h
    ipc.c
    worklet.c
    worklet.bundle.h
    worklet.h
)

target_link_libraries(
  bare_worklet
  PUBLIC
    $<LINK_LIBRARY:WHOLE_ARCHIVE,bare_static>
  PUBLIC
    log_static
)

link_bare_modules(bare_worklet WORKING_DIRECTORY ..)
