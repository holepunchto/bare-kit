#include <assert.h>
#include <string.h>
#include <uv.h>

#include <bare-kit.h>

// A message larger than the read buffer arrives over several reads.

int
main() {
  int e;

  bare_worklet_t *worklet;
  e = bare_worklet_create(NULL, &worklet);
  assert(e == 0);

  char *code = "Bare.IPC.write('Hello world!')";

  e = bare_worklet_start(worklet, "app.js", code, strlen(code), 0, NULL);
  assert(e == 0);

  bare_ipc_t *ipc;
  e = bare_ipc_create(worklet, &ipc);
  assert(e == 0);

  char message[64];
  size_t received = 0;

  while (received < 12) {
    int len = bare_ipc_read(ipc, message + received, 5);

    if (len == bare_ipc_would_block) {
      uv_sleep(10);
    } else {
      assert(len > 0 && len <= 5);

      received += len;
    }
  }

  assert(memcmp(message, "Hello world!", 12) == 0);

  bare_ipc_destroy(ipc);

  bare_worklet_destroy(worklet);

  e = bare_worklet_teardown();
  assert(e == 0);
}
