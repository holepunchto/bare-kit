#include <stdlib.h>

#include "ipc.h"

// Runs on the worklet thread when the queue changes.
static void
bare_ipc__on_signal(bare_queue_port_t *port) {
  bare_ipc_t *ipc = (bare_ipc_t *) port->data;

  if (ipc == NULL) return;

  bare_ipc__signal(ipc);
}

int
bare_ipc_alloc(bare_ipc_t **result) {
  bare_ipc_t *ipc = malloc(sizeof(bare_ipc_t));

  if (ipc == NULL) return -1;

  *result = ipc;

  return 0;
}

int
bare_ipc_init(bare_ipc_t *ipc, bare_worklet_t *worklet) {
  ipc->poll = NULL;
  ipc->signal = NULL;

  int err = bare_ipc__signal_init(ipc);

  if (err < 0) return err;

  ipc->port = bare_queue_open_thread(worklet->queue, bare_ipc__on_signal);
  ipc->port->data = ipc;

  return 0;
}

void
bare_ipc_destroy(bare_ipc_t *ipc) {
  bare_queue_t *queue = ipc->port->queue;

  // Order matters: close marks the end shut, retiring clears the callback under
  // the queue's lock, and only then is nothing left that can reach the signal.
  bare_queue_close(ipc->port);

  bare_queue_shutdown_thread(queue);

  bare_ipc__signal_destroy(ipc);
}

int
bare_ipc__ready(bare_ipc_t *ipc, int events) {
  int ready = 0;

  if ((events & bare_ipc_readable) != 0 && bare_queue_readable(ipc->port)) ready |= bare_ipc_readable;
  if ((events & bare_ipc_writable) != 0 && bare_queue_writable(ipc->port)) ready |= bare_ipc_writable;

  return ready;
}

int
bare_ipc_read(bare_ipc_t *ipc, void **data, size_t *len) {
  int status = bare_queue_read(ipc->port, data, len);

  if (status == bare_queue_would_block) return bare_ipc_would_block;

  return 0;
}

int
bare_ipc_write(bare_ipc_t *ipc, const void *data, size_t len) {
  int n = bare_queue_write(ipc->port, data, len);

  if (n == bare_queue_would_block) return bare_ipc_would_block;
  if (n == bare_queue_closed) return bare_ipc_error;

  return n;
}
