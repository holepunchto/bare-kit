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
    ipc.def
    worklet.def
)

target_link_libraries(
  bare_kit
  PRIVATE
    bare_worklet
)

# A prebuilt addon delay-loads `bare.dll` and resolves it against the host
# executable, so a host embedding the runtime here has to re-export these
# symbols for any addon to load. Ship the forwarders next to the library so that
# a host passes `/DEF:` at this file instead of deriving the list itself.
set(exports
  "${bare}/src/bare.def"
  "${bare}/src/js.def"
  "${bare}/src/napi.def"
  "${bare}/src/uv.def"
  "${CMAKE_CURRENT_LIST_DIR}/ipc.def"
  "${CMAKE_CURRENT_LIST_DIR}/worklet.def"
)

set(forwarders "EXPORTS\n")

foreach(export ${exports})
  file(STRINGS "${export}" symbols)

  foreach(symbol ${symbols})
    string(STRIP "${symbol}" symbol)

    if(symbol MATCHES "^[A-Za-z_][A-Za-z0-9_]*$" AND NOT symbol STREQUAL "EXPORTS")
      string(APPEND forwarders "\t${symbol} = bare-kit.${symbol}\n")
    endif()
  endforeach()
endforeach()

file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bare-kit.def" CONTENT "${forwarders}")
