#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#include "queue.h"

#define BARE_QUEUE_MASK (BARE_QUEUE_CAPACITY - 1)

static bool
bare_queue__ring_push(bare_queue_ring_t *ring, void *base, size_t len) {
  int next = (ring->head + 1) & BARE_QUEUE_MASK;

  if (next == ring->tail) return false;

  ring->slots[ring->head].base = base;
  ring->slots[ring->head].len = len;
  ring->head = next;

  return true;
}

static bool
bare_queue__ring_shift(bare_queue_ring_t *ring, bare_queue_message_t *message) {
  if (ring->tail == ring->head) return false;

  *message = ring->slots[ring->tail];
  ring->tail = (ring->tail + 1) & BARE_QUEUE_MASK;

  return true;
}

static void
bare_queue__signal_uv(bare_queue_t *queue) {
  // Never wake a peer that has closed its end; it may be tearing down.
  if (queue->uv_open && !queue->uv_closed) uv_async_send(&queue->async);
}

// Called with the lock held, and it keeps it: `bare_queue_shutdown_thread` takes
// the same lock to clear the callback, so the host cannot free what the callback
// touches between the check here and the call. The callback may therefore only
// wake the host's run loop - never re-enter the queue.
static void
bare_queue__signal_thread(bare_queue_t *queue) {
  if (queue->thread_open && !queue->thread_closed && queue->on_signal_thread) queue->on_signal_thread(&queue->thread_port);
}

static void
bare_queue__on_wakeup_uv(uv_async_t *async) {
  bare_queue_t *queue = (bare_queue_t *) async->data;

  if (queue->on_recv_uv) queue->on_recv_uv(&queue->uv_port);
}

bare_queue_t *
bare_queue_create(void) {
  int err;

  bare_queue_t *queue = malloc(sizeof(bare_queue_t));

  if (queue == NULL) return NULL;

  memset(&queue->to_uv, 0, sizeof(queue->to_uv));
  memset(&queue->to_thread, 0, sizeof(queue->to_thread));

  queue->uv_open = false;
  queue->thread_open = false;
  queue->uv_closed = false;
  queue->thread_closed = false;

  queue->on_signal_thread = NULL;
  queue->on_recv_uv = NULL;

  queue->uv_port.queue = queue;
  queue->uv_port.is_uv = true;
  queue->uv_port.held = NULL;
  queue->uv_port.data = NULL;

  queue->thread_port.queue = queue;
  queue->thread_port.is_uv = false;
  queue->thread_port.held = NULL;
  queue->thread_port.data = NULL;

  err = uv_mutex_init(&queue->lock);
  assert(err == 0);

  return queue;
}

bare_queue_port_t *
bare_queue_open_uv(bare_queue_t *queue, uv_loop_t *loop, bare_queue_recv_cb on_recv) {
  int err;

  err = uv_async_init(loop, &queue->async, bare_queue__on_wakeup_uv);
  assert(err == 0);

  queue->async.data = queue;

  // Published under the lock, because the peer reads them under it: an open can
  // race a signal from the other thread.
  uv_mutex_lock(&queue->lock);

  queue->on_recv_uv = on_recv;
  queue->uv_open = true;

  // Catch up on anything the host queued before we opened.
  bool pending = queue->to_uv.head != queue->to_uv.tail;

  uv_mutex_unlock(&queue->lock);

  if (pending) uv_async_send(&queue->async);

  return &queue->uv_port;
}

bare_queue_port_t *
bare_queue_open_thread(bare_queue_t *queue, bare_queue_signal_cb on_signal) {
  uv_mutex_lock(&queue->lock);

  queue->on_signal_thread = on_signal;
  queue->thread_open = true;

  // Catch up on anything the worklet queued before we opened.
  if (queue->to_thread.head != queue->to_thread.tail) bare_queue__signal_thread(queue);

  uv_mutex_unlock(&queue->lock);

  return &queue->thread_port;
}

