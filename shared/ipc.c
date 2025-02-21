#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "ipc.h"

int
bare_ipc_init(bare_ipc_t *ipc, int incoming, int outgoing) {
  ipc->incoming = incoming;
  ipc->outgoing = outgoing;

  return 0;
}

void
bare_ipc_destroy(bare_ipc_t *ipc) {
  close(ipc->incoming);
  close(ipc->outgoing);
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

  return 0;
}
