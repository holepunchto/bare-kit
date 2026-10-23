#include <android/looper.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "ipc.h"

// The looper wakes on a file descriptor, so the signal is an eventfd.
typedef struct {
  int wake;
} bare_ipc_signal_t;

int
bare_ipc__signal_init(bare_ipc_t *ipc) {
  bare_ipc_signal_t *signal = malloc(sizeof(bare_ipc_signal_t));

  if (signal == NULL) return -1;

  // The count persists until read, so a poll started after a signal still wakes.
  signal->wake = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);

  if (signal->wake < 0) {
    free(signal);

    return -1;
  }

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

// The fd registration belongs to the looper's thread. Changed from elsewhere, it
// races the callback, which may then run on a freed poll.
static void
bare_ipc__assert_looper(bare_ipc_poll_t *poll) {
  assert(ALooper_forThread() == poll->looper);
}

void
bare_ipc_poll_destroy(bare_ipc_poll_t *poll) {
  bare_ipc__assert_looper(poll);

  int err;
  err = bare_ipc_poll_stop(poll);
  assert(err == 0);

  poll->ipc->poll = NULL;

  ALooper_release(poll->looper);
}

static int
bare_ipc__on_wake(int fd, int events, void *data) {
  bare_ipc_poll_t *poll = (bare_ipc_poll_t *) data;

  uint64_t value;

  ssize_t read_len = read(fd, &value, sizeof(value));
  assert(read_len == sizeof(value) || errno == EAGAIN);

  bare_ipc__dispatch(poll->ipc);

  return 1; // Don't deregister the file descriptor
}

int
bare_ipc_poll_start(bare_ipc_poll_t *poll, int events, bare_ipc_poll_cb cb) {
  if (events == 0) return bare_ipc_poll_stop(poll);

  bare_ipc__assert_looper(poll);

  bare_ipc_t *ipc = poll->ipc;

  bare_ipc_signal_t *signal = (bare_ipc_signal_t *) ipc->signal;

  if (poll->events == 0) {
    int err = ALooper_addFd(poll->looper, signal->wake, ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT, bare_ipc__on_wake, (void *) poll);
    assert(err == 1);
  }

  poll->events = events;
  poll->cb = cb;

  // Level-triggered: signal once in case the events are already ready.
  bare_ipc__signal(ipc);

  return 0;
}

int
bare_ipc_poll_stop(bare_ipc_poll_t *poll) {
  bare_ipc__assert_looper(poll);

  bare_ipc_signal_t *signal = (bare_ipc_signal_t *) poll->ipc->signal;

  if (poll->events != 0) ALooper_removeFd(poll->looper, signal->wake);

  poll->events = 0;
  poll->cb = NULL;

  return 0;
}
