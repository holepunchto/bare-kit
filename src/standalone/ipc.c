#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <uv.h>

#include "../ipc.h"
#include "../platform.h"

// Polls run on a thread of their own rather than a host run loop, so that their
// callbacks are called the same way on every platform. `running` is held across
// a callback so a poll cannot be torn down underneath it, and is recursive so the
// callback can stop or destroy its own poll, or its ipc.
typedef struct {
  uv_loop_t loop;

  uv_async_t wake;
  uv_async_t close;

  uv_thread_t thread;
  uv_mutex_t running;

  // NULL once the ipc is destroyed.
  bare_ipc_t *ipc;

  // Set when the ipc is destroyed from a callback, in which case the thread
  // frees the signal once its loop is done.
  bool detached;
} bare_ipc_signal_t;

static void
bare_ipc__on_wake(uv_async_t *handle) {
  bare_ipc_signal_t *signal = (bare_ipc_signal_t *) uv_handle_get_data((uv_handle_t *) handle);

  uv_mutex_lock(&signal->running);

  if (signal->ipc) bare_ipc__dispatch(signal->ipc);

  uv_mutex_unlock(&signal->running);
}

static void
bare_ipc__on_close(uv_async_t *handle) {
  bare_ipc_signal_t *signal = (bare_ipc_signal_t *) uv_handle_get_data((uv_handle_t *) handle);

  uv_close((uv_handle_t *) &signal->wake, NULL);
  uv_close((uv_handle_t *) &signal->close, NULL);
}

static void
bare_ipc__signal_thread(void *data) {
  bare_ipc_signal_t *signal = (bare_ipc_signal_t *) data;

  bare_kit__on_thread_enter();

  int err;

  err = uv_run(&signal->loop, UV_RUN_DEFAULT);
  assert(err == 0);

  err = uv_loop_close(&signal->loop);
  assert(err == 0);

  if (signal->detached) {
    uv_mutex_destroy(&signal->running);

    free(signal);
  }

  bare_kit__on_thread_exit();
}

int
bare_ipc__signal_init(bare_ipc_t *ipc) {
  bare_ipc_signal_t *signal = malloc(sizeof(bare_ipc_signal_t));

  if (signal == NULL) return -1;

  int err;

  err = uv_loop_init(&signal->loop);
  assert(err == 0);

  err = uv_async_init(&signal->loop, &signal->wake, bare_ipc__on_wake);
  assert(err == 0);

  uv_handle_set_data((uv_handle_t *) &signal->wake, (void *) signal);

  err = uv_async_init(&signal->loop, &signal->close, bare_ipc__on_close);
  assert(err == 0);

  uv_handle_set_data((uv_handle_t *) &signal->close, (void *) signal);

  err = uv_mutex_init_recursive(&signal->running);
  assert(err == 0);

  signal->ipc = ipc;
  signal->detached = false;

  ipc->signal = signal;

  err = uv_thread_create(&signal->thread, bare_ipc__signal_thread, (void *) signal);
  assert(err == 0);

  return 0;
}

void
bare_ipc__signal(bare_ipc_t *ipc) {
  bare_ipc_signal_t *signal = (bare_ipc_signal_t *) ipc->signal;

  int err = uv_async_send(&signal->wake);
  assert(err == 0);
}

void
bare_ipc__signal_destroy(bare_ipc_t *ipc) {
  bare_ipc_signal_t *signal = (bare_ipc_signal_t *) ipc->signal;

  int err;

  uv_mutex_lock(&signal->running);

  signal->ipc = NULL;

  uv_mutex_unlock(&signal->running);

  err = uv_async_send(&signal->close);
  assert(err == 0);

  uv_thread_t self = uv_thread_self();

  if (uv_thread_equal(&self, &signal->thread)) {
    // Destroyed from a poll callback, and a thread cannot join itself.
    signal->detached = true;

    err = uv_thread_detach(&signal->thread);
    assert(err == 0);
  } else {
    err = uv_thread_join(&signal->thread);
    assert(err == 0);

    uv_mutex_destroy(&signal->running);

    free(signal);
  }

  ipc->signal = NULL;
}

void
bare_ipc__set_poll(bare_ipc_t *ipc, bare_ipc_poll_t *poll) {
  bare_ipc_signal_t *signal = (bare_ipc_signal_t *) ipc->signal;

  uv_mutex_lock(&signal->running);

  ipc->poll = poll;

  uv_mutex_unlock(&signal->running);
}
