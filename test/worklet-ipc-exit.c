#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>
#include <uv.h>

#include <bare-kit.h>

// A worklet whose ipc has ended in both directions exits on its own, without
// being terminated or destroyed.

static atomic_int exit_code = -1;

static void
on_worklet_exit(int code, void *data) {
  atomic_store(&exit_code, code);
}

int
main() {
  int e;

  bare_worklet_t *worklet;
  e = bare_worklet_create(NULL, &worklet);
  assert(e == 0);

  e = bare_worklet_on_exit(worklet, on_worklet_exit, NULL);
  assert(e == 0);

  bare_ipc_t *ipc;
  e = bare_ipc_create(worklet, &ipc);
  assert(e == 0);

  char *code = "Bare.IPC.on('data', (data) => Bare.IPC.end(data))";

  e = bare_worklet_start(worklet, "app.js", code, strlen(code), 0, NULL);
  assert(e == 0);

  e = bare_ipc_write(ipc, "ping", 4);
  assert(e == 4);

  char data[BARE_IPC_READ_BUFFER_SIZE];
  int len;

  bool eof = false;
  for (int i = 0; i < 200 && !eof; i++) {
    len = bare_ipc_read(ipc, data, sizeof(data));
    assert(len >= 0 || len == bare_ipc_would_block);
    if (len == 0) eof = true;
    else uv_sleep(10);
  }

  assert(eof);

  bare_ipc_close(ipc);

  for (int i = 0; i < 200 && atomic_load(&exit_code) == -1; i++) {
    uv_sleep(10);
  }

  assert(atomic_load(&exit_code) == 0);

  bare_ipc_destroy(ipc);

  bare_worklet_destroy(worklet);

  e = bare_worklet_teardown();
  assert(e == 0);
}
