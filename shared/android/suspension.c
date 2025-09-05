#include "../suspension.h"

int
bare_suspension_init(bare_suspension_t *suspension) {
  return 0;
}

int
bare_suspension_start(bare_suspension_t *result, int timeout) {
  return timeout < 0 ? 30000 : timeout;
}

int
bare_suspension_end(bare_suspension_t *suspension) {
  return 0;
}
