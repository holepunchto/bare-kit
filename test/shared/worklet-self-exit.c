#include <uv.h>

#include "../../shared/worklet.h"

static bool exit_called = false;

static void
on_worklet_exit(bare_worklet_t *worklet, void *data) {
  exit_called = true;
}

int
main() {
  int e;

  bare_worklet_t worklet;
  e = bare_worklet_init(&worklet, NULL);
  assert(e == 0);

  e = bare_worklet_on_exit(&worklet, on_worklet_exit, NULL);
  assert(e == 0);

  char *code = "setTimeout(() => Bare.exit(0), 10)";

  uv_buf_t source = uv_buf_init(code, strlen(code));

  e = bare_worklet_start(&worklet, "app.js", &source, 0, NULL);
  assert(e == 0);

  uv_sleep(500); // Let the worklet exit on its own and flush the callback

  assert(exit_called);

  bare_worklet_destroy(&worklet);
}
