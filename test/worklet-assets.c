#include "../shared/worklet.h"

int
main () {
  int e;

  bare_worklet_options_t options = {
    .memory_limit = 0,
    .assets = "test/assets/assets.bundle",
  };

  bare_worklet_t worklet;
  e = bare_worklet_init(&worklet, &options);
  assert(e == 0);

  e = bare_worklet_start(&worklet, "test/fixtures/assets.bundle", NULL, 0, NULL);
  assert(e == 0);
}
