#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <uv.h>

#include <bare-kit.h>

// Once the worklet's loop has stopped, a push fails rather than blocking or
// waiting for a reply that never comes.

static void
on_push(const char *error, const void *reply, size_t len, void *data) {
  assert(false);
}

int
main() {
  int e;

  bare_worklet_t *worklet;
  e = bare_worklet_create(NULL, &worklet);
  assert(e == 0);

  char *code = "Bare.exit(0)";

  e = bare_worklet_start(worklet, "app.js", code, strlen(code), 0, NULL);
  assert(e == 0);

  uv_sleep(500);

  e = bare_worklet_push(worklet, "ping", 4, on_push, NULL);
  assert(e < 0);

  bare_worklet_destroy(worklet);

  e = bare_worklet_teardown();
  assert(e == 0);
}
