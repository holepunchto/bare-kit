#include <assert.h>
#include <string.h>
#include <uv.h>

#include <bare-kit.h>

// A pending worklet write must error, not hang, once the host closes its ipc.

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

  char *code =
    "Bare.IPC.on('error', () => Bare.exit(0))\n"
    "setInterval(() => Bare.IPC.write(Buffer.alloc(64)), 5)\n";

  e = bare_worklet_start(worklet, "app.js", code, strlen(code), 0, NULL);
  assert(e == 0);

  bare_ipc_t *ipc;
  e = bare_ipc_create(worklet, &ipc);
  assert(e == 0);

  // Never read, so the queue fills and the worklet is left with a pending write.
  uv_sleep(100);

  bare_ipc_destroy(ipc);

  uv_sem_wait(&exited);

  assert(exit_code == 0);

  bare_worklet_destroy(worklet);

  e = bare_worklet_teardown();
  assert(e == 0);
}
