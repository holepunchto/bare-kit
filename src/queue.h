#ifndef BARE_KIT_QUEUE_H
#define BARE_KIT_QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <uv.h>

// A bidirectional in-process message queue between the worklet, which runs on a
// libuv loop, and the host, which is a plain native thread. Each direction is an
// independent bounded ring of owned byte messages guarded by a single mutex.
// The worklet (uv) end is woken via a uv_async and the host end via a callback.

#define BARE_QUEUE_CAPACITY 1024 // Must be a power of two

typedef struct bare_queue_s bare_queue_t;
typedef struct bare_queue_port_s bare_queue_port_t;

typedef void (*bare_queue_cb)(bare_queue_port_t *port);

enum {
  bare_queue_ok = 0,
  bare_queue_would_block = -1,
  bare_queue_closed = -2,
};

// Each end only moves forward through these.
typedef enum {
  bare_queue_state_idle = 0,
  bare_queue_state_open,
  bare_queue_state_ended, // Writes ended, still reading
  bare_queue_state_closed,
} bare_queue_state_t;

typedef struct {
  void *base;
  size_t len;
} bare_queue_message_t;

typedef struct {
  bare_queue_message_t slots[BARE_QUEUE_CAPACITY];
  int head; // Producer pushes here
  int tail; // Consumer shifts from here
} bare_queue_ring_t;

struct bare_queue_port_s {
  bare_queue_t *queue;
  bool is_uv;
  void *data;
};

struct bare_queue_s {
  uv_async_t async; // Wakes the uv side; also the handle closed on destroy
  uv_mutex_t lock;

  bare_queue_ring_t to_uv;     // Host -> worklet
  bare_queue_ring_t to_thread; // Worklet -> host

  bare_queue_state_t uv_state;
  bare_queue_state_t thread_state;

  bool destroyed;

  bare_queue_cb on_signal_thread;
  bare_queue_cb on_recv_uv;

  bare_queue_port_t uv_port;
  bare_queue_port_t thread_port;
};

// Created on the host thread and handed to the worklet at start. The worklet
// and the host end each own a share: the queue is freed by whichever of
// `bare_queue_destroy` and closing the host end comes last. A host that never
// started a worklet destroys it itself.
bare_queue_t *
bare_queue_create(void);

void
bare_queue_destroy(bare_queue_t *queue);

// Opens the worklet (uv) end. `on_recv` fires on `loop` when this end has
// something to do: data has arrived, the host has drained a full ring so a
// blocked write can be retried, or the host has ended. Wakes are coalesced, so
// on each one read until `bare_queue_would_block` and retry any blocked write.
// It also fires once during this call if the host has already queued data.
bare_queue_port_t *
bare_queue_open_uv(bare_queue_t *queue, uv_loop_t *loop, bare_queue_cb on_recv, void *data);

// Opens the host (native thread) end. `on_signal` is called, possibly from the
// worklet thread, under the same conditions as `on_recv` above, and the host
// hops to its own run loop to act on it. It may fire during this call if the
// worklet has already queued data, and must be safe to call concurrently: an
// open racing a worklet-thread signal can invoke it from two threads at once.
// Returns NULL if the host end has already been opened.
bare_queue_port_t *
bare_queue_open_thread(bare_queue_t *queue, bare_queue_cb on_signal, void *data);

// Copies `len` bytes to the peer. Returns `len`, `bare_queue_would_block` when
// the ring is full, or `bare_queue_closed` once this end has ended or the peer
// has closed, but not when the peer has only ended. A zero length write is a
// no-op returning 0, so it is never mistaken for EOF.
int
bare_queue_write(bare_queue_port_t *port, const void *data, size_t len);

// Hands back the next message, which the caller then owns and must free.
// Returns `bare_queue_ok` with `*len` set (0 meaning EOF once the peer ended),
// or `bare_queue_would_block` when nothing is ready.
int
bare_queue_read(bare_queue_port_t *port, void **data, size_t *len);

// Half-closes this end: the peer reads EOF once drained, while this end keeps
// reading.
void
bare_queue_end(bare_queue_port_t *port);

// Ends this end and retires it: the peer's writes fail and it no longer signals
// this end. Closing the host end gives up its share of the queue, so the port
// must not be used afterwards.
void
bare_queue_close(bare_queue_port_t *port);

// Non-consuming status for a poller. `readable` is true when a read would not
// return `bare_queue_would_block` (data buffered, or EOF once the peer ended);
// `writable` is true when a write would not (space free, this end ended, or the
// peer closed).
bool
bare_queue_readable(bare_queue_port_t *port);

bool
bare_queue_writable(bare_queue_port_t *port);

// Retires the opened worklet (uv) end by closing its wakeup handle. Must be called on
// the worklet thread, with the loop still to run so the close completes.
void
bare_queue_shutdown_uv(bare_queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_QUEUE_H
