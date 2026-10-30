#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <bare-kit.h>

// Destroying a running worklet still reports its exit, once, and the exit
// callback may release the host's resources. Tearing down waits for it, after
// which no worklet may start.

static int exits = 0;

static void
on_worklet_exit(int code, void *data) {
  free(data);

  exits++;
}

int
main() {
  int e;

  bare_worklet_t *worklet;
  e = bare_worklet_create(NULL, &worklet);
  assert(e == 0);

  e = bare_worklet_on_exit(worklet, on_worklet_exit, malloc(1));
  assert(e == 0);

  char *code = "setInterval(() => {}, 1000)";

  e = bare_worklet_start(worklet, "app.js", code, strlen(code), 0, NULL);
  assert(e == 0);

  bare_worklet_destroy(worklet);

  e = bare_worklet_teardown();
  assert(e == 0);

  assert(exits == 1);

  e = bare_worklet_create(NULL, &worklet);
  assert(e == 0);

  e = bare_worklet_start(worklet, "app.js", code, strlen(code), 0, NULL);
  assert(e < 0);

  bare_worklet_destroy(worklet);
}
