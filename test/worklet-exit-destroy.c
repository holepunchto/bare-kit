#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#include <bare-kit.h>

// Destroying a running worklet still reports its exit, once, and the exit
// callback may release the host's resources.

static atomic_int exits = 0;

static void
on_worklet_exit(int code, void *data) {
  free(data);

  atomic_fetch_add(&exits, 1);
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

  for (int i = 0; i < 200 && atomic_load(&exits) == 0; i++)
    uv_sleep(10);

  uv_sleep(100);

  assert(atomic_load(&exits) == 1);

  e = bare_worklet_teardown();
  assert(e == 0);
}
