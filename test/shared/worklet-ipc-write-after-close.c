#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <uv.h>

#include "../../shared/ipc.h"
#include "../../shared/worklet.h"

// The host retires its IPC while the worklet still has a write outstanding. The
// worklet's queue end then reports closed, which must surface as a stream error
// rather than a write that never completes; the worklet reports which it saw.

static bool replied = false;
static char reply_buf[64];

static void
on_push(bare_worklet_push_t *req, const char *error, const uv_buf_t *reply) {
  assert(error == NULL);

  memcpy(reply_buf, reply->base, reply->len);
  reply_buf[reply->len] = '\0';

  replied = true;
}

int
main() {
  int e;

  bare_worklet_t worklet;
  e = bare_worklet_init(&worklet, NULL);
  assert(e == 0);

  char *code =
    "let errored = false\n"
    "Bare.IPC.on('error', () => { errored = true })\n"
    "setInterval(() => Bare.IPC.write(Buffer.alloc(64)), 5)\n"
    "BareKit.on('push', (data, reply) => reply(null, errored ? 'errored' : 'wedged'))\n";

  uv_buf_t source = uv_buf_init(code, strlen(code));

  e = bare_worklet_start(&worklet, "app.js", &source, 0, NULL);
  assert(e == 0);

  bare_ipc_t ipc;
  e = bare_ipc_init(&ipc, &worklet);
  assert(e == 0);

  // Never read, so the queue fills and the worklet is left with a pending write.
  uv_sleep(100);

  bare_ipc_destroy(&ipc);

  // Give the worklet time to retry that write against the closed peer.
  uv_sleep(100);

  uv_buf_t payload = uv_buf_init("status", 6);

  bare_worklet_push_t req;
  e = bare_worklet_push(&worklet, &req, &payload, on_push);
  assert(e == 0);

  for (int i = 0; i < 200 && !replied; i++)
    uv_sleep(10);

  assert(replied);
  assert(strcmp(reply_buf, "errored") == 0); // not "wedged": the write completed

  bare_worklet_destroy(&worklet);
}
