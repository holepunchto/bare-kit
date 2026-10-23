#ifndef BARE_KIT_CONTEXT_H
#define BARE_KIT_CONTEXT_H

#include <bare.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Publish the handles that only the embedder can produce for the addons of the
 * process to retrieve with `bare_context_get()`. Must be called before the
 * first `bare_load()`, as an addon only sees what was published before it was
 * loaded.
 */
void
bare_kit__publish_context(bare_t *bare);

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_CONTEXT_H
