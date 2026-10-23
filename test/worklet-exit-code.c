#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>
#include <uv.h>

#include <bare-kit.h>

// A worklet that exits on its own reports its exit code without being
// terminated or destroyed, by which time its ipc has reached EOF.

static atomic_int exit_code = -1;

static void
on_worklet_exit(int code, void *data) {
  bare_ipc_t *ipc = (bare_ipc_t *) data;

  char buffer[BARE_IPC_READ_BUFFER_SIZE];

  assert(bare_ipc_read(ipc, buffer, sizeof(buffer)) == 0);

  atomic_store(&exit_code, code);
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

  e = bare_worklet_on_exit(worklet, on_worklet_exit, ipc);
  assert(e == 0);

  char *code = "setTimeout(() => Bare.exit(42), 50)";

  e = bare_worklet_start(worklet, "app.js", code, strlen(code), 0, NULL);
  assert(e == 0);

  for (int i = 0; i < 200 && atomic_load(&exit_code) == -1; i++)
    uv_sleep(10);

  assert(atomic_load(&exit_code) == 42);

  // Once it has torn down, the worklet ignores the host.
  e = bare_worklet_suspend(worklet, 0);
  assert(e == 0);

  e = bare_worklet_resume(worklet);
  assert(e == 0);

  e = bare_worklet_wakeup(worklet, 0);
  assert(e == 0);

  e = bare_worklet_terminate(worklet);
  assert(e == 0);

  bare_ipc_destroy(ipc);

  bare_worklet_destroy(worklet);
}
