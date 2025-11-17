#include <assert.h>
#include <uv.h>

#include "../../shared/ipc.h"
#include "worklet-ipc.h"

#define BUFFER_LEN 4 * 1024 * 1024

static uint8_t buffer[BUFFER_LEN];
static uv_async_t finished;
static size_t received = 0;

void
on_exit(uv_async_t *handle);
void
on_read(bare_kit_context_t *context);
void
on_write(bare_kit_context_t *context);

void
on_exit(uv_async_t *handle) {
  uv_close((uv_handle_t *) &finished, NULL);
}

void
on_read(bare_kit_context_t *context) {
  char *data;
  size_t len;
  int err = bare_ipc_read(&context->ipc, (void **) &data, &len);
  assert(err == 0 || err == bare_ipc_would_block);

  received += max(len, 0);

  for (int i = 0; i < max(len, 0); i++) {
    assert(((uint8_t *) data)[i] == (received + i) % 256);
  }

  if (received == BUFFER_LEN) {
    context->readable = NULL;
    bare_kit_update(context);
    uv_async_send(&finished);
  }
}

void
on_write(bare_kit_context_t *context) {
  context->readable = on_read;
  bare_kit_update(context);
}

int
main() {
  int err;

  uv_loop_t *loop = uv_default_loop();

  err = uv_async_init(loop, &finished, on_exit);
  assert(err == 0);

  bare_kit_context_t context;
  context.readable = NULL;
  context.writable = NULL;
  context.pending_read = NULL;
  context.pending_write = NULL;
  context.data = NULL;
  context.len = 0;
  finished.data = (void *) &context;

  err = bare_worklet_init(&context.worklet, NULL);
  assert(err == 0);

  char *code = "BareKit.IPC.on('data', (data) => BareKit.IPC.write(data))";
  uv_buf_t source = uv_buf_init(code, strlen(code));
  err = bare_worklet_start(&context.worklet, "app.js", &source, NULL, NULL, 0, NULL);
  assert(err == 0);

  err = bare_ipc_init(&context.ipc, &context.worklet);
  assert(err == 0);

  err = bare_ipc_poll_init(&context.poll, &context.ipc);
  assert(err == 0);

  bare_ipc_poll_set_data(&context.poll, (void *) &context);

  for (int i = 0; i < BUFFER_LEN; i++) {
    buffer[i] = i % 256;
  }

  bare_kit_write(&context, (char *) buffer, BUFFER_LEN, on_write);

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
