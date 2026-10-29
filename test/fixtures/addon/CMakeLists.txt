add_bare_module(addon INSTALL OFF)

target_sources(
  ${addon}
  PRIVATE
    binding.c
)

set(addon ${addon} PARENT_SCOPE)
