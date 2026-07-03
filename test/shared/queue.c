#include <assert.h>
#include <string.h>
#include <uv.h>

#include "../../shared/queue.h"

static int signaled = 0;
static int uv_received = 0;

static void
on_signal(bare_queue_port_t *port) {
  signaled++;
}

static void
on_recv_uv(bare_queue_port_t *port) {
  uv_received++;
}

int
main() {
  int err;

  uv_loop_t *loop = uv_default_loop();

  bare_queue_t queue;
  bare_queue_init(&queue);

  bare_queue_port_t *uv = bare_queue_open_uv(&queue, loop, on_recv_uv);
  bare_queue_port_t *thread = bare_queue_open_thread(&queue, on_signal);

  void *data;
  size_t len;

  // Empty reads block on both ends.
  assert(bare_queue_read(uv, &data, &len) == bare_queue_would_block);
  assert(bare_queue_read(thread, &data, &len) == bare_queue_would_block);

  // Host -> worklet.
  assert(bare_queue_write(thread, "ping", 4) == 4);
  assert(bare_queue_read(uv, &data, &len) == bare_queue_ok);
  assert(len == 4 && memcmp(data, "ping", 4) == 0);
  assert(bare_queue_read(uv, &data, &len) == bare_queue_would_block);

  // Worklet -> host, and the host is signalled.
  int before = signaled;
  assert(bare_queue_write(uv, "pong", 4) == 4);
  assert(signaled == before + 1);
  assert(bare_queue_read(thread, &data, &len) == bare_queue_ok);
  assert(len == 4 && memcmp(data, "pong", 4) == 0);

  // A host write wakes the worklet loop.
  uv_received = 0;
  assert(bare_queue_write(thread, "z", 1) == 1);
  err = uv_run(loop, UV_RUN_NOWAIT);
  assert(err >= 0);
  assert(uv_received == 1);
  assert(bare_queue_read(uv, &data, &len) == bare_queue_ok && len == 1);

  // Backpressure: fill the ring, then the next write blocks.
  int n = 0;
  while (bare_queue_write(uv, "x", 1) == 1) n++;
  assert(n == BARE_QUEUE_CAPACITY - 1);
  assert(bare_queue_write(uv, "x", 1) == bare_queue_would_block);
  while (bare_queue_read(thread, &data, &len) == bare_queue_ok && len > 0) n--;
  assert(n == 0);

  // Zero-length writes never enqueue, so they cannot be read as EOF.
  assert(bare_queue_write(uv, "", 0) == 0);
  assert(bare_queue_read(thread, &data, &len) == bare_queue_would_block);

  // Closing one end surfaces EOF on the other, and further writes are rejected.
  bare_queue_close(uv);
  assert(bare_queue_read(thread, &data, &len) == bare_queue_ok && len == 0);
  assert(bare_queue_write(thread, "late", 4) == bare_queue_closed);

  bare_queue_close(thread);
  assert(bare_queue_read(uv, &data, &len) == bare_queue_ok && len == 0);

  bare_queue_destroy(&queue, NULL);

  err = uv_run(loop, UV_RUN_DEFAULT);
  assert(err == 0);

  err = uv_loop_close(loop);
  assert(err == 0);

  return 0;
}
