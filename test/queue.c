#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#include "../src/queue.h"

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

  // Catch-up on open: data queued before the consumer opens still wakes it.
  {
    bare_queue_t *q = bare_queue_create();

    // Host writes, then the worklet opens.
    bare_queue_port_t *producer = bare_queue_open_thread(q, on_signal, NULL);
    assert(bare_queue_write(producer, "early", 5) == 5);

    uv_received = 0;
    bare_queue_open_uv(q, loop, on_recv_uv, NULL);
    err = uv_run(loop, UV_RUN_NOWAIT);
    assert(err >= 0);
    assert(uv_received == 1);

    bare_queue_close(producer);

    bare_queue_shutdown_uv(q);
    err = uv_run(loop, UV_RUN_DEFAULT);
    assert(err == 0);
    bare_queue_destroy(q);
  }

  {
    bare_queue_t *q = bare_queue_create();

    // Worklet writes, then the host opens: it is signalled during open.
    bare_queue_port_t *producer = bare_queue_open_uv(q, loop, on_recv_uv, NULL);
    assert(bare_queue_write(producer, "early", 5) == 5);

    int before = signaled;
    bare_queue_port_t *consumer = bare_queue_open_thread(q, on_signal, NULL);
    assert(signaled == before + 1);

    bare_queue_close(consumer);

    bare_queue_shutdown_uv(q);
    err = uv_run(loop, UV_RUN_DEFAULT);
    assert(err == 0);
    bare_queue_destroy(q);
  }

  // Non-consuming poll status.
  {
    bare_queue_t *q = bare_queue_create();
    bare_queue_port_t *u = bare_queue_open_uv(q, loop, on_recv_uv, NULL);
    bare_queue_port_t *t = bare_queue_open_thread(q, on_signal, NULL);

    assert(!bare_queue_readable(u) && bare_queue_writable(u));
    assert(!bare_queue_readable(t) && bare_queue_writable(t));

    assert(bare_queue_write(t, "x", 1) == 1);
    assert(bare_queue_readable(u));

    bare_queue_close(t);
    assert(bare_queue_writable(u)); // peer closed, so a write fails rather than blocks
    assert(bare_queue_write(u, "y", 1) == bare_queue_closed);
    assert(bare_queue_readable(u)); // buffered data, then EOF

    bare_queue_shutdown_uv(q);
    err = uv_run(loop, UV_RUN_DEFAULT);
    assert(err == 0);
    bare_queue_destroy(q);
  }

  // Ending is a half-close: the peer reads EOF but may keep writing.
  {
    bare_queue_t *q = bare_queue_create();
    bare_queue_port_t *u = bare_queue_open_uv(q, loop, on_recv_uv, NULL);
    bare_queue_port_t *t = bare_queue_open_thread(q, on_signal, NULL);

    void *data;
    size_t len;

    int before = signaled;
    bare_queue_end(u);
    assert(signaled == before + 1);

    assert(bare_queue_readable(t));
    assert(bare_queue_read(t, &data, &len) == bare_queue_ok && len == 0);
    assert(bare_queue_write(u, "more", 4) == bare_queue_closed);

    assert(bare_queue_writable(t));
    assert(bare_queue_write(t, "late", 4) == 4);
    assert(bare_queue_read(u, &data, &len) == bare_queue_ok);
    assert(len == 4 && memcmp(data, "late", 4) == 0);
    free(data);

    bare_queue_close(t);

    bare_queue_shutdown_uv(q);
    err = uv_run(loop, UV_RUN_DEFAULT);
    assert(err == 0);
    bare_queue_destroy(q);
  }

  // There is only the one host end.
  {
    bare_queue_t *q = bare_queue_create();

    bare_queue_port_t *t = bare_queue_open_thread(q, on_signal, NULL);
    assert(t != NULL);
    assert(bare_queue_open_thread(q, on_signal, NULL) == NULL);

    bare_queue_destroy(q);
    bare_queue_close(t);
  }

  // The queue outlives whichever of the worklet and the host end lets go first.
  {
    bare_queue_t *q = bare_queue_create();
    bare_queue_port_t *u = bare_queue_open_uv(q, loop, on_recv_uv, NULL);
    bare_queue_port_t *t = bare_queue_open_thread(q, on_signal, NULL);

    bare_queue_close(u);
    bare_queue_shutdown_uv(q);
    err = uv_run(loop, UV_RUN_DEFAULT);
    assert(err == 0);
    bare_queue_destroy(q);

    void *data;
    size_t len;
    assert(bare_queue_read(t, &data, &len) == bare_queue_ok && len == 0);
    assert(bare_queue_write(t, "late", 4) == bare_queue_closed);

    bare_queue_close(t);
  }

  bare_queue_t *queue = bare_queue_create();

  bare_queue_port_t *uv = bare_queue_open_uv(queue, loop, on_recv_uv, NULL);
  bare_queue_port_t *thread = bare_queue_open_thread(queue, on_signal, NULL);

  void *data;
  size_t len;

  // Empty reads block on both ends.
  assert(bare_queue_read(uv, &data, &len) == bare_queue_would_block);
  assert(bare_queue_read(thread, &data, &len) == bare_queue_would_block);

  // Host -> worklet.
  assert(bare_queue_write(thread, "ping", 4) == 4);
  assert(bare_queue_read(uv, &data, &len) == bare_queue_ok);
  assert(len == 4 && memcmp(data, "ping", 4) == 0);
  free(data);
  assert(bare_queue_read(uv, &data, &len) == bare_queue_would_block);

  // Worklet -> host, and the host is signalled.
  int before = signaled;
  assert(bare_queue_write(uv, "pong", 4) == 4);
  assert(signaled == before + 1);
  assert(bare_queue_read(thread, &data, &len) == bare_queue_ok);
  assert(len == 4 && memcmp(data, "pong", 4) == 0);
  free(data);

  // Wakes are coalesced: only a write into an empty ring signals.
  before = signaled;
  assert(bare_queue_write(uv, "a", 1) == 1);
  assert(bare_queue_write(uv, "b", 1) == 1);
  assert(signaled == before + 1);
  assert(bare_queue_read(thread, &data, &len) == bare_queue_ok && len == 1);
  free(data);
  assert(bare_queue_read(thread, &data, &len) == bare_queue_ok && len == 1);
  free(data);

  // A host write wakes the worklet loop.
  uv_received = 0;
  assert(bare_queue_write(thread, "z", 1) == 1);
  err = uv_run(loop, UV_RUN_NOWAIT);
  assert(err >= 0);
  assert(uv_received == 1);
  assert(bare_queue_read(uv, &data, &len) == bare_queue_ok && len == 1);
  free(data);

  // Backpressure: fill the ring, then the next write blocks.
  int n = 0;
  while (bare_queue_write(uv, "x", 1) == 1)
    n++;
  assert(n == BARE_QUEUE_CAPACITY - 1);
  assert(bare_queue_write(uv, "x", 1) == bare_queue_would_block);

  // Draining the full ring wakes the worklet so it can retry the blocked write.
  uv_received = 0;
  assert(bare_queue_read(thread, &data, &len) == bare_queue_ok && len == 1);
  free(data);
  n--;
  err = uv_run(loop, UV_RUN_NOWAIT);
  assert(err >= 0);
  assert(uv_received == 1);

  while (bare_queue_read(thread, &data, &len) == bare_queue_ok && len > 0) {
    free(data);
    n--;
  }
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

  bare_queue_shutdown_uv(queue);

  err = uv_run(loop, UV_RUN_DEFAULT);
  assert(err == 0);

  bare_queue_destroy(queue);

  err = uv_loop_close(loop);
  assert(err == 0);

  return 0;
}
