#ifndef BARE_KIT_RESOURCES_H
#define BARE_KIT_RESOURCES_H

#include <bare-kit.h>
#include <js.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the directory that relative filenames resolve against, and a function
 * that reads a resource by its relative path where resources aren't files.
 * Either is `null` if the platform has no such thing.
 */
void
bare_kit__create_resources(js_env_t *env, const bare_worklet_options_t *options, js_value_t **directory, js_value_t **read);

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_RESOURCES_H
