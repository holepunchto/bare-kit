#include "ipc.h"

int
bare_ipc_alloc(bare_ipc_t **result) {
  bare_ipc_t *ipc = malloc(sizeof(bare_ipc_t));

  if (ipc == NULL) return -1;

  *result = ipc;

  return 0;
}

int
bare_ipc_get_incoming(bare_ipc_t *ipc) {
  return ipc->incoming;
}

int
bare_ipc_get_outgoing(bare_ipc_t *ipc) {
  return ipc->outgoing;
}
