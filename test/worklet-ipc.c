#include <assert.h>
#include <string.h>
#include <uv.h>

#include <bare-kit.h>

// Polls for both events to echo a write much larger than the queue through the
// worklet.

#define LEN (4 * 1024 * 1024)

static char message[LEN];

static size_t written = 0;
static size_t received = 0;

static uv_async_t finished;

static void
on_poll(bare_ipc_poll_t *poll, int events) {
  int e;

  bare_ipc_t *ipc = bare_ipc_poll_get_ipc(poll);

  while (written < LEN) {
    int len = bare_ipc_write(ipc, &message[written], LEN - written);
    if (len == bare_ipc_would_block) break;
    assert(len > 0);

    written += len;
  }

  while (received < LEN) {
    char data[BARE_IPC_READ_BUFFER_SIZE];

    int len = bare_ipc_read(ipc, data, sizeof(data));
    if (len == bare_ipc_would_block) break;
    assert(len > 0);

    assert(memcmp(data, &message[received], len) == 0);

    received += len;
  }

  if (received == LEN) {
    e = bare_ipc_poll_stop(poll);
    assert(e == 0);

    uv_async_send(&finished);
  } else {
    e = bare_ipc_poll_start(poll, written < LEN ? bare_ipc_readable | bare_ipc_writable : bare_ipc_readable, on_poll);
    assert(e == 0);
  }
}

static void
on_finished(uv_async_t *handle) {
  uv_close((uv_handle_t *) handle, NULL);
}

int
main() {
  int e;

  for (size_t i = 0; i < LEN; i++)
    message[i] = (char) i;

  uv_loop_t *loop = uv_default_loop();

  e = uv_async_init(loop, &finished, on_finished);
  assert(e == 0);

  bare_worklet_t *worklet;
  e = bare_worklet_create(NULL, &worklet);
  assert(e == 0);

  char *code = "Bare.IPC.on('data', (data) => Bare.IPC.write(data))";

  e = bare_worklet_start(worklet, "app.js", code, strlen(code), 0, NULL);
  assert(e == 0);

  bare_ipc_t *ipc;
  e = bare_ipc_create(worklet, &ipc);
  assert(e == 0);

  bare_ipc_poll_t *poll;
  e = bare_ipc_poll_create(ipc, &poll);
  assert(e == 0);

  e = bare_ipc_poll_start(poll, bare_ipc_readable | bare_ipc_writable, on_poll);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);

  bare_ipc_poll_destroy(poll);

  bare_ipc_destroy(ipc);

  bare_worklet_destroy(worklet);

  e = bare_worklet_teardown();
  assert(e == 0);

  e = uv_loop_close(loop);
  assert(e == 0);
}
