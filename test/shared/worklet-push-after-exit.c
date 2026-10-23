#include <assert.h>
#include <string.h>
#include <uv.h>

#include "../../shared/worklet.h"

// Once the worklet's loop has stopped, a push fails rather than blocking or
// waiting for a reply that never comes.

static void
on_push(bare_worklet_push_t *req, const char *error, const uv_buf_t *reply) {
  assert(false);
}

int
main() {
  int e;

  bare_worklet_t worklet;
  e = bare_worklet_init(&worklet, NULL);
  assert(e == 0);

  char *code = "Bare.exit(0)";

  uv_buf_t source = uv_buf_init(code, strlen(code));

  e = bare_worklet_start(&worklet, "app.js", &source, 0, NULL);
  assert(e == 0);

  uv_sleep(500);

  uv_buf_t payload = uv_buf_init("ping", 4);

  bare_worklet_push_t req;
  e = bare_worklet_push(&worklet, &req, &payload, on_push);
  assert(e < 0);

  bare_worklet_destroy(&worklet);
}
