#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <uv.h>

#include <bare-kit.h>

// A worklet that was never started can be suspended, resumed, woken up,
// terminated and pushed to without crashing, and destroying it gives an open ipc
// EOF.

static void
on_push(const char *error, const void *reply, size_t len, void *data) {
  assert(false);
}

int
main() {
  int e;

  bare_worklet_t *worklet;
  e = bare_worklet_create(NULL, &worklet);
  assert(e == 0);

  bare_ipc_t *ipc;
  e = bare_ipc_create(worklet, &ipc);
  assert(e == 0);

  e = bare_worklet_suspend(worklet, 0);
  assert(e == 0);

  e = bare_worklet_resume(worklet);
  assert(e == 0);

  e = bare_worklet_wakeup(worklet, 0);
  assert(e == 0);

  e = bare_worklet_terminate(worklet);
  assert(e == 0);

  e = bare_worklet_push(worklet, "ping", 4, on_push, NULL);
  assert(e < 0);

  bare_worklet_destroy(worklet);

  char data[BARE_IPC_READ_BUFFER_SIZE];
  int len;
  len = bare_ipc_read(ipc, data, sizeof(data));
  assert(len == 0);

  e = bare_ipc_write(ipc, "late", 4);
  assert(e == bare_ipc_error);

  bare_ipc_destroy(ipc);

  e = bare_worklet_teardown();
  assert(e == 0);
}
