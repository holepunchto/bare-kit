#include <assert.h>
#include <string.h>
#include <uv.h>

#include <bare-kit.h>

// Ending the host end is a half-close: Bare.IPC reads EOF while the host keeps
// reading, and the worklet exits once it ends its own end in turn.

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
    "const chunks = []\n"
    "Bare.IPC\n"
    "  .on('data', (data) => chunks.push(data))\n"
    "  .on('end', () => Bare.IPC.end(Buffer.concat(chunks)))\n";

  e = bare_worklet_start(worklet, "app.js", code, strlen(code), 0, NULL);
  assert(e == 0);

  bare_ipc_t *ipc;
  e = bare_ipc_create(worklet, &ipc);
  assert(e == 0);

  e = bare_ipc_write(ipc, "hello", 5);
  assert(e == 5);

  bare_ipc_end(ipc);
  bare_ipc_end(ipc);

  e = bare_ipc_write(ipc, "world", 5);
  assert(e == bare_ipc_error);

  char data[BARE_IPC_READ_BUFFER_SIZE];
  size_t received = 0;
  int len;

  while ((len = bare_ipc_read(ipc, data + received, sizeof(data) - received)) != 0) {
    if (len == bare_ipc_would_block) uv_sleep(10);
    else received += len;
  }

  assert(received == 5);
  assert(memcmp(data, "hello", 5) == 0);

  uv_sem_wait(&exited);

  assert(exit_code == 0);

  bare_ipc_destroy(ipc);

  bare_worklet_destroy(worklet);

  e = bare_worklet_teardown();
  assert(e == 0);
}
