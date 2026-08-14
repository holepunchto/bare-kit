#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <uv.h>

#include "../../shared/queue.h"

// Exercises the queue across a real thread boundary: the main thread produces
// on the thread port while a uv thread consumes on the uv port. Verifies every
// message arrives once, in order, through the mutex + async wakeup, that
// backpressure holds under a full ring, and that close reaches the consumer as
// EOF.

#define MESSAGES 100000

static bare_queue_t *queue;
static uv_loop_t loop;
static uv_sem_t opened;

static int received = 0; // uv thread only

static void
on_recv_uv(bare_queue_port_t *port) {
  void *data;
  size_t len;

  while (bare_queue_read(port, &data, &len) == bare_queue_ok) {
    if (len == 0) {
      // EOF: unref the async so the loop can drain and uv_run returns.
      uv_unref((uv_handle_t *) &port->queue->async);
      return;
    }

    assert(len == sizeof(int));
    assert(*(int *) data == received);
    received++;
  }
}

static void
on_thread(void *arg) {
  bare_queue_open_uv(queue, &loop, on_recv_uv);

  uv_sem_post(&opened);

  int err = uv_run(&loop, UV_RUN_DEFAULT);
  assert(err == 0);
}

int
main() {
  int err;

  queue = bare_queue_create();

  err = uv_loop_init(&loop);
  assert(err == 0);

  err = uv_sem_init(&opened, 0);
  assert(err == 0);

  bare_queue_port_t *thread = bare_queue_open_thread(queue, NULL);

  uv_thread_t consumer;
  err = uv_thread_create(&consumer, on_thread, NULL);
  assert(err == 0);

  uv_sem_wait(&opened);

  for (int i = 0; i < MESSAGES;) {
    int status = bare_queue_write(thread, &i, sizeof(int));

    if (status == (int) sizeof(int)) i++;
    else assert(status == bare_queue_would_block); // full ring: retry
  }

  bare_queue_close(thread);

  err = uv_thread_join(&consumer);
  assert(err == 0);

  assert(received == MESSAGES);

  // The consumer only unref'd the async; close it and pump once to finish.
  bare_queue_shutdown_uv(queue);
  bare_queue_shutdown_thread(queue);

  err = uv_run(&loop, UV_RUN_DEFAULT);
  assert(err == 0);

  bare_queue_destroy(queue);

  err = uv_loop_close(&loop);
  assert(err == 0);

  uv_sem_destroy(&opened);

  return 0;
}
