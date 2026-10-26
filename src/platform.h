#ifndef BARE_KIT_PLATFORM_H
#define BARE_KIT_PLATFORM_H

#include <bare-kit.h>
#include <bare.h>

#ifdef __cplusplus
extern "C" {
#endif

void
bare_kit__on_thread_enter(void);

void
bare_kit__on_thread_exit(void);

/**
 * Retain the handles of `options` that the worklet keeps for publishing into
 * its own copy of the options, `result`.
 */
void
bare_kit__retain_context(bare_worklet_options_t *result, const bare_worklet_options_t *options);

void
bare_kit__release_context(const bare_worklet_options_t *options);

/**
 * Publish the handles that only the embedder can produce for the addons of the
 * process to retrieve with `bare_context_get()`. Must be called before the
 * first `bare_load()`, as an addon only sees what was published before it was
 * loaded.
 */
void
bare_kit__publish_context(bare_t *bare, const bare_worklet_options_t *options);

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_PLATFORM_H
