#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "ipc.h"

enum EVENT {
  CLOSE = 0,
  INCOMING,
  OUTGOING,
};

void *
bare_ipc_poll__wait(void *data) {
  bare_ipc_poll_t *poll = (bare_ipc_poll_t *) data;

  while (true) {
    struct epoll_event events[4];
    int n = epoll_wait(poll->epoll_fd, events, 4, -1);
    assert(n >= 0 || errno == EINTR);

    if (errno == EINTR) {
      continue;
    }

    int flags = 0;

    for (int i = 0; i < n; i++) {
      struct epoll_event event = events[i];

      if (event.data.fd == CLOSE) {
        close(poll->close_fd);
        close(poll->epoll_fd);
        pthread_exit(0);
      }

      if (event.data.fd == INCOMING && (event.events & EPOLLIN) != 0) {
        flags |= bare_ipc_readable;
      }

      if (event.data.fd == OUTGOING && (event.events & EPOLLOUT) != 0) {
        flags |= bare_ipc_writable;
      }
    }

    if (poll->cb != NULL) {
      poll->cb(poll, flags);
    }
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
  poll->ipc = ipc;
  poll->epoll_fd = epoll_create(3);
  poll->close_fd = eventfd(0, 0);
  poll->events = 0;
  poll->cb = NULL;

  struct epoll_event signal;
  signal.events = EPOLLIN;
  signal.data.fd = CLOSE;
  int err = epoll_ctl(poll->epoll_fd, EPOLL_CTL_ADD, poll->close_fd, &signal);
  assert(err == 0);

  pthread_create(&poll->thread, NULL, &bare_ipc_poll__wait, (void *) poll);

  return 0;
}

void
bare_ipc_poll_destroy(bare_ipc_poll_t *poll) {
  int err;
  err = bare_ipc_poll_stop(poll);
  assert(err == 0);

  uint64_t signal = 1;
  ssize_t e = write(poll->close_fd, (const void *) &signal, 8);
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

  struct epoll_event incoming;
  incoming.events = EPOLLIN;
  incoming.data.fd = INCOMING;

  if ((events & bare_ipc_readable) == 0) {
    if ((poll->events & bare_ipc_readable) != 0) {
      err = epoll_ctl(poll->epoll_fd, EPOLL_CTL_DEL, poll->ipc->incoming, &incoming);
      assert(err == 0);
    }
  } else {
    if ((poll->events & bare_ipc_readable) == 0) {
      err = epoll_ctl(poll->epoll_fd, EPOLL_CTL_ADD, poll->ipc->incoming, &incoming);
      assert(err == 0);
    }
  }

  struct epoll_event outgoing;
  outgoing.events = EPOLLOUT;
  outgoing.data.fd = OUTGOING;

  if ((events & bare_ipc_writable) == 0) {
    if ((poll->events & bare_ipc_writable) != 0) {
      err = epoll_ctl(poll->epoll_fd, EPOLL_CTL_DEL, poll->ipc->outgoing, &outgoing);
      assert(err == 0);
    }
  } else {
    if ((poll->events & bare_ipc_writable) == 0) {
      err = epoll_ctl(poll->epoll_fd, EPOLL_CTL_ADD, poll->ipc->outgoing, &outgoing);
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

  struct epoll_event incoming;
  incoming.events = EPOLLIN;
  err = epoll_ctl(poll->epoll_fd, EPOLL_CTL_DEL, poll->ipc->incoming, &incoming);
  assert(err == 0 || errno == ENOENT);

  struct epoll_event outgoing;
  outgoing.events = EPOLLOUT;
  err = epoll_ctl(poll->epoll_fd, EPOLL_CTL_DEL, poll->ipc->outgoing, &outgoing);
  assert(err == 0 || errno == ENOENT);

  poll->events = 0;
  poll->cb = NULL;

  return 0;
}
