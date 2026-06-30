#include <uv.h>

#include "../../shared/ipc.h"
#include "../../shared/worklet.h"

// When the worklet exits on its own (Bare.exit()), Bare.IPC is closed as part
// of process exit, so the host's read reaches EOF (a zero-length read) without
// the host having to call terminate/destroy first.

static uv_async_t finished;
static bool eof = false;

static void
on_poll(bare_ipc_poll_t *poll, int events) {
  if ((events & bare_ipc_readable) == 0) return;

  void *data;
  size_t len;
  int err = bare_ipc_read(bare_ipc_poll_get_ipc(poll), &data, &len);
  assert(err == 0 || err == bare_ipc_would_block);

  if (err == 0 && len == 0) {
    eof = true;

    bare_ipc_poll_stop(poll);

    uv_async_send(&finished);
  }
}

static void
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

  char *code = "setTimeout(() => Bare.exit(0), 50)";
  uv_buf_t source = uv_buf_init(code, strlen(code));
  err = bare_worklet_start(&worklet, "app.js", &source, 0, NULL);
  assert(err == 0);

  bare_ipc_t ipc;
  err = bare_ipc_init(&ipc, &worklet);
  assert(err == 0);

  bare_ipc_poll_t poll;
  err = bare_ipc_poll_init(&poll, &ipc);
  assert(err == 0);

  err = bare_ipc_poll_start(&poll, bare_ipc_readable, on_poll);
  assert(err == 0);

  err = uv_run(loop, UV_RUN_DEFAULT);
  assert(err == 0);

  assert(eof); // The self-exit closed Bare.IPC and the host saw EOF

  err = uv_loop_close(loop);
  assert(err == 0);

  bare_ipc_poll_destroy(&poll);

  bare_ipc_destroy(&ipc);

  bare_worklet_destroy(&worklet);
}
