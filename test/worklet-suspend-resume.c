#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <uv.h>

#include <bare-kit.h>

static bool suspend_called = false;
static bool resume_called = false;

static void
on_suspend(int linger, void *data) {
  suspend_called = true;

  assert(linger == 10);
}

static void
on_resume(void *data) {
  resume_called = true;
}

int
main() {
  int e;

  bare_worklet_t *worklet;
  e = bare_worklet_create(NULL, &worklet);
  assert(e == 0);

  e = bare_worklet_on_suspend(worklet, on_suspend, NULL);
  assert(e == 0);

  e = bare_worklet_on_resume(worklet, on_resume, NULL);
  assert(e == 0);

  // Keep Bare.IPC open so suspend/resume exercise the queue's ref/unref.
  char *code = "BareKit.IPC.on('data', () => {})";

  e = bare_worklet_start(worklet, "app.js", code, strlen(code), 0, NULL);
  assert(e == 0);

  e = bare_worklet_suspend(worklet, 10);
  assert(e == 0);

  uv_sleep(100); // Let suspension flush

  assert(suspend_called);

  e = bare_worklet_resume(worklet);
  assert(e == 0);

  uv_sleep(100); // Let resumption flush

  assert(resume_called);

  e = bare_worklet_terminate(worklet);
  assert(e == 0);

  bare_worklet_destroy(worklet);
}
