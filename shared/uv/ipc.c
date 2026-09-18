#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <uv.h>

#include "ipc.h"

// A loop of our own, on a thread of our own: the callback runs there, and hosts
// marshal to their UI thread themselves. `running` is held across the callback so
// a poll cannot be torn down underneath one, and the key marks the thread that
// holds it, for a poll that stops or destroys itself from inside its callback.
typedef struct {
  uv_loop_t loop;

  uv_async_t wake;
  uv_async_t close;

  uv_thread_t thread;
  uv_key_t key;
  uv_mutex_t running;
} bare_ipc_signal_t;

static inline bool
bare_ipc__reentrant(bare_ipc_signal_t *signal) {
  return uv_key_get(&signal->key) == (void *) signal;
}

static inline void
bare_ipc__lock(bare_ipc_signal_t *signal) {
  if (!bare_ipc__reentrant(signal)) uv_mutex_lock(&signal->running);
}

static inline void
bare_ipc__unlock(bare_ipc_signal_t *signal) {
  if (!bare_ipc__reentrant(signal)) uv_mutex_unlock(&signal->running);
}

static void
bare_ipc__on_wake(uv_async_t *handle) {
  bare_ipc_t *ipc = (bare_ipc_t *) uv_handle_get_data((uv_handle_t *) handle);

  bare_ipc_signal_t *signal = (bare_ipc_signal_t *) ipc->signal;

  uv_mutex_lock(&signal->running);

  bare_ipc_poll_t *poll = ipc->poll;

  if (poll != NULL && poll->cb != NULL) {
    int ready = bare_ipc__ready(ipc, poll->events);

    if (ready != 0) poll->cb(poll, ready);
  }

  uv_mutex_unlock(&signal->running);
}

static void
bare_ipc__on_close(uv_async_t *handle) {
  bare_ipc_t *ipc = (bare_ipc_t *) uv_handle_get_data((uv_handle_t *) handle);

  bare_ipc_signal_t *signal = (bare_ipc_signal_t *) ipc->signal;

  uv_close((uv_handle_t *) &signal->wake, NULL);
  uv_close((uv_handle_t *) &signal->close, NULL);
}

static void
bare_ipc__signal_thread(void *data) {
  bare_ipc_t *ipc = (bare_ipc_t *) data;

  bare_ipc_signal_t *signal = (bare_ipc_signal_t *) ipc->signal;

  uv_key_set(&signal->key, (void *) signal);

  int err;

  err = uv_run(&signal->loop, UV_RUN_DEFAULT);
  assert(err == 0);

  err = uv_loop_close(&signal->loop);
  assert(err == 0);
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

  uv_handle_set_data((uv_handle_t *) &signal->wake, (void *) ipc);

  err = uv_async_init(&signal->loop, &signal->close, bare_ipc__on_close);
  assert(err == 0);

  uv_handle_set_data((uv_handle_t *) &signal->close, (void *) ipc);

  err = uv_key_create(&signal->key);
  assert(err == 0);

  err = uv_mutex_init(&signal->running);
  assert(err == 0);

  ipc->signal = signal;

  // The loop and its handles are up, so a signal arriving the moment the port
  // opens has somewhere to land.
  err = uv_thread_create(&signal->thread, bare_ipc__signal_thread, (void *) ipc);
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

  err = uv_async_send(&signal->close);
  assert(err == 0);

  err = uv_thread_join(&signal->thread);
  assert(err == 0);

  uv_mutex_destroy(&signal->running);

  uv_key_delete(&signal->key);

  free(signal);

  ipc->signal = NULL;
}

int
bare_ipc_poll_alloc(bare_ipc_poll_t **result) {
  bare_ipc_poll_t *poll = malloc(sizeof(bare_ipc_poll_t));

  if (poll == NULL) return -1;

  *result = poll;

  return 0;
}

int
bare_ipc_poll_init(bare_ipc_poll_t *poll, bare_ipc_t *ipc) {
  poll->ipc = ipc;
  poll->events = 0;
  poll->cb = NULL;
  poll->data = NULL;

  bare_ipc_signal_t *signal = (bare_ipc_signal_t *) ipc->signal;

  bare_ipc__lock(signal);

  ipc->poll = poll;

  bare_ipc__unlock(signal);

  return 0;
}

void
bare_ipc_poll_destroy(bare_ipc_poll_t *poll) {
  bare_ipc_t *ipc = poll->ipc;

  bare_ipc_signal_t *signal = (bare_ipc_signal_t *) ipc->signal;

  // Under the lock, so the caller may free the poll as soon as this returns.
  bare_ipc__lock(signal);

  poll->events = 0;
  poll->cb = NULL;

  ipc->poll = NULL;

  bare_ipc__unlock(signal);
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

  bare_ipc_t *ipc = poll->ipc;

  bare_ipc_signal_t *signal = (bare_ipc_signal_t *) ipc->signal;

  bare_ipc__lock(signal);

  poll->events = events;
  poll->cb = cb;

  bare_ipc__unlock(signal);

  // Level-triggered: re-check in case data or space is already there.
  bare_ipc__signal(ipc);

  return 0;
}

int
bare_ipc_poll_stop(bare_ipc_poll_t *poll) {
  bare_ipc_signal_t *signal = (bare_ipc_signal_t *) poll->ipc->signal;

  bare_ipc__lock(signal);

  poll->events = 0;
  poll->cb = NULL;

  bare_ipc__unlock(signal);

  return 0;
}
