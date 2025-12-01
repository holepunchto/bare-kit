#include <uv.h>

#include "../../shared/worklet.h"

#include "worklet-ipc.h"

#define BARE_IPC_LARGE_BUF_LEN 4 * 1024 * 1024

const char BARE_IPC_LARGE_BUF[BARE_IPC_LARGE_BUF_LEN];

size_t received = 0;
uv_async_t finished;

void
on_read(bare_worklet_ipc_t *ipc, ssize_t len, const char *data) {
  free((void *) data);

  received += len;

  if (received == BARE_IPC_LARGE_BUF_LEN) {
    uv_async_send(&finished);
  } else {
    bare_worklet_ipc__set_readable(ipc, on_read);
  }
}

void
on_write(bare_worklet_ipc_t *ipc, int status) {
  bare_worklet_ipc_read(ipc, on_read);
}

void
on_finish(uv_async_t *handle) {
  uv_close((uv_handle_t *) handle, NULL);
}

int
main() {
  int err;

  uv_loop_t *loop = uv_default_loop();
  err = uv_async_init(loop, &finished, on_finish);
  assert(err == 0);

  bare_worklet_t worklet;
  err = bare_worklet_init(&worklet, NULL);
  assert(err == 0);

  char *code = "BareKit.IPC.on('data', (data) => BareKit.IPC.write(data))";
  uv_buf_t source = uv_buf_init(code, strlen(code));
  err = bare_worklet_start(&worklet, "app.js", &source, 0, NULL);
  assert(err == 0);

  bare_worklet_ipc_t ipc;
  err = bare_worklet_ipc_init(&ipc, &worklet);
  assert(err == 0);

  bare_worklet_ipc_write(&ipc, BARE_IPC_LARGE_BUF, BARE_IPC_LARGE_BUF_LEN, on_write);

  err = uv_run(loop, UV_RUN_DEFAULT);
  assert(err == 0);

  err = uv_loop_close(loop);
  assert(err == 0);

  err = bare_worklet_terminate(&worklet);
  assert(err == 0);

  bare_worklet_ipc_destroy(&ipc);

  bare_worklet_destroy(&worklet);
}
