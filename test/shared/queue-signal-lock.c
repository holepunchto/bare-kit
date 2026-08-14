#include <assert.h>
#include <stdbool.h>
#include <uv.h>

#include "../../shared/queue.h"

// A signal callback must run with the queue's lock held. Without that the peer
// can retire its end - and free whatever the callback touches - between the
// queue deciding to call it and the call happening.
//
// The callback parks for a while and this asserts the lock cannot be taken for
// as long as it is in there.

static bare_queue_t *queue;

static uv_sem_t entered;
static uv_loop_t loop;

static bool locked_during_callback;

static void
on_signal(bare_queue_port_t *port) {
  uv_sem_post(&entered);

  // Long enough that a lock taken meanwhile is a real observation, not a race
  // the assert happened to win.
  uv_sleep(500);
}

static void
on_recv_uv(bare_queue_port_t *port) {}

static void
on_thread(void *arg) {
  bare_queue_port_t *uv = (bare_queue_port_t *) arg;

  // Wakes the host end, which calls on_signal from this thread.
  int written = bare_queue_write(uv, "x", 1);
  assert(written == 1);
}

int
main() {
  int err;

  err = uv_loop_init(&loop);
  assert(err == 0);

  err = uv_sem_init(&entered, 0);
  assert(err == 0);

  queue = bare_queue_create();
  assert(queue != NULL);

  bare_queue_port_t *uv = bare_queue_open_uv(queue, &loop, on_recv_uv);
  bare_queue_open_thread(queue, on_signal);

  uv_thread_t producer;
  err = uv_thread_create(&producer, on_thread, (void *) uv);
  assert(err == 0);

  uv_sem_wait(&entered);

  // The callback is running right now. Taking the lock must not be possible.
  if (uv_mutex_trylock(&queue->lock) == 0) {
    uv_mutex_unlock(&queue->lock);
  } else {
    locked_during_callback = true;
  }

  err = uv_thread_join(&producer);
  assert(err == 0);

  assert(locked_during_callback);

  bare_queue_shutdown_uv(queue);
  bare_queue_shutdown_thread(queue);

  err = uv_run(&loop, UV_RUN_DEFAULT);
  assert(err == 0);

  err = uv_loop_close(&loop);
  assert(err == 0);

  bare_queue_destroy(queue);

  uv_sem_destroy(&entered);

  return 0;
}
