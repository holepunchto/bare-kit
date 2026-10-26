#include <assert.h>

#include <bare-kit.h>

int
main() {
  int e;

  bare_worklet_options_t options = {
    .memory_limit = 0,
    .assets = "test/assets/assets.bundle",
  };

  bare_worklet_t *worklet;
  e = bare_worklet_create(&options, &worklet);
  assert(e == 0);

  e = bare_worklet_start(worklet, "test/fixtures/assets.bundle", NULL, 0, 0, NULL);
  assert(e == 0);

  bare_worklet_destroy(worklet);

  e = bare_worklet_teardown();
  assert(e == 0);
}
