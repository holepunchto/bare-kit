#include <assert.h>
#include <string.h>
#include <uv.h>

#include <bare-kit.h>

// A worklet whose ipc has ended in both directions exits on its own, without
// being terminated or destroyed.

static uv_sem_t exited;
static int exit_code;

static void
on_worklet_exit(int code, void *data) {
  exit_code = code;

  uv_sem_post(&exited);
}

int
main() {
  int e;

  e = uv_sem_init(&exited, 0);
  assert(e == 0);

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

  while ((len = bare_ipc_read(ipc, data, sizeof(data))) != 0) {
    assert(len > 0 || len == bare_ipc_would_block);

    uv_sleep(10);
  }

  bare_ipc_close(ipc);

  uv_sem_wait(&exited);

  assert(exit_code == 0);

  bare_ipc_destroy(ipc);

  bare_worklet_destroy(worklet);

  e = bare_worklet_teardown();
  assert(e == 0);
}
