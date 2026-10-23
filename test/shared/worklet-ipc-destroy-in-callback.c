#include <assert.h>
#include <string.h>
#include <uv.h>

#include "../../shared/ipc.h"
#include "../../shared/worklet.h"

// An ipc may be destroyed from its own poll callback.

static uv_async_t finished;

static bare_ipc_t ipc;
static bare_ipc_poll_t poll;

static void
on_poll(bare_ipc_poll_t *poll, int events) {
  char data[BARE_IPC_READ_BUFFER_SIZE];

  int len = bare_ipc_read(&ipc, data, sizeof(data));

  if (len == bare_ipc_would_block) return;

  bare_ipc_poll_destroy(poll);
  bare_ipc_destroy(&ipc);

  uv_async_send(&finished);
}

static void
on_finished(uv_async_t *handle) {
  uv_close((uv_handle_t *) handle, NULL);
}

int
main() {
  int e;

  uv_loop_t *loop = uv_default_loop();

  e = uv_async_init(loop, &finished, on_finished);
  assert(e == 0);

  bare_worklet_t worklet;
  e = bare_worklet_init(&worklet, NULL);
  assert(e == 0);

  char *code = "Bare.IPC.write('Hello!')";

  uv_buf_t source = uv_buf_init(code, strlen(code));

  e = bare_worklet_start(&worklet, "app.js", &source, 0, NULL);
  assert(e == 0);

  e = bare_ipc_init(&ipc, &worklet);
  assert(e == 0);

  e = bare_ipc_poll_init(&poll, &ipc);
  assert(e == 0);

  e = bare_ipc_poll_start(&poll, bare_ipc_readable, on_poll);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);

  bare_worklet_destroy(&worklet);
}
