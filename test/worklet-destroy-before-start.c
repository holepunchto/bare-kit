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

  bare_worklet_destroy(worklet);

  e = bare_worklet_teardown();
  assert(e == 0);
}
