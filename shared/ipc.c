#include <assert.h>
#include <errno.h>
#include <zmq.h>

#include "ipc.h"

int
bare_ipc_init(bare_ipc_t *ipc, const char *endpoint) {
  int err;

  ipc->context = zmq_ctx_new();
  assert(ipc->context);

  ipc->socket = zmq_socket(ipc->context, ZMQ_PAIR);
  assert(ipc->socket);

  err = zmq_connect(ipc->socket, endpoint);
  assert(err == 0);

  return 0;
}

void
bare_ipc_destroy(bare_ipc_t *ipc) {
  int err;

  err = zmq_close(ipc->socket);
  assert(err == 0);

  err = zmq_ctx_term(ipc->context);
  assert(err == 0);
}

int
bare_ipc_fd(bare_ipc_t *ipc) {
  int err;

  int fd;
  size_t fd_len = sizeof(fd);
  err = zmq_getsockopt(ipc->socket, ZMQ_FD, &fd, &fd_len);
  assert(err == 0);

  return fd;
}

int
bare_ipc_read(bare_ipc_t *ipc, bare_ipc_msg_t *msg, void **data, size_t *len) {
  int err;

  err = zmq_msg_init((zmq_msg_t *) msg);
  assert(err == 0);

  err = zmq_msg_recv((zmq_msg_t *) msg, ipc->socket, ZMQ_DONTWAIT);

  if (err < 0) {
    return zmq_errno() == EAGAIN ? bare_ipc_would_block : bare_ipc_error;
  }

  *data = zmq_msg_data((zmq_msg_t *) msg);
  *len = zmq_msg_size((zmq_msg_t *) msg);

  return 0;
}

int
bare_ipc_write(bare_ipc_t *ipc, bare_ipc_msg_t *msg, const void *data, size_t len) {
  int err;

  err = zmq_msg_init_buffer((zmq_msg_t *) msg, data, len);
  assert(err == 0);

  err = zmq_msg_send((zmq_msg_t *) msg, ipc->socket, ZMQ_DONTWAIT);

  if (err < 0) {
    return zmq_errno() == EAGAIN ? bare_ipc_would_block : bare_ipc_error;
  }

  return 0;
}

void
bare_ipc_release(bare_ipc_msg_t *msg) {
  int err;

  err = zmq_msg_close((zmq_msg_t *) msg);
  assert(err == 0);
}
