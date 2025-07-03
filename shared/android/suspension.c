#include "../suspension.h"

int
bare_suspension_start(int linger, bare_suspension_t **result) {
  return linger < 0 ? 0 : linger;
}

int
bare_suspension_end(bare_suspension_t *suspension) {
  return 0;
}
