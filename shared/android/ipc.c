#include <android/looper.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "ipc.h"

// The looper wakes on a file descriptor, so the signal is an eventfd. The ipc
// owns it because the worklet writes to it; the poll owns the registration,
// which belongs to the host thread whose looper it is.
typedef struct {
  int wake;
} bare_ipc_signal_t;

int
bare_ipc__signal_init(bare_ipc_t *ipc) {
  bare_ipc_signal_t *signal = malloc(sizeof(bare_ipc_signal_t));

  if (signal == NULL) return -1;

  // The count stays up until a callback reads it, so a poll that starts after a
  // signal still wakes.
  signal->wake = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  assert(signal->wake >= 0);

  ipc->signal = signal;

  return 0;
}

void
bare_ipc__signal(bare_ipc_t *ipc) {
  bare_ipc_signal_t *signal = (bare_ipc_signal_t *) ipc->signal;

  uint64_t value = 1;

  ssize_t written = write(signal->wake, &value, sizeof(value));
  assert(written == sizeof(value));
}

void
bare_ipc__signal_destroy(bare_ipc_t *ipc) {
  bare_ipc_signal_t *signal = (bare_ipc_signal_t *) ipc->signal;

  int err = close(signal->wake);
  assert(err == 0);

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
  poll->looper = ALooper_forThread();

  ALooper_acquire(poll->looper);

  ipc->poll = poll;

  return 0;
}

void
bare_ipc_poll_destroy(bare_ipc_poll_t *poll) {
  int err;
  err = bare_ipc_poll_stop(poll);
  assert(err == 0);

  poll->ipc->poll = NULL;

  ALooper_release(poll->looper);
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

// Runs on the looper's thread, which is also the thread that started the poll, so
// the poll cannot go away underneath.
static int
bare_ipc__on_wake(int fd, int events, void *data) {
  bare_ipc_poll_t *poll = (bare_ipc_poll_t *) data;

  uint64_t value;

  ssize_t read_len = read(fd, &value, sizeof(value));
  assert(read_len == sizeof(value) || errno == EAGAIN);

  if (poll->cb != NULL) {
    int ready = bare_ipc__ready(poll->ipc, poll->events);

    if (ready != 0) poll->cb(poll, ready);
  }

  return 1; // Don't deregister the file descriptor
}

int
bare_ipc_poll_start(bare_ipc_poll_t *poll, int events, bare_ipc_poll_cb cb) {
  if (events == 0) return bare_ipc_poll_stop(poll);

  bare_ipc_t *ipc = poll->ipc;

  bare_ipc_signal_t *signal = (bare_ipc_signal_t *) ipc->signal;

  if (poll->events == 0) {
    int err = ALooper_addFd(poll->looper, signal->wake, ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT, bare_ipc__on_wake, (void *) poll);
    assert(err == 1);
  }

  poll->events = events;
  poll->cb = cb;

  // Level-triggered: re-check in case data or space is already there.
  bare_ipc__signal(ipc);

  return 0;
}

int
bare_ipc_poll_stop(bare_ipc_poll_t *poll) {
  bare_ipc_signal_t *signal = (bare_ipc_signal_t *) poll->ipc->signal;

  if (poll->events != 0) ALooper_removeFd(poll->looper, signal->wake);

  poll->events = 0;
  poll->cb = NULL;

  return 0;
}
