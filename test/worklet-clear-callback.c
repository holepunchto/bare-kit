#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <uv.h>

#include <bare-kit.h>

// Clearing a callback while it runs waits for it to return, so the host may
// free its data as soon as the setter returns.

static uv_sem_t entered;
static bool returned = false;

static void
on_worklet_exit(int code, void *data) {
  uv_sem_post(&entered);

  uv_sleep(100);

  returned = true;
}

int
main() {
  int e;

  e = uv_sem_init(&entered, 0);
  assert(e == 0);

  bare_worklet_t *worklet;
  e = bare_worklet_create(NULL, &worklet);
  assert(e == 0);

  e = bare_worklet_on_exit(worklet, on_worklet_exit, NULL);
  assert(e == 0);

  char *code = "Bare.exit(0)";

  e = bare_worklet_start(worklet, "app.js", code, strlen(code), 0, NULL);
  assert(e == 0);

  uv_sem_wait(&entered);

  e = bare_worklet_on_exit(worklet, NULL, NULL);
  assert(e == 0);

  assert(returned);

  bare_worklet_destroy(worklet);

  e = bare_worklet_teardown();
  assert(e == 0);
}
