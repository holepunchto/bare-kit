#include <assert.h>
#include <string.h>

#include <bare-kit.h>

// Abandoning a running worklet tears it down without reporting its exit.

static int exits = 0;

static void
on_worklet_exit(int code, void *data) {
  exits++;
}

int
main() {
  int e;

  bare_worklet_t *worklet;
  e = bare_worklet_create(NULL, &worklet);
  assert(e == 0);

  e = bare_worklet_on_exit(worklet, on_worklet_exit, NULL);
  assert(e == 0);

  char *code = "setInterval(() => {}, 1000)";

  e = bare_worklet_start(worklet, "app.js", code, strlen(code), 0, NULL);
  assert(e == 0);

  bare_worklet_abandon(worklet);

  e = bare_worklet_teardown();
  assert(e == 0);

  assert(exits == 0);
}
