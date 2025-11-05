#ifndef BARE_KIT_LINUX_IPC_H
#define BARE_KIT_LINUX_IPC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <sys/epoll.h>

#include "../ipc.h"

struct bare_ipc_poll_s {
  bare_ipc_t *ipc;

  struct {
    int poll;
    int close;
  } fd;

  pthread_t thread;

  int events;

  bare_ipc_poll_cb cb;

  void *data;
};

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_LINUX_IPC_H
