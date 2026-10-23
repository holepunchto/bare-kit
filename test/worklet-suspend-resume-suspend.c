#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <uv.h>

#include <bare-kit.h>

static int suspend_calls = 0;
static int resume_calls = 0;
static int idle_calls = 0;

static void
on_suspend(int linger, void *data) {
  suspend_calls++;
}

static void
on_idle(void *data) {
  idle_calls++;
}

static void
on_resume(void *data) {
  resume_calls++;
}

int
main() {
  int e;

  bare_worklet_t *worklet;
  e = bare_worklet_create(NULL, &worklet);
  assert(e == 0);

  e = bare_worklet_on_suspend(worklet, on_suspend, NULL);
  assert(e == 0);

  e = bare_worklet_on_idle(worklet, on_idle, NULL);
  assert(e == 0);

  e = bare_worklet_on_resume(worklet, on_resume, NULL);
  assert(e == 0);

  char *code = "console.log('Hello world')";

  e = bare_worklet_start(worklet, "app.js", code, strlen(code), 0, NULL);
  assert(e == 0);

  e = bare_worklet_suspend(worklet, 10);
  assert(e == 0);

  e = bare_worklet_resume(worklet);
  assert(e == 0);

  e = bare_worklet_suspend(worklet, 10);
  assert(e == 0);

  uv_sleep(100); // Let the transitions flush

  assert(suspend_calls == 2);
  assert(resume_calls == 1);
  assert(idle_calls >= 1);

  e = bare_worklet_resume(worklet);
  assert(e == 0);

  e = bare_worklet_suspend(worklet, 10);
  assert(e == 0);

  uv_sleep(100); // Let the transitions flush

  assert(suspend_calls == 3);
  assert(resume_calls == 2);
  assert(idle_calls >= 2);

  e = bare_worklet_terminate(worklet);
  assert(e == 0);

  bare_worklet_destroy(worklet);

  e = bare_worklet_teardown();
  assert(e == 0);
}
