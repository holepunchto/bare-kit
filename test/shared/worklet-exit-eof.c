#include <uv.h>

#include "../../shared/ipc.h"
#include "../../shared/worklet.h"

// When the worklet exits on its own (Bare.exit()), Bare.IPC is closed as part
// of process exit, so the host read reaches EOF (returns 0) without the host
// having to call terminate/destroy first.

int
main() {
  int e;

  bare_worklet_t worklet;
  e = bare_worklet_init(&worklet, NULL);
  assert(e == 0);

  char *code = "setTimeout(() => Bare.exit(0), 50)";

  uv_buf_t source = uv_buf_init(code, strlen(code));

  e = bare_worklet_start(&worklet, "app.js", &source, 0, NULL);
  assert(e == 0);

  bare_ipc_t ipc;
  e = bare_ipc_init(&ipc, &worklet);
  assert(e == 0);

  void *data;
  size_t len;

  bool eof = false;
  for (int i = 0; i < 200; i++) {
    e = bare_ipc_read(&ipc, &data, &len);
    assert(e == 0 || e == bare_ipc_would_block);
    if (e == 0 && len == 0) {
      eof = true;
      break;
    }
    uv_sleep(10);
  }

  assert(eof); // The self-exit closed Bare.IPC and the host saw EOF

  bare_ipc_destroy(&ipc);

  bare_worklet_destroy(&worklet);
}
