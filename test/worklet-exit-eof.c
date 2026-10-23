#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <uv.h>

#include <bare-kit.h>

// When the worklet exits on its own, the host reads EOF without having to
// terminate or destroy it first.

int
main() {
  int e;

  bare_worklet_t *worklet;
  e = bare_worklet_create(NULL, &worklet);
  assert(e == 0);

  char *code = "setTimeout(() => Bare.exit(0), 50)";

  e = bare_worklet_start(worklet, "app.js", code, strlen(code), 0, NULL);
  assert(e == 0);

  bare_ipc_t *ipc;
  e = bare_ipc_create(worklet, &ipc);
  assert(e == 0);

  char data[BARE_IPC_READ_BUFFER_SIZE];
  int len;

  bool eof = false;
  for (int i = 0; i < 200; i++) {
    len = bare_ipc_read(ipc, data, sizeof(data));
    assert(len >= 0 || len == bare_ipc_would_block);
    if (len == 0) {
      eof = true;
      break;
    }
    uv_sleep(10);
  }

  assert(eof); // The self-exit closed Bare.IPC and the host saw EOF

  bare_ipc_destroy(ipc);

  bare_worklet_destroy(worklet);
}
