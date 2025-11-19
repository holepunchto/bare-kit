#include "../../shared/worklet.h"

int
main() {
  int e;

  bare_worklet_options_t options = {
    .memory_limit = 0,
    .assets = "test/shared/assets/assets.bundle",
  };

  bare_worklet_t worklet;
  e = bare_worklet_init(&worklet, &options);
  assert(e == 0);

  e = bare_worklet_start(&worklet, "test/shared/fixtures/assets.bundle", NULL, NULL, NULL);
  assert(e == 0);

  e = bare_worklet_terminate(&worklet);
  assert(e == 0);

  bare_worklet_destroy(&worklet);
}
