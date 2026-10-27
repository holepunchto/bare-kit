#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "ipc.h"

static void
bare_ipc__on_signal(bare_queue_port_t *port) {
  bare_ipc__signal((bare_ipc_t *) port->data);
}

int
bare_ipc_create(bare_worklet_t *worklet, bare_ipc_t **result) {
  bare_ipc_t *ipc = malloc(sizeof(bare_ipc_t));

  if (ipc == NULL) return -1;

  ipc->pending.base = NULL;
  ipc->offset = 0;
  ipc->poll = NULL;
  ipc->signal = NULL;

  int err = bare_ipc__signal_init(ipc);

  if (err < 0) {
    free(ipc);

    return err;
  }

  ipc->port = bare_queue_open_thread(worklet->queue, bare_ipc__on_signal, ipc);

  if (ipc->port == NULL) {
    bare_ipc__signal_destroy(ipc);

    free(ipc);

    return -1;
  }

  err = uv_mutex_init(&ipc->lock);
  assert(err == 0);

  *result = ipc;

  return 0;
}

void
bare_ipc_close(bare_ipc_t *ipc) {
  uv_mutex_lock(&ipc->lock);

  if (ipc->port != NULL) {
    bare_queue_close(ipc->port);

    ipc->port = NULL;

    free(ipc->pending.base);
    ipc->pending.base = NULL;

    // Let a poll see the end of the stream.
    bare_ipc__signal(ipc);
  }

  uv_mutex_unlock(&ipc->lock);
}

void
bare_ipc_end(bare_ipc_t *ipc) {
  uv_mutex_lock(&ipc->lock);

  if (ipc->port != NULL) {
    bare_queue_end(ipc->port);

    // Let a poll see that writes now fail.
    bare_ipc__signal(ipc);
  }

  uv_mutex_unlock(&ipc->lock);
}

void
bare_ipc_destroy(bare_ipc_t *ipc) {
  bare_ipc_close(ipc);

  bare_ipc__signal_destroy(ipc);

  uv_mutex_destroy(&ipc->lock);

  free(ipc);
}

void
bare_ipc__dispatch(bare_ipc_t *ipc) {
  bare_ipc_poll_t *poll = ipc->poll;

  if (poll == NULL) return;

  bare_ipc_poll_cb cb = atomic_load(&poll->cb);

  if (cb == NULL) return;

  // A closed ipc reads EOF and fails writes, so it is ready for both.
  int ready = atomic_load(&poll->events);

  uv_mutex_lock(&ipc->lock);

  if (ipc->port != NULL) {
    if (ipc->pending.base == NULL && !bare_queue_readable(ipc->port)) ready &= ~bare_ipc_readable;
    if (!bare_queue_writable(ipc->port)) ready &= ~bare_ipc_writable;
  }

  uv_mutex_unlock(&ipc->lock);

  if (ready != 0) cb(poll, ready);
}

int
bare_ipc_read(bare_ipc_t *ipc, void *data, size_t len) {
  assert(len > 0);

  if (len > INT_MAX) len = INT_MAX;

  uv_mutex_lock(&ipc->lock);

  int result = 0;

  bare_queue_message_t *pending = &ipc->pending;

  if (ipc->port != NULL && pending->base == NULL) {
    ipc->offset = 0;

    if (bare_queue_read(ipc->port, &pending->base, &pending->len) == bare_queue_would_block) result = bare_ipc_would_block;
  }

  if (pending->base != NULL) {
    size_t remaining = pending->len - ipc->offset;

    if (len > remaining) len = remaining;

    memcpy(data, (char *) pending->base + ipc->offset, len);

    ipc->offset += len;

    if (ipc->offset == pending->len) {
      free(pending->base);
      pending->base = NULL;
    }

    result = (int) len;

    // Wake again while data remains, so a poll may read once per wake.
    if (pending->base != NULL || bare_queue_readable(ipc->port)) bare_ipc__signal(ipc);
  }

  uv_mutex_unlock(&ipc->lock);

  return result;
}

int
bare_ipc_write(bare_ipc_t *ipc, const void *data, size_t len) {
  uv_mutex_lock(&ipc->lock);

  int n = ipc->port == NULL ? bare_queue_closed : bare_queue_write(ipc->port, data, len);

  uv_mutex_unlock(&ipc->lock);

  if (n == bare_queue_would_block) return bare_ipc_would_block;
  if (n == bare_queue_closed) return bare_ipc_error;

  return n;
}

int
bare_ipc_poll_create(bare_ipc_t *ipc, bare_ipc_poll_t **result) {
  bare_ipc_poll_t *poll = malloc(sizeof(bare_ipc_poll_t));

  if (poll == NULL) return -1;

  poll->ipc = ipc;
  poll->data = NULL;

  atomic_init(&poll->events, 0);
  atomic_init(&poll->cb, NULL);

  bare_ipc__set_poll(ipc, poll);

  *result = poll;

  return 0;
}

void
bare_ipc_poll_destroy(bare_ipc_poll_t *poll) {
  bare_ipc_poll_stop(poll);

  bare_ipc__set_poll(poll->ipc, NULL);

  free(poll);
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

  atomic_store(&poll->events, events);
  atomic_store(&poll->cb, cb);

  // Level-triggered: signal once in case the events are already ready.
  bare_ipc__signal(poll->ipc);

  return 0;
}

int
bare_ipc_poll_stop(bare_ipc_poll_t *poll) {
  atomic_store(&poll->events, 0);
  atomic_store(&poll->cb, NULL);

  return 0;
}
