#include "../suspension.h"

int
bare_suspension_init(bare_suspension_t *suspension) {
  return 0;
}

int
bare_suspension_start(bare_suspension_t *result, int linger) {
  return linger < 0 ? 30000 : linger;
}

int
bare_suspension_end(bare_suspension_t *suspension) {
  return 0;
}
