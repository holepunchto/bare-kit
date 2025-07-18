#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ipc.h"

int
bare_ipc_alloc(bare_ipc_t **result) {
  bare_ipc_t *ipc = malloc(sizeof(bare_ipc_t));

  if (ipc == NULL) return -1;

  *result = ipc;

  return 0;
}

int
bare_ipc_init(bare_ipc_t *ipc, bare_worklet_t *worklet) {
  ipc->incoming = dup(worklet->incoming);
  ipc->outgoing = dup(worklet->outgoing);

  return 0;
}

void
bare_ipc_destroy(bare_ipc_t *ipc) {
  close(ipc->incoming);
  close(ipc->outgoing);
}

int
bare_ipc_get_incoming(bare_ipc_t *ipc) {
  return ipc->incoming;
}

int
bare_ipc_get_outgoing(bare_ipc_t *ipc) {
  return ipc->outgoing;
}

int
bare_ipc_read(bare_ipc_t *ipc, void **data, size_t *len) {
  ssize_t res = read(ipc->incoming, ipc->data, BARE_IPC_READ_BUFFER_SIZE);

  if (res < 0) {
    return (errno == EAGAIN || errno == EWOULDBLOCK) ? bare_ipc_would_block : bare_ipc_error;
  }

  *len = res;
  *data = ipc->data;

  return 0;
}

int
bare_ipc_write(bare_ipc_t *ipc, const void *data, size_t len) {
  ssize_t res = write(ipc->outgoing, data, len);

  if (res < 0) {
    return (errno == EAGAIN || errno == EWOULDBLOCK) ? bare_ipc_would_block : bare_ipc_error;
  }

  return res;
}
