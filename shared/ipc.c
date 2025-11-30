#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(BARE_KIT_WINDOWS)
#include <io.h>
#include <uv.h>
#else
#include <unistd.h>
#endif

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
bare_ipc__loop(void *arg) {
  bare_ipc_t *ipc = (bare_ipc_t *) arg;

  int err;

  err = uv_run(&ipc->loop, UV_RUN_DEFAULT);
  assert(err == 0);

  err = uv_loop_close(&ipc->loop);
  assert(err == 0);
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

  int err = uv_read_stop(stream);
  assert(err == 0);

  bare_ipc_t *ipc = (bare_ipc_t *) uv_handle_get_data((uv_handle_t *) stream);

  memcpy(ipc->read_buffer.base, buf->base, nread);
  ipc->read_buffer.len = nread;
  if (ipc->poll && ipc->poll->cb) ipc->poll->cb(ipc->poll, bare_ipc_readable);

  free(buf->base);
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
}

void
bare_ipc__write(uv_async_t *handle) {
  bare_ipc_t *ipc = (bare_ipc_t *) uv_handle_get_data((uv_handle_t *) handle);

  uv_write_t *req = malloc(sizeof(uv_write_t));
  uv_handle_set_data((uv_handle_t *) req, (void *) ipc);

  int err = uv_write(req, (uv_stream_t *) &ipc->pipe.outgoing, &ipc->write_buffer, 1, bare_ipc__on_write);
  assert(err == 0);
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
  ipc->incoming = dup(worklet->incoming);
  ipc->outgoing = dup(worklet->outgoing);

#if defined(BARE_KIT_WINDOWS)
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

  err = uv_thread_create(&ipc->thread, bare_ipc__loop, (void *) ipc);
  assert(err == 0);

  ipc->read_buffer.len = 0;
  ipc->poll = NULL;
#endif

  return 0;
}

void
bare_ipc_destroy(bare_ipc_t *ipc) {
#if defined(BARE_KIT_WINDOWS)
  int err;

  err = uv_async_send(&ipc->close);
  assert(err == 0);

  err = uv_thread_join(&ipc->thread);
  assert(err == 0);
#else
  close(ipc->incoming);
  close(ipc->outgoing);
#endif
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
#if defined(BARE_KIT_WINDOWS)
  if (ipc->read_buffer.len > 0) {
    memcpy(data, ipc->read_buffer.base, ipc->read_buffer.len);
    *len = ipc->read_buffer.len;
    ipc->read_buffer.len = 0;
    return 0;
  }

  int err = uv_async_send(&ipc->read);
  assert(err == 0);

  return bare_ipc_would_block;
#else
  ssize_t res = read(ipc->incoming, ipc->data, BARE_IPC_READ_BUFFER_SIZE);

  if (res < 0) {
    return (errno == EAGAIN || errno == EWOULDBLOCK) ? bare_ipc_would_block : bare_ipc_error;
  }

  *len = res;
  *data = ipc->data;

  return 0;
#endif
}

int
bare_ipc_write(bare_ipc_t *ipc, const void *data, size_t len) {
#if defined(BARE_KIT_WINDOWS)
  int err;

  uv_buf_t buf = uv_buf_init((char *) data, len);
  err = uv_try_write((uv_stream_t *) &ipc->pipe.outgoing, &buf, 1);

  if (err > 0) {
    return err;
  }

  ipc->write_buffer.base = malloc(len);
  memcpy(ipc->write_buffer.base, data, len);
  ipc->write_buffer.len = len;

  err = uv_async_send(&ipc->write);
  assert(err == 0);

  return len;
#else
  ssize_t res = write(ipc->outgoing, data, len);

  if (res < 0) {
    return (errno == EAGAIN || errno == EWOULDBLOCK) ? bare_ipc_would_block : bare_ipc_error;
  }

  return res;
#endif
}
