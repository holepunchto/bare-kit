#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <uv.h>

#include "../../shared/ipc.h"
#include "../../shared/worklet.h"

// A message larger than the read buffer arrives over several reads.

int
main() {
  int e;

  bare_worklet_t worklet;
  e = bare_worklet_init(&worklet, NULL);
  assert(e == 0);

  char *code = "Bare.IPC.write('Hello world!')";

  uv_buf_t source = uv_buf_init(code, strlen(code));

  e = bare_worklet_start(&worklet, "app.js", &source, 0, NULL);
  assert(e == 0);

  bare_ipc_t ipc;
  e = bare_ipc_init(&ipc, &worklet);
  assert(e == 0);

  char message[64];
  size_t received = 0;

  for (int i = 0; i < 200 && received < 12; i++) {
    char data[5];
    int len = bare_ipc_read(&ipc, data, sizeof(data));
    assert(len > 0 || len == bare_ipc_would_block);

    if (len == bare_ipc_would_block) {
      uv_sleep(10);
    } else {
      assert(len <= 5);

      memcpy(message + received, data, len);
      received += len;
    }
  }

  assert(received == 12);
  assert(memcmp(message, "Hello world!", 12) == 0);

  bare_ipc_destroy(&ipc);

  bare_worklet_destroy(&worklet);
}
