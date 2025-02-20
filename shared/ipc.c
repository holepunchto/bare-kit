#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "ipc.h"

int
bare_ipc_init(bare_ipc_t *ipc, int incoming, int outgoing) {
  ipc->incoming = incoming;
  ipc->outgoing = outgoing;

  return 0;
}

int
bare_ipc_read(bare_ipc_t *ipc, void **data, size_t *len) {
  size_t bytes_read = read(ipc->incoming, ipc->data, BARE_IPC_READ_BUFFER_SIZE);

  if (bytes_read < 0) {
    return (errno == EAGAIN || errno == EWOULDBLOCK) ? bare_ipc_would_block : bare_ipc_error;
  }

  *len = bytes_read;
  *data = (void *) ipc->data;

  return 0;
}

int
bare_ipc_write(bare_ipc_t *ipc, const void *data, size_t len) {
  ssize_t bytes_written = write(ipc->outgoing, data, len);

  if (bytes_written < 0) {
    return (errno == EAGAIN || errno == EWOULDBLOCK) ? bare_ipc_would_block : bare_ipc_error;
  }

  return 0;
}
