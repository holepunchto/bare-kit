#include <assert.h>
#include <stdatomic.h>
#include <string.h>
#include <uv.h>

#include "../../shared/worklet.h"

// Pushes still pending when the worklet stops are each settled with an error,
// rather than lost.

static atomic_int replies = 0;
static atomic_int errors = 0;

static void
on_push(bare_worklet_push_t *req, const char *error, const uv_buf_t *reply) {
  if (error) errors++;
  else replies++;
}

int
main() {
  int e;

  bare_worklet_t worklet;
  e = bare_worklet_init(&worklet, NULL);
  assert(e == 0);

  char *code = "BareKit.on('push', (data, reply) => setTimeout(() => reply(null, 'late'), 500))";

  uv_buf_t source = uv_buf_init(code, strlen(code));

  e = bare_worklet_start(&worklet, "app.js", &source, 0, NULL);
  assert(e == 0);

  uv_buf_t payload = uv_buf_init("ping", 4);

  bare_worklet_push_t reqs[10];

  for (int i = 0; i < 10; i++) {
    e = bare_worklet_push(&worklet, &reqs[i], &payload, on_push);
    assert(e == 0);
  }

  uv_sleep(30);

  bare_worklet_destroy(&worklet);

  for (int i = 0; i < 100 && errors < 10; i++) uv_sleep(10);

  assert(replies == 0);
  assert(errors == 10);
}
