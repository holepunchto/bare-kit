#include "platform.h"

void
bare_kit__on_thread_enter(void) {}

void
bare_kit__on_thread_exit(void) {}

void
bare_kit__retain_context(bare_worklet_options_t *result, const bare_worklet_options_t *options) {}

void
bare_kit__release_context(const bare_worklet_options_t *options) {}

void
bare_kit__publish_context(bare_t *bare, const bare_worklet_options_t *options) {}
