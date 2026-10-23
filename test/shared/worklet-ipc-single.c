#include <assert.h>
#include <string.h>
#include <uv.h>

#include "../../shared/ipc.h"
#include "../../shared/worklet.h"

// A second ipc on the same worklet is refused.

int
main() {
  int e;

  bare_worklet_t worklet;
  e = bare_worklet_init(&worklet, NULL);
  assert(e == 0);

  char *code = "Bare.IPC.on('data', () => {})";

  uv_buf_t source = uv_buf_init(code, strlen(code));

  e = bare_worklet_start(&worklet, "app.js", &source, 0, NULL);
  assert(e == 0);

  bare_ipc_t ipc;
  e = bare_ipc_init(&ipc, &worklet);
  assert(e == 0);

  bare_ipc_t other;
  e = bare_ipc_init(&other, &worklet);
  assert(e < 0);

  bare_ipc_destroy(&ipc);

  bare_worklet_destroy(&worklet);
}
