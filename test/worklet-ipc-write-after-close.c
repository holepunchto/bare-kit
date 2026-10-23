#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <uv.h>

#include <bare-kit.h>

// A pending worklet write must error, not hang, once the host closes its ipc.

static bool replied = false;
static char reply_buf[64];

static void
on_push(const char *error, const void *reply, size_t len, void *data) {
  assert(error == NULL);

  memcpy(reply_buf, reply, len);
  reply_buf[len] = '\0';

  replied = true;
}

int
main() {
  int e;

  bare_worklet_t *worklet;
  e = bare_worklet_create(NULL, &worklet);
  assert(e == 0);

  char *code =
    "let errored = false\n"
    "Bare.IPC.on('error', () => { errored = true })\n"
    "setInterval(() => Bare.IPC.write(Buffer.alloc(64)), 5)\n"
    "BareKit.on('push', (data, reply) => reply(null, errored ? 'errored' : 'wedged'))\n";

  e = bare_worklet_start(worklet, "app.js", code, strlen(code), 0, NULL);
  assert(e == 0);

  bare_ipc_t *ipc;
  e = bare_ipc_create(worklet, &ipc);
  assert(e == 0);

  // Never read, so the queue fills and the worklet is left with a pending write.
  uv_sleep(100);

  bare_ipc_destroy(ipc);

  // Give the worklet time to retry that write against the closed peer.
  uv_sleep(300);

  e = bare_worklet_push(worklet, "status", 6, on_push, NULL);
  assert(e == 0);

  for (int i = 0; i < 200 && !replied; i++)
    uv_sleep(10);

  assert(replied);
  assert(strcmp(reply_buf, "errored") == 0);

  bare_worklet_destroy(worklet);

  e = bare_worklet_teardown();
  assert(e == 0);
}
