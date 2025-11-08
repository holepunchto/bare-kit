#ifndef BARE_KIT_WINDOWS_IPC_H
#define BARE_KIT_WINDOWS_IPC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../ipc.h"

struct bare_ipc_poll_s {
  bare_ipc_t *ipc;

  int events;

  bare_ipc_poll_cb cb;

  void *data;
};

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_WINDOWS_IPC_H
