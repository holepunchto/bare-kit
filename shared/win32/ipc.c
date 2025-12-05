#include <io.h>
#include <uv.h>

#include "ipc.h"

void
bare_ipc__close(uv_async_t *handle) {
  bare_ipc_t *ipc = (bare_ipc_t *) uv_handle_get_data((uv_handle_t *) handle);

  uv_close((uv_handle_t *) &ipc->pipe.incoming, NULL);
  uv_close((uv_handle_t *) &ipc->pipe.outgoing, NULL);
  uv_close((uv_handle_t *) &ipc->read, NULL);
  uv_close((uv_handle_t *) &ipc->write, NULL);
  uv_close((uv_handle_t *) &ipc->close, NULL);
}

void
bare_ipc__on_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
  buf->base = malloc(BARE_IPC_READ_BUFFER_SIZE);
  buf->len = BARE_IPC_READ_BUFFER_SIZE;
}

void
bare_ipc__on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
  assert(nread >= 0);

  if (nread == 0) {
    free(buf->base);
    return;
  }

  int err;

  err = uv_read_stop(stream);
  assert(err == 0);

  bare_ipc_t *ipc = (bare_ipc_t *) uv_handle_get_data((uv_handle_t *) stream);

  uv_mutex_lock(&ipc->reading);

  memcpy(ipc->read_buffer.base, buf->base, nread);
  ipc->read_buffer.len = nread;

  uv_mutex_unlock(&ipc->reading);

  free(buf->base);

  if (ipc->poll && ipc->poll->cb) ipc->poll->cb(ipc->poll, bare_ipc_readable);
}

void
bare_ipc__read(uv_async_t *handle) {
  bare_ipc_t *ipc = (bare_ipc_t *) uv_handle_get_data((uv_handle_t *) handle);

  int err = uv_read_start((uv_stream_t *) &ipc->pipe.incoming, bare_ipc__on_alloc, bare_ipc__on_read);
  assert(err == 0);
}

void
bare_ipc__on_write(uv_write_t *req, int status) {
  assert(status == 0);

  bare_ipc_t *ipc = (bare_ipc_t *) uv_handle_get_data((uv_handle_t *) req);

  free(ipc->write_buffer.base);
  free(req);

  uv_mutex_unlock(&ipc->writing);

  if (ipc->poll && ipc->poll->cb && (ipc->poll->events & bare_ipc_writable) != 0) ipc->poll->cb(ipc->poll, bare_ipc_writable);
}

void
bare_ipc__write(uv_async_t *handle) {
  int err;

  bare_ipc_t *ipc = (bare_ipc_t *) uv_handle_get_data((uv_handle_t *) handle);

  uv_write_t *req = malloc(sizeof(uv_write_t));
  uv_handle_set_data((uv_handle_t *) req, (void *) ipc);

  err = uv_write(req, (uv_stream_t *) &ipc->pipe.outgoing, &ipc->write_buffer, 1, bare_ipc__on_write);
  assert(err == 0);
}

void
bare_ipc__loop(void *arg) {
  bare_ipc_t *ipc = (bare_ipc_t *) arg;

  int err;

  err = uv_loop_init(&ipc->loop);
  assert(err == 0);

  err = uv_async_init(&ipc->loop, &ipc->read, bare_ipc__read);
  assert(err == 0);
  uv_handle_set_data((uv_handle_t *) &ipc->read, (void *) ipc);

  err = uv_async_init(&ipc->loop, &ipc->write, bare_ipc__write);
  assert(err == 0);
  uv_handle_set_data((uv_handle_t *) &ipc->write, (void *) ipc);

  err = uv_async_init(&ipc->loop, &ipc->close, bare_ipc__close);
  assert(err == 0);
  uv_handle_set_data((uv_handle_t *) &ipc->close, (void *) ipc);

  err = uv_pipe_init(&ipc->loop, &ipc->pipe.incoming, 0);
  assert(err == 0);
  err = uv_pipe_open(&ipc->pipe.incoming, ipc->incoming);
  assert(err == 0);
  uv_handle_set_data((uv_handle_t *) &ipc->pipe.incoming, (void *) ipc);

  err = uv_pipe_init(&ipc->loop, &ipc->pipe.outgoing, 0);
  assert(err == 0);
  err = uv_pipe_open(&ipc->pipe.outgoing, ipc->outgoing);
  assert(err == 0);
  uv_handle_set_data((uv_handle_t *) &ipc->pipe.outgoing, (void *) ipc);

  uv_barrier_wait(&ipc->ready);

  err = uv_run(&ipc->loop, UV_RUN_DEFAULT);
  assert(err == 0);

  err = uv_loop_close(&ipc->loop);
  assert(err == 0);
}

