#include <uv.h>

#include "../../shared/ipc.h"
#include "../../shared/worklet.h"

// Corroborates worklet-exit-no-eof: the host EOF comes from the JS side. When
// the worklet ends its Bare.IPC (closing the b1 write end), the host read
// returns 0 (EOF) - the signal the bindings turn into (nil, nil).

int
main() {
  int e;

  bare_worklet_t worklet;
  e = bare_worklet_init(&worklet, NULL);
  assert(e == 0);

  char *code = "setTimeout(() => Bare.IPC.end(), 50)";

  uv_buf_t source = uv_buf_init(code, strlen(code));

  e = bare_worklet_start(&worklet, "app.js", &source, 0, NULL);
  assert(e == 0);

  bare_ipc_t ipc;
  e = bare_ipc_init(&ipc, &worklet);
  assert(e == 0);

  void *data;
  size_t len;

  // Poll for up to ~2s for EOF. Expect read to return 0 once b1 closes.
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

  assert(eof); // The JS-side end reached the host as EOF

  bare_ipc_destroy(&ipc);

  bare_worklet_destroy(&worklet);
}
