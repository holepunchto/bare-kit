#include <assert.h>
#include <uv.h>

#include "common-worklet-ipc.h"

static char *buffer = "Hello, World!\n";
static uv_async_t finished;

void
on_write(bare_kit_context_t *context);
void
on_read(bare_kit_context_t *context, const char *data, size_t len);
void
on_finish(uv_async_t *handle);

void
on_write(bare_kit_context_t *context) {
  bare_kit_read(context, on_read);
}

void
on_read(bare_kit_context_t *context, const char *data, size_t len) {
  assert(strcmp(data, buffer) == 0);
  uv_async_send(&finished);
}

void
on_finish(uv_async_t *handle) {
  uv_close((uv_handle_t *) &finished, NULL);
}

int
main() {
  int err;

  uv_loop_t *loop = uv_default_loop();

  err = uv_async_init(loop, &finished, on_finish);
  assert(err == 0);

  bare_kit_context_t context;
  context.readable = NULL;
  context.writable = NULL;
  context.pending_read = NULL;
  context.pending_write = NULL;
  context.data = NULL;
  context.len = 0;

  err = bare_worklet_init(&context.worklet, NULL);
  assert(err == 0);

  char *code = "BareKit.IPC.on('data', (data) => BareKit.IPC.write(data))";
  uv_buf_t source = uv_buf_init(code, strlen(code));
  err = bare_worklet_start(&context.worklet, "app.js", &source, 0, NULL);
  assert(err == 0);

  err = bare_ipc_init(&context.ipc, &context.worklet);
  assert(err == 0);

  err = bare_ipc_poll_init(&context.poll, &context.ipc);
  assert(err == 0);

  bare_ipc_poll_set_data(&context.poll, (void *) &context);

  bare_kit_write(&context, buffer, strlen(buffer), on_write);

  err = uv_run(loop, UV_RUN_DEFAULT);
  assert(err == 0);

  err = uv_loop_close(loop);
  assert(err == 0);

  err = bare_worklet_terminate(&context.worklet);
  assert(err == 0);

  bare_ipc_poll_destroy(&context.poll);

  bare_ipc_destroy(&context.ipc);

  bare_worklet_destroy(&context.worklet);
}
