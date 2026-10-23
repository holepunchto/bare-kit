#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <uv.h>

#include "../../shared/ipc.h"
#include "../../shared/worklet.h"

// An ipc outlives its worklet: once the worklet is destroyed, it reads EOF and
// fails writes without touching the worklet again.

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

  bare_worklet_destroy(&worklet);

  memset(&worklet, 0, sizeof(worklet));

  char data[BARE_IPC_READ_BUFFER_SIZE];
  int len;

  bool eof = false;
  for (int i = 0; i < 200 && !eof; i++) {
    len = bare_ipc_read(&ipc, data, sizeof(data));
    assert(len >= 0 || len == bare_ipc_would_block);
    if (len == 0) eof = true;
    else uv_sleep(10);
  }

  assert(eof);

  e = bare_ipc_write(&ipc, "late", 4);
  assert(e == bare_ipc_error);

  bare_ipc_destroy(&ipc);
}
