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
// The worklet (uv) end is woken via a uv_async; the host end is woken via a
// caller-provided signal callback, which is the only platform-specific concern.

#define BARE_QUEUE_CAPACITY 1024 // Must be a power of two

typedef struct bare_queue_s bare_queue_t;
typedef struct bare_queue_port_s bare_queue_port_t;

typedef void (*bare_queue_signal_cb)(bare_queue_port_t *port);
typedef void (*bare_queue_recv_cb)(bare_queue_port_t *port);

enum {
  bare_queue_ok = 0,
  bare_queue_would_block = -1,
  bare_queue_closed = -2,
};

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
  void *held; // Buffer handed to the last read, freed on the next read
  void *data;
};

struct bare_queue_s {
  uv_async_t async; // Wakes the uv side; also the handle closed on destroy
  uv_mutex_t lock;

  bare_queue_ring_t to_uv;     // Host -> worklet
  bare_queue_ring_t to_thread; // Worklet -> host

  bool uv_open;
  bool thread_open;
  bool uv_closed;
  bool thread_closed;

  bare_queue_signal_cb on_signal_thread;
  bare_queue_recv_cb on_recv_uv;

  bare_queue_port_t uv_port;
  bare_queue_port_t thread_port;
};

// The worklet closes its end after it has parked, so by the ownership invariants
// the queue belongs to the worklet, and cannot live in the host's storage: the
// host is free to destroy its worklet the moment it has signalled it.
//
// Created on the host thread, handed to the worklet at start, and freed by
// `bare_queue_destroy` on the worklet thread once its loop is done. A host that
// never started a worklet frees it itself.
bare_queue_t *
bare_queue_create(void);

void
bare_queue_destroy(bare_queue_t *queue);

// Opens the worklet (uv) end. `on_recv` fires on `loop` whenever this end's
// state changes: the host has sent data to read, or has drained a full ring so
// a blocked write can be retried. Re-check both readability and writability on
// each wake. It also fires once during this call if the host has already queued
// data.
bare_queue_port_t *
bare_queue_open_uv(bare_queue_t *queue, uv_loop_t *loop, bare_queue_recv_cb on_recv);

// Opens the host (native thread) end. `on_signal` is called, possibly from the
// worklet thread, whenever this end's state changes: data to read, or space
// freed so a blocked write can be retried. The host hops to its own run loop
// and re-checks both. It may fire during this call if the worklet has already
// queued data, and must be safe to call concurrently: an open racing a
// worklet-thread signal can invoke it from two threads at once.
bare_queue_port_t *
bare_queue_open_thread(bare_queue_t *queue, bare_queue_signal_cb on_signal);

// Copies `len` bytes to the peer. Returns `len`, `bare_queue_would_block` when
// the ring is full, or `bare_queue_closed` when the peer has closed. A zero
// length write is a no-op returning 0, so it is never mistaken for EOF.
int
bare_queue_write(bare_queue_port_t *port, const void *data, size_t len);

// Hands back the next message, owned by the port until the following read.
// Returns `bare_queue_ok` with `*len` set (0 meaning EOF once the peer closed),
// or `bare_queue_would_block` when nothing is ready.
int
bare_queue_read(bare_queue_port_t *port, void **data, size_t *len);

void
bare_queue_close(bare_queue_port_t *port);

// Non-consuming status for a poller. `readable` is true when a read would not
// return `bare_queue_would_block` (data buffered, or EOF once the peer closed);
// `writable` is true when a write would not (space free and the peer open).
bool
bare_queue_readable(bare_queue_port_t *port);

bool
bare_queue_writable(bare_queue_port_t *port);

// Retires the worklet (uv) end by closing its wakeup handle. Must be called on
// the worklet thread, with the loop still to run so the close completes.
void
bare_queue_shutdown_uv(bare_queue_t *queue);

// Retires the host end: after this the worklet will not call `on_signal` again,
// so the host may free whatever that callback touches. Must be called before the
// host signals its worklet to be destroyed, because afterwards the queue is the
// worklet's alone.
void
bare_queue_shutdown_thread(bare_queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_QUEUE_H
