#include <uv.h>

#include "../../shared/worklet.h"

static int suspend_calls = 0;
static int resume_calls = 0;
static int idle_calls = 0;

static void
on_suspend(bare_t *bare, int linger, void *data) {
  suspend_calls++;
}

static void
on_idle(bare_t *bare, void *data) {
  idle_calls++;
}

static void
on_resume(bare_t *bare, void *data) {
  resume_calls++;
}

int
main() {
  int e;

  bare_worklet_t worklet;
  e = bare_worklet_init(&worklet, NULL);
  assert(e == 0);

  e = bare_worklet_on_suspend(&worklet, on_suspend, NULL);
  assert(e == 0);

  e = bare_worklet_on_idle(&worklet, on_idle, NULL);
  assert(e == 0);

  e = bare_worklet_on_resume(&worklet, on_resume, NULL);
  assert(e == 0);

  char *code = "console.log('Hello world')";

  uv_buf_t source = uv_buf_init(code, strlen(code));

  e = bare_worklet_start(&worklet, "app.js", &source, 0, NULL);
  assert(e == 0);

  e = bare_worklet_suspend(&worklet, 10);
  assert(e == 0);

  e = bare_worklet_resume(&worklet);
  assert(e == 0);

  e = bare_worklet_suspend(&worklet, 10);
  assert(e == 0);

  uv_sleep(100); // Let the transitions flush

  assert(suspend_calls == 2);
  assert(resume_calls == 1);
  assert(idle_calls >= 1);

  e = bare_worklet_resume(&worklet);
  assert(e == 0);

  e = bare_worklet_suspend(&worklet, 10);
  assert(e == 0);

  uv_sleep(100); // Let the transitions flush

  assert(suspend_calls == 3);
  assert(resume_calls == 2);
  assert(idle_calls >= 2);

  e = bare_worklet_terminate(&worklet);
  assert(e == 0);

  bare_worklet_destroy(&worklet);
}
