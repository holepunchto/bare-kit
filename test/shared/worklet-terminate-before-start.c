#include <assert.h>
#include <uv.h>

#include "../../shared/ipc.h"
#include "../../shared/worklet.h"

// A worklet that was never started can be suspended, resumed, woken up,
// terminated and pushed to without crashing, and destroying it gives an open ipc
// EOF.

static void
on_push(bare_worklet_push_t *req, const char *error, const uv_buf_t *reply) {
  assert(false);
}

int
main() {
  int e;

  bare_worklet_t worklet;
  e = bare_worklet_init(&worklet, NULL);
  assert(e == 0);

  bare_ipc_t ipc;
  e = bare_ipc_init(&ipc, &worklet);
  assert(e == 0);

  e = bare_worklet_suspend(&worklet, 0);
  assert(e == 0);

  e = bare_worklet_resume(&worklet);
  assert(e == 0);

  e = bare_worklet_wakeup(&worklet, 0);
  assert(e == 0);

  e = bare_worklet_terminate(&worklet);
  assert(e == 0);

  uv_buf_t payload = uv_buf_init("ping", 4);

  bare_worklet_push_t req;
  e = bare_worklet_push(&worklet, &req, &payload, on_push);
  assert(e < 0);

  bare_worklet_destroy(&worklet);

  char data[BARE_IPC_READ_BUFFER_SIZE];
  int len;
  len = bare_ipc_read(&ipc, data, sizeof(data));
  assert(len == 0);

  e = bare_ipc_write(&ipc, "late", 4);
  assert(e == bare_ipc_error);

  bare_ipc_destroy(&ipc);
}
