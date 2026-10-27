#include <assert.h>
#include <js.h>

#include "resources.h"

void
bare_kit__create_resources(js_env_t *env, const bare_worklet_options_t *options, js_value_t **directory, js_value_t **read) {
  int err;

  err = js_get_null(env, directory);
  assert(err == 0);

  err = js_get_null(env, read);
  assert(err == 0);
}
