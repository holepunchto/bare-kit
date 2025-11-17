#ifndef BARE_KIT_WINDOWS_IPC_H
#define BARE_KIT_WINDOWS_IPC_H

#ifdef __cplusplus
extern "C" {
#endif

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "../ipc.h"

#define BARE_IPC_POLL_NUM_EVENTS 2

struct bare_ipc_poll_s {
  bare_ipc_t *ipc;

  struct {
    HANDLE thread;
    HANDLE events[BARE_IPC_POLL_NUM_EVENTS];
  } reader;

  struct {
    HANDLE thread;
    HANDLE events[BARE_IPC_POLL_NUM_EVENTS];
  } writer;

  int events;

  bare_ipc_poll_cb cb;

  void *data;
};

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_WINDOWS_IPC_H
