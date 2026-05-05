#include <uv.h>

#include "../../shared/worklet.h"

static bool suspend_called = false;
static bool resume_called = false;

static void
on_suspend(bare_t *bare, int linger, void *data) {
  suspend_called = true;

  assert(linger == 10);
}

static void
on_resume(bare_t *bare, void *data) {
  resume_called = true;
}

int
main() {
  int e;

  bare_worklet_t worklet;
  e = bare_worklet_init(&worklet, NULL);
  assert(e == 0);

  e = bare_worklet_on_suspend(&worklet, on_suspend, NULL);
  assert(e == 0);

  e = bare_worklet_on_resume(&worklet, on_resume, NULL);
  assert(e == 0);

  char *code = "console.log('Hello world')";

  uv_buf_t source = uv_buf_init(code, strlen(code));

  e = bare_worklet_start(&worklet, "app.js", &source, 0, NULL);
  assert(e == 0);

  e = bare_worklet_suspend(&worklet, 10);
  assert(e == 0);

  uv_sleep(100); // Let suspension flush

  assert(suspend_called);

  e = bare_worklet_resume(&worklet);
  assert(e == 0);

  uv_sleep(100); // Let resumption flush

  assert(resume_called);

  e = bare_worklet_terminate(&worklet);
  assert(e == 0);

  bare_worklet_destroy(&worklet);
}
