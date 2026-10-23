#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>
#include <uv.h>

#include <bare-kit.h>

// Pushes still pending when the worklet stops are each settled with an error,
// rather than lost.

static atomic_int replies = 0;
static atomic_int errors = 0;

static void
on_push(const char *error, const void *reply, size_t len, void *data) {
  if (error) errors++;
  else replies++;
}

int
main() {
  int e;

  bare_worklet_t *worklet;
  e = bare_worklet_create(NULL, &worklet);
  assert(e == 0);

  char *code = "BareKit.on('push', (data, reply) => setTimeout(() => reply(null, 'late'), 500))";

  e = bare_worklet_start(worklet, "app.js", code, strlen(code), 0, NULL);
  assert(e == 0);

  for (int i = 0; i < 10; i++) {
    e = bare_worklet_push(worklet, "ping", 4, on_push, NULL);
    assert(e == 0);
  }

  uv_sleep(30);

  bare_worklet_destroy(worklet);

  for (int i = 0; i < 100 && errors < 10; i++)
    uv_sleep(10);

  assert(replies == 0);
  assert(errors == 10);
}
