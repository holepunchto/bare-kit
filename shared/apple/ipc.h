#ifndef BARE_KIT_APPLE_IPC_H
#define BARE_KIT_APPLE_IPC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <dispatch/dispatch.h>
#include <stdbool.h>

#include "../ipc.h"

struct bare_ipc_poll_s {
  bare_ipc_t *ipc;

  dispatch_queue_t queue;
  dispatch_source_t reader;
  dispatch_source_t writer;

  int events;

  bare_ipc_poll_cb cb;

  void *data;
};

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_APPLE_IPC_H
