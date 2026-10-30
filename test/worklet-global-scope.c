#define _GNU_SOURCE

#include <assert.h>
#include <dlfcn.h>
#include <stddef.h>

#include <bare-kit.h>

// The host loads the library locally, yet the addons of a worklet must resolve
// the symbols of Bare from the global scope.

int
main() {
  int e;

  void *library = dlopen(BARE_KIT, RTLD_NOW | RTLD_LOCAL);
  assert(library != NULL);

  assert(dlsym(RTLD_DEFAULT, "js_create_string_utf8") == NULL);

  int (*worklet_create)(const bare_worklet_options_t *, bare_worklet_t **) = dlsym(library, "bare_worklet_create");
  void (*worklet_destroy)(bare_worklet_t *) = dlsym(library, "bare_worklet_destroy");
  int (*worklet_teardown)(void) = dlsym(library, "bare_worklet_teardown");

  bare_worklet_t *worklet;
  e = worklet_create(NULL, &worklet);
  assert(e == 0);

  assert(dlsym(RTLD_DEFAULT, "js_create_string_utf8") != NULL);

  worklet_destroy(worklet);

  e = worklet_teardown();
  assert(e == 0);
}
