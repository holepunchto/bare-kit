#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#include "queue.h"

#define BARE_QUEUE_MASK (BARE_QUEUE_CAPACITY - 1)

static bool
bare_queue__ring_empty(bare_queue_ring_t *ring) {
  return ring->head == ring->tail;
}

static bool
bare_queue__ring_full(bare_queue_ring_t *ring) {
  return ((ring->head + 1) & BARE_QUEUE_MASK) == ring->tail;
}

static bool
bare_queue__ring_push(bare_queue_ring_t *ring, void *base, size_t len) {
  if (bare_queue__ring_full(ring)) return false;

  ring->slots[ring->head].base = base;
  ring->slots[ring->head].len = len;
  ring->head = (ring->head + 1) & BARE_QUEUE_MASK;

  return true;
}

static bool
bare_queue__ring_shift(bare_queue_ring_t *ring, bare_queue_message_t *message) {
  if (bare_queue__ring_empty(ring)) return false;

  *message = ring->slots[ring->tail];
  ring->tail = (ring->tail + 1) & BARE_QUEUE_MASK;

  return true;
}

static bare_queue_ring_t *
bare_queue__inbox(bare_queue_port_t *port) {
  return port->is_uv ? &port->queue->to_uv : &port->queue->to_thread;
}

static bare_queue_ring_t *
bare_queue__outbox(bare_queue_port_t *port) {
  return port->is_uv ? &port->queue->to_thread : &port->queue->to_uv;
}

static bare_queue_state_t *
bare_queue__state(bare_queue_port_t *port) {
  return port->is_uv ? &port->queue->uv_state : &port->queue->thread_state;
}

static bare_queue_state_t
bare_queue__peer_state(bare_queue_port_t *port) {
  return port->is_uv ? port->queue->thread_state : port->queue->uv_state;
}

static bool
bare_queue__listening(bare_queue_state_t state) {
  return state == bare_queue_state_open || state == bare_queue_state_ended;
}

static void
bare_queue__signal_uv(bare_queue_t *queue) {
  if (bare_queue__listening(queue->uv_state)) uv_async_send(&queue->async);
}

// Chosen and called with the lock held, so a host that retires its end cannot
// free what the callback touches in between. The callback may therefore only
// wake the host's run loop - it must never re-enter the queue.
static void
bare_queue__signal_thread(bare_queue_t *queue) {
  if (bare_queue__listening(queue->thread_state) && queue->on_signal_thread) queue->on_signal_thread(&queue->thread_port);
}

static void
bare_queue__signal_peer(bare_queue_port_t *port) {
  if (port->is_uv) bare_queue__signal_thread(port->queue);
  else bare_queue__signal_uv(port->queue);
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

  queue->to_uv.head = queue->to_uv.tail = 0;
  queue->to_thread.head = queue->to_thread.tail = 0;

  queue->uv_state = bare_queue_state_idle;
  queue->thread_state = bare_queue_state_idle;

  queue->destroyed = false;

  queue->on_signal_thread = NULL;
  queue->on_recv_uv = NULL;

  queue->uv_port.queue = queue;
  queue->uv_port.is_uv = true;
  queue->uv_port.data = NULL;

  queue->thread_port.queue = queue;
  queue->thread_port.is_uv = false;
  queue->thread_port.data = NULL;

  err = uv_mutex_init(&queue->lock);
  assert(err == 0);

  return queue;
}

bare_queue_port_t *
bare_queue_open_uv(bare_queue_t *queue, uv_loop_t *loop, bare_queue_cb on_recv, void *data) {
  int err;

  err = uv_async_init(loop, &queue->async, bare_queue__on_wakeup_uv);
  assert(err == 0);

  queue->async.data = queue;

  // Published under the lock, because the peer reads them under it: an open can
  // race a signal from the other thread.
  uv_mutex_lock(&queue->lock);

  queue->uv_port.data = data;
  queue->on_recv_uv = on_recv;
  queue->uv_state = bare_queue_state_open;

  // Catch up on anything the host queued before we opened.
  bool pending = !bare_queue__ring_empty(&queue->to_uv);

  uv_mutex_unlock(&queue->lock);

  if (pending) uv_async_send(&queue->async);

  return &queue->uv_port;
}

