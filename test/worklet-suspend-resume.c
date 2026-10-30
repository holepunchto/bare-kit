#include <assert.h>
#include <string.h>
#include <uv.h>

#include <bare-kit.h>

// The open ipc keeps the worklet alive until a suspension unrefs it, so each
// suspension idles and each resumption refs it again.

static int suspends = 0;
static int idles = 0;
static int resumes = 0;

static void
on_suspend(int linger, void *data) {
  assert(linger == 10);

  suspends++;
}

static void
on_idle(void *data) {
  idles++;
}

static void
on_resume(void *data) {
  resumes++;
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

  char *code = "Bare.IPC.on('data', () => {})";

  e = bare_worklet_start(worklet, "app.js", code, strlen(code), 0, NULL);
  assert(e == 0);

  for (int i = 1; i <= 3; i++) {
    if (i > 1) {
      e = bare_worklet_resume(worklet);
      assert(e == 0);
    }

    e = bare_worklet_suspend(worklet, 10);
    assert(e == 0);

    uv_sleep(100);

    assert(suspends == i);
    assert(resumes == i - 1);
    assert(idles >= i);
  }

  bare_worklet_destroy(worklet);

  e = bare_worklet_teardown();
  assert(e == 0);
}
