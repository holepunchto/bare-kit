#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "ipc.h"

enum {
  bare_ipc_poll_close,
  bare_ipc_poll_incoming,
  bare_ipc_poll_outging,
};

void *
bare_ipc_poll__poller(void *data) {
  bare_ipc_poll_t *poll = (bare_ipc_poll_t *) data;

  while (true) {
    struct epoll_event events[4];
    int n = epoll_wait(poll->fd.poll, events, 4, -1);
    assert(n >= 0 || errno == EINTR);

    if (errno == EINTR) continue;

    int flags = 0;

    for (int i = 0; i < n; i++) {
      struct epoll_event event = events[i];

      if (event.data.fd == bare_ipc_poll_close) {
        close(poll->fd.poll);
        close(poll->fd.close);

        pthread_exit(0);
      }

      if (event.data.fd == bare_ipc_poll_incoming && (event.events & EPOLLIN) != 0) {
        flags |= bare_ipc_readable;
      }

      if (event.data.fd == bare_ipc_poll_outging && (event.events & EPOLLOUT) != 0) {
        flags |= bare_ipc_writable;
      }
    }

    if (poll->cb) poll->cb(poll, flags);
  }
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
  int err;

  poll->ipc = ipc;
  poll->fd.poll = epoll_create(3);
  poll->fd.close = eventfd(0, 0);
  poll->events = 0;
  poll->cb = NULL;

  struct epoll_event signal = {
    .events = EPOLLIN,
    .data.fd = bare_ipc_poll_close,
  };

  err = epoll_ctl(poll->fd.poll, EPOLL_CTL_ADD, poll->fd.close, &signal);
  assert(err == 0);

  pthread_create(&poll->thread, NULL, &bare_ipc_poll__poller, (void *) poll);

  return 0;
}

void
bare_ipc_poll_destroy(bare_ipc_poll_t *poll) {
  int err;
  err = bare_ipc_poll_stop(poll);
  assert(err == 0);

  uint64_t signal = 1;

  ssize_t written = write(poll->fd.close, (const void *) &signal, 8);
  assert(written == 8);
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

  int err;

  struct epoll_event incoming = {
    .events = EPOLLIN,
    .data.fd = bare_ipc_poll_incoming,
  };

  if ((events & bare_ipc_readable) == 0) {
    if ((poll->events & bare_ipc_readable) != 0) {
      err = epoll_ctl(poll->fd.poll, EPOLL_CTL_DEL, poll->ipc->incoming, &incoming);
      assert(err == 0);
    }
  } else {
    if ((poll->events & bare_ipc_readable) == 0) {
      err = epoll_ctl(poll->fd.poll, EPOLL_CTL_ADD, poll->ipc->incoming, &incoming);
      assert(err == 0);
    }
  }

  struct epoll_event outgoing = {
    .events = EPOLLOUT,
    .data.fd = bare_ipc_poll_outging,
  };

  if ((events & bare_ipc_writable) == 0) {
    if ((poll->events & bare_ipc_writable) != 0) {
      err = epoll_ctl(poll->fd.poll, EPOLL_CTL_DEL, poll->ipc->outgoing, &outgoing);
      assert(err == 0);
    }
  } else {
    if ((poll->events & bare_ipc_writable) == 0) {
      err = epoll_ctl(poll->fd.poll, EPOLL_CTL_ADD, poll->ipc->outgoing, &outgoing);
      assert(err == 0);
    }
  }

  poll->events = events;
  poll->cb = cb;

  return 0;
}

int
bare_ipc_poll_stop(bare_ipc_poll_t *poll) {
  int err;

  struct epoll_event incoming = {
    .events = EPOLLIN,
  };

  err = epoll_ctl(poll->fd.poll, EPOLL_CTL_DEL, poll->ipc->incoming, &incoming);
  assert(err == 0 || errno == ENOENT);

  struct epoll_event outgoing = {
    .events = EPOLLOUT,
  };

  err = epoll_ctl(poll->fd.poll, EPOLL_CTL_DEL, poll->ipc->outgoing, &outgoing);
  assert(err == 0 || errno == ENOENT);

  poll->events = 0;
  poll->cb = NULL;

  return 0;
}