int
bare_ipc_init(bare_ipc_t *ipc, bare_worklet_t *worklet) {
  int err;

  ipc->incoming = _dup(worklet->incoming);
  ipc->outgoing = _dup(worklet->outgoing);

  err = uv_mutex_init(&ipc->reading);
  assert(err == 0);

  err = uv_mutex_init(&ipc->writing);
  assert(err == 0);

  err = uv_barrier_init(&ipc->ready, 2);
  assert(err == 0);

  err = uv_thread_create(&ipc->thread, bare_ipc__loop, ipc);
  assert(err == 0);

  ipc->read_buffer.len = 0;
  ipc->poll = NULL;

  uv_barrier_wait(&ipc->ready);
  uv_barrier_destroy(&ipc->ready);

  return 0;
}

void
bare_ipc_destroy(bare_ipc_t *ipc) {
  int err;

  err = uv_async_send(&ipc->close);
  assert(err == 0);

  err = uv_thread_join(&ipc->thread);
  assert(err == 0);

  uv_mutex_destroy(&ipc->reading);

  uv_mutex_destroy(&ipc->writing);
}

int
bare_ipc_read(bare_ipc_t *ipc, void **data, size_t *len) {
  int err;

  if (ipc->read_buffer.len > 0) {
    uv_mutex_lock(&ipc->reading);

    memcpy(data, ipc->read_buffer.base, ipc->read_buffer.len);
    *len = ipc->read_buffer.len;
    ipc->read_buffer.len = 0;

    uv_mutex_unlock(&ipc->reading);

    return 0;
  }

  err = uv_async_send(&ipc->read);
  assert(err == 0);

  return bare_ipc_would_block;
}

int
bare_ipc_write(bare_ipc_t *ipc, const void *data, size_t len) {
  int err;

  if (uv_mutex_trylock(&ipc->writing) != 0) {
    return bare_ipc_would_block;
  }

  ipc->write_buffer.base = malloc(len);
  memcpy(ipc->write_buffer.base, data, len);
  ipc->write_buffer.len = len;

  err = uv_async_send(&ipc->write);
  assert(err == 0);

  return len;
}

int
bare_ipc_poll_alloc(bare_ipc_poll_t **result) {
  bare_ipc_poll_t *poll = malloc(sizeof(bare_ipc_poll_t));

  if (poll == NULL) return -1;

  *result = poll;

  return 0;
}

int
bare_ipc_poll_init(bare_ipc_poll_t *poll, bare_ipc_t *ipc) {
  poll->ipc = ipc;
  poll->events = 0;
  poll->cb = NULL;
  ipc->poll = poll;

  return 0;
}

void
bare_ipc_poll_destroy(bare_ipc_poll_t *poll) {
  int err;

  err = bare_ipc_poll_stop(poll);
  assert(err == 0);
}

void *
bare_ipc_poll_get_data(bare_ipc_poll_t *poll) {
  return poll->data;
}

void
bare_ipc_poll_set_data(bare_ipc_poll_t *poll, void *data) {
  poll->data = data;
}

bare_ipc_t *
bare_ipc_poll_get_ipc(bare_ipc_poll_t *poll) {
  return poll->ipc;
}

int
bare_ipc_poll_start(bare_ipc_poll_t *poll, int events, bare_ipc_poll_cb cb) {
  if (events == 0) return bare_ipc_poll_stop(poll);

  poll->events = events;
  poll->cb = cb;

  return 0;
}

int
bare_ipc_poll_stop(bare_ipc_poll_t *poll) {
  poll->events = 0;
  poll->cb = NULL;

  return 0;
}