int
bare_queue_write(bare_queue_port_t *port, const void *data, size_t len) {
  bare_queue_t *queue = port->queue;

  if (len == 0) return 0;

  // Cap the copied span so the returned byte count never overflows the int
  // return value or aliases a negative status. Callers write the rest.
  if (len > INT_MAX) len = INT_MAX;

  bare_queue_ring_t *ring = port->is_uv ? &queue->to_thread : &queue->to_uv;

  uv_mutex_lock(&queue->lock);

  bool peer_closed = port->is_uv ? queue->thread_closed : queue->uv_closed;

  if (peer_closed) {
    uv_mutex_unlock(&queue->lock);
    return bare_queue_closed;
  }

  void *base = malloc(len);
  memcpy(base, data, len);

  if (!bare_queue__ring_push(ring, base, len)) {
    uv_mutex_unlock(&queue->lock);
    free(base);
    return bare_queue_would_block;
  }

  if (port->is_uv) bare_queue__signal_thread(queue);
  else bare_queue__signal_uv(queue);

  uv_mutex_unlock(&queue->lock);

  return (int) len;
}

int
bare_queue_read(bare_queue_port_t *port, void **data, size_t *len) {
  bare_queue_t *queue = port->queue;

  bare_queue_ring_t *ring = port->is_uv ? &queue->to_uv : &queue->to_thread;

  uv_mutex_lock(&queue->lock);

  bool was_full = ((ring->head + 1) & BARE_QUEUE_MASK) == ring->tail;

  bare_queue_message_t message;
  bool shifted = bare_queue__ring_shift(ring, &message);

  bool peer_closed = port->is_uv ? queue->thread_closed : queue->uv_closed;

  // Draining a full ring lets the peer producer retry a blocked write.
  if (shifted && was_full) {
    if (port->is_uv) bare_queue__signal_thread(queue);
    else bare_queue__signal_uv(queue);
  }

  uv_mutex_unlock(&queue->lock);

  if (!shifted) {
    if (!peer_closed) return bare_queue_would_block;

    free(port->held);
    port->held = NULL;

    *data = NULL;
    *len = 0;

    return bare_queue_ok;
  }

  free(port->held);
  port->held = message.base;

  *data = message.base;
  *len = message.len;

  return bare_queue_ok;
}

void
bare_queue_close(bare_queue_port_t *port) {
  bare_queue_t *queue = port->queue;

  uv_mutex_lock(&queue->lock);

  if (port->is_uv) queue->uv_closed = true;
  else queue->thread_closed = true;

  if (port->is_uv) bare_queue__signal_thread(queue);
  else bare_queue__signal_uv(queue);

  uv_mutex_unlock(&queue->lock);
}

bool
bare_queue_readable(bare_queue_port_t *port) {
  bare_queue_t *queue = port->queue;

  bare_queue_ring_t *ring = port->is_uv ? &queue->to_uv : &queue->to_thread;

  uv_mutex_lock(&queue->lock);

  bool peer_closed = port->is_uv ? queue->thread_closed : queue->uv_closed;
  bool readable = ring->head != ring->tail || peer_closed;

  uv_mutex_unlock(&queue->lock);

  return readable;
}

bool
bare_queue_writable(bare_queue_port_t *port) {
  bare_queue_t *queue = port->queue;

  bare_queue_ring_t *ring = port->is_uv ? &queue->to_thread : &queue->to_uv;

  uv_mutex_lock(&queue->lock);

  bool peer_closed = port->is_uv ? queue->thread_closed : queue->uv_closed;
  bool full = ((ring->head + 1) & BARE_QUEUE_MASK) == ring->tail;
  bool writable = !peer_closed && !full;

  uv_mutex_unlock(&queue->lock);

  return writable;
}

void
bare_queue_destroy(bare_queue_t *queue) {
  bare_queue_message_t message;

  while (bare_queue__ring_shift(&queue->to_uv, &message))
    free(message.base);
  while (bare_queue__ring_shift(&queue->to_thread, &message))
    free(message.base);

  free(queue->uv_port.held);
  free(queue->thread_port.held);

  uv_mutex_destroy(&queue->lock);

  free(queue);
}

void
bare_queue_shutdown_uv(bare_queue_t *queue) {
  if (queue->uv_open) uv_close((uv_handle_t *) &queue->async, NULL);
}

void
bare_queue_shutdown_thread(bare_queue_t *queue) {
  uv_mutex_lock(&queue->lock);

  queue->on_signal_thread = NULL;

  uv_mutex_unlock(&queue->lock);
}
