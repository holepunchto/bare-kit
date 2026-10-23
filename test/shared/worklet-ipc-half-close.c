#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <uv.h>

#include "../../shared/ipc.h"
#include "../../shared/worklet.h"

// Ending Bare.IPC from the worklet is a half-close: the host reads EOF but may
// keep writing.

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
    "let received = ''\n"
    "Bare.IPC.on('data', (data) => { received += data })\n"
    "Bare.IPC.end()\n"
    "BareKit.on('push', (data, reply) => reply(null, received))\n";

  uv_buf_t source = uv_buf_init(code, strlen(code));

  e = bare_worklet_start(&worklet, "app.js", &source, 0, NULL);
  assert(e == 0);

  bare_ipc_t ipc;
  e = bare_ipc_init(&ipc, &worklet);
  assert(e == 0);

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

  e = bare_ipc_write(&ipc, "hello", 5);
  assert(e == 5);

  uv_sleep(100);

  uv_buf_t payload = uv_buf_init("status", 6);

  bare_worklet_push_t req;
  e = bare_worklet_push(&worklet, &req, &payload, on_push);
  assert(e == 0);

  for (int i = 0; i < 200 && !replied; i++)
    uv_sleep(10);

  assert(replied);
  assert(strcmp(reply_buf, "hello") == 0);

  bare_ipc_destroy(&ipc);

  bare_worklet_destroy(&worklet);
}