bare_queue_port_t *
bare_queue_open_thread(bare_queue_t *queue, bare_queue_cb on_signal, void *data) {
  uv_mutex_lock(&queue->lock);

  if (queue->thread_state != bare_queue_state_idle) {
    uv_mutex_unlock(&queue->lock);
    return NULL;
  }

  queue->thread_port.data = data;
  queue->on_signal_thread = on_signal;
  queue->thread_state = bare_queue_state_open;

  // Catch up on anything the worklet queued before we opened.
  if (!bare_queue__ring_empty(&queue->to_thread)) bare_queue__signal_thread(queue);

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

  bare_queue_ring_t *ring = bare_queue__outbox(port);

  // Copied before taking the lock, so a large write does not hold it.
  void *base = malloc(len);
  memcpy(base, data, len);

  uv_mutex_lock(&queue->lock);

  bool was_empty = bare_queue__ring_empty(ring);

  int result = (int) len;

  if (*bare_queue__state(port) >= bare_queue_state_ended || bare_queue__peer_state(port) == bare_queue_state_closed) result = bare_queue_closed;
  else if (!bare_queue__ring_push(ring, base, len)) result = bare_queue_would_block;
  else if (was_empty) bare_queue__signal_peer(port);

  uv_mutex_unlock(&queue->lock);

  if (result < 0) free(base);

  return result;
}

int
bare_queue_read(bare_queue_port_t *port, void **data, size_t *len) {
  bare_queue_t *queue = port->queue;

  bare_queue_ring_t *ring = bare_queue__inbox(port);

  uv_mutex_lock(&queue->lock);

  bool was_full = bare_queue__ring_full(ring);

  bare_queue_message_t message;
  bool shifted = bare_queue__ring_shift(ring, &message);

  bool peer_ended = bare_queue__peer_state(port) >= bare_queue_state_ended;

  // Draining a full ring lets the peer producer retry a blocked write.
  if (shifted && was_full) bare_queue__signal_peer(port);

  uv_mutex_unlock(&queue->lock);

  if (shifted) {
    *data = message.base;
    *len = message.len;
  } else if (peer_ended) {
    *data = NULL;
    *len = 0;
  } else {
    return bare_queue_would_block;
  }

  return bare_queue_ok;
}

static void
bare_queue__free(bare_queue_t *queue) {
  bare_queue_message_t message;

  while (bare_queue__ring_shift(&queue->to_uv, &message))
    free(message.base);
  while (bare_queue__ring_shift(&queue->to_thread, &message))
    free(message.base);

  uv_mutex_destroy(&queue->lock);

  free(queue);
}

// Whether the last share of the queue is gone. Called with the lock held.
static bool
bare_queue__unowned(bare_queue_t *queue) {
  return queue->destroyed && (queue->thread_state == bare_queue_state_idle || queue->thread_state == bare_queue_state_closed);
}

static void
bare_queue__advance(bare_queue_port_t *port, bare_queue_state_t state) {
  bare_queue_t *queue = port->queue;

  uv_mutex_lock(&queue->lock);

  bare_queue_state_t *current = bare_queue__state(port);

  if (*current < state) *current = state;

  bare_queue__signal_peer(port);

  bool unowned = bare_queue__unowned(queue);

  uv_mutex_unlock(&queue->lock);

  if (unowned) bare_queue__free(queue);
}

void
bare_queue_end(bare_queue_port_t *port) {
  bare_queue__advance(port, bare_queue_state_ended);
}

void
bare_queue_close(bare_queue_port_t *port) {
  bare_queue__advance(port, bare_queue_state_closed);
}

bool
bare_queue_readable(bare_queue_port_t *port) {
  bare_queue_t *queue = port->queue;

  uv_mutex_lock(&queue->lock);

  bool readable = !bare_queue__ring_empty(bare_queue__inbox(port)) || bare_queue__peer_state(port) >= bare_queue_state_ended;

  uv_mutex_unlock(&queue->lock);

  return readable;
}

bool
bare_queue_writable(bare_queue_port_t *port) {
  bare_queue_t *queue = port->queue;

  uv_mutex_lock(&queue->lock);

  bool writable = !bare_queue__ring_full(bare_queue__outbox(port)) || *bare_queue__state(port) >= bare_queue_state_ended || bare_queue__peer_state(port) == bare_queue_state_closed;

  uv_mutex_unlock(&queue->lock);

  return writable;
}

void
bare_queue_destroy(bare_queue_t *queue) {
  uv_mutex_lock(&queue->lock);

  queue->destroyed = true;

  bool unowned = bare_queue__unowned(queue);

  uv_mutex_unlock(&queue->lock);

  if (unowned) bare_queue__free(queue);
}

void
bare_queue_shutdown_uv(bare_queue_t *queue) {
  uv_close((uv_handle_t *) &queue->async, NULL);
}
