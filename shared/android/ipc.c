#include <android/looper.h>
#include <assert.h>
#include <stdlib.h>

#include "ipc.h"

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
  poll->looper = ALooper_forThread();

  ALooper_acquire(poll->looper);

  return 0;
}

void
bare_ipc_poll_destroy(bare_ipc_poll_t *poll) {
  int err;
  err = bare_ipc_poll_stop(poll);
  assert(err == 0);

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

static int
bare_ipc__on_poll(int fd, int events, void *data) {
  bare_ipc_poll_t *poll = data;

  if (poll->cb) poll->cb(poll, (events & ALOOPER_EVENT_INPUT ? bare_ipc_readable : 0) | (events & ALOOPER_EVENT_OUTPUT ? bare_ipc_writable : 0));

  return 1; // Don't deregister the file descriptor
}

int
bare_ipc_poll_start(bare_ipc_poll_t *poll, int events, bare_ipc_poll_cb cb) {
  if (events == 0) return bare_ipc_poll_stop(poll);

  int err;

  if ((events & bare_ipc_readable) == 0) {
    if ((poll->events & bare_ipc_readable) != 0) {
      err = ALooper_removeFd(poll->looper, poll->ipc->incoming);
      assert(err == 1);
    }
  } else {
    if ((poll->events & bare_ipc_readable) == 0) {
      err = ALooper_addFd(poll->looper, poll->ipc->incoming, ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT, bare_ipc__on_poll, (void *) poll);
      assert(err == 1);
    }
  }

  if ((events & bare_ipc_writable) == 0) {
    if ((poll->events & bare_ipc_writable) != 0) {
      err = ALooper_removeFd(poll->looper, poll->ipc->outgoing);
      assert(err == 1);
    }
  } else {
    if ((poll->events & bare_ipc_writable) == 0) {
      err = ALooper_addFd(poll->looper, poll->ipc->outgoing, ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_OUTPUT, bare_ipc__on_poll, (void *) poll);
      assert(err == 1);
    }
  }

  poll->events = events;
  poll->cb = cb;

  return 0;
}

int
bare_ipc_poll_stop(bare_ipc_poll_t *poll) {
  ALooper_removeFd(poll->looper, poll->ipc->incoming);

  ALooper_removeFd(poll->looper, poll->ipc->outgoing);

  poll->events = 0;
  poll->cb = NULL;

  return 0;
}
