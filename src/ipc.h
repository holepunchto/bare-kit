#ifndef BARE_KIT_IPC_H
#define BARE_KIT_IPC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <bare-kit.h>
#include <stdatomic.h>
#include <stddef.h>

#include "queue.h"
#include "worklet.h"

struct bare_ipc_s {
  // NULL once closed, as closing gives up the ipc's share of the queue.
  bare_queue_port_t *port;
  uv_mutex_t lock;

  // The rest of a message too large for the last read.
  bare_queue_message_t pending;
  size_t offset;

  bare_ipc_poll_t *poll;

  // Owned by the ipc rather than the poll, as the worklet signals it for as long
  // as the port is open.
  void *signal;
};

struct bare_ipc_poll_s {
  bare_ipc_t *ipc;

  _Atomic int events;
  _Atomic bare_ipc_poll_cb cb;

  void *data;
};

// Implemented per platform. `bare_ipc__signal` may run on the worklet thread, so
// it may only touch state owned by the ipc. `bare_ipc__set_poll` waits for a
// running poll callback, unless called from it, so that a detached poll may be
// freed.
int
bare_ipc__signal_init(bare_ipc_t *ipc);

void
bare_ipc__signal_destroy(bare_ipc_t *ipc);

void
bare_ipc__signal(bare_ipc_t *ipc);

void
bare_ipc__set_poll(bare_ipc_t *ipc, bare_ipc_poll_t *poll);

// Calls the poll back with whichever of its events are ready, as a signal says
// that the queue changed but not how. Platforms call it on each wake.
void
bare_ipc__dispatch(bare_ipc_t *ipc);

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_IPC_H
