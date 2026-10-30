#include <assert.h>
#include <string.h>
#include <uv.h>

#include <bare-kit.h>

// A native addon loaded by the worklet resolves the symbols of Bare from the
// host, which here either links Bare Kit statically and exports its symbols or
// links the shared library.

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

  char *code = "Bare.IPC.end(new Bare.Addon(new URL(Bare.argv[0])).exports)";

  const char *argv[] = {ADDON};

  e = bare_worklet_start(worklet, "app.js", code, strlen(code), 1, argv);
  assert(e == 0);

  char data[BARE_IPC_READ_BUFFER_SIZE];
  size_t received = 0;
  int len;

  while ((len = bare_ipc_read(ipc, data + received, sizeof(data) - received)) != 0) {
    if (len == bare_ipc_would_block) uv_sleep(10);
    else received += len;
  }

  assert(received == strlen("Hello from addon"));
  assert(memcmp(data, "Hello from addon", received) == 0);

  bare_ipc_end(ipc);

  uv_sem_wait(&exited);

  assert(exit_code == 0);

  bare_ipc_destroy(ipc);

  bare_worklet_destroy(worklet);

  e = bare_worklet_teardown();
  assert(e == 0);
}
