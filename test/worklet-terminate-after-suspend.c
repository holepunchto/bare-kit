#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <uv.h>

#include <bare-kit.h>

int
main() {
  int e;

  bare_worklet_t *worklet;
  e = bare_worklet_create(NULL, &worklet);
  assert(e == 0);

  char *code = "console.log('Hello world')";

  e = bare_worklet_start(worklet, "app.js", code, strlen(code), 0, NULL);
  assert(e == 0);

  e = bare_worklet_suspend(worklet, 0);
  assert(e == 0);

  uv_sleep(100); // Let suspension flush

  e = bare_worklet_terminate(worklet);
  assert(e == 0);

  bare_worklet_destroy(worklet);

  e = bare_worklet_teardown();
  assert(e == 0);
}
