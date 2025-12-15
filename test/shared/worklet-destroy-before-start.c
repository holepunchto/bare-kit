#include "../../shared/worklet.h"

int
main() {
  int e;

  bare_worklet_t worklet;
  e = bare_worklet_init(&worklet, NULL);
  assert(e == 0);

  bare_worklet_destroy(&worklet);
}
