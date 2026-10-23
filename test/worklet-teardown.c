#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>
#include <uv.h>

#include <bare-kit.h>

// Tearing down waits for the worklets that are still running, after which no
// worklet may start.

static atomic_bool exited = false;

static void
on_worklet_exit(int code, void *data) {
  atomic_store(&exited, true);
}

int
main() {
  int e;

  bare_worklet_t *worklet;
  e = bare_worklet_create(NULL, &worklet);
  assert(e == 0);

  e = bare_worklet_on_exit(worklet, on_worklet_exit, NULL);
  assert(e == 0);

  char *code = "setTimeout(() => {}, 100)";

  e = bare_worklet_start(worklet, "app.js", code, strlen(code), 0, NULL);
  assert(e == 0);

  e = bare_worklet_teardown();
  assert(e == 0);

  assert(atomic_load(&exited));

  bare_worklet_destroy(worklet);

  e = bare_worklet_create(NULL, &worklet);
  assert(e == 0);

  e = bare_worklet_start(worklet, "app.js", code, strlen(code), 0, NULL);
  assert(e < 0);

  bare_worklet_destroy(worklet);
}
