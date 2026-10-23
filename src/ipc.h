#ifndef BARE_KIT_IPC_H
#define BARE_KIT_IPC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <bare-kit.h>
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

// Implemented per platform. `bare_ipc__signal` may run on the worklet thread, so
// it may only touch state owned by the ipc.
int
bare_ipc__signal_init(bare_ipc_t *ipc);

void
bare_ipc__signal_destroy(bare_ipc_t *ipc);

void
bare_ipc__signal(bare_ipc_t *ipc);

// Calls the poll back with whichever of its events are ready, as a signal says
// that the queue changed but not how. Platforms call it on each wake.
void
bare_ipc__dispatch(bare_ipc_t *ipc);

#if defined(BARE_KIT_DARWIN) || defined(BARE_KIT_IOS)
#include "apple/ipc.h"
#endif

// Polls run on a thread of their own rather than a host run loop, so that their
// callbacks are called the same way on every platform.
#if defined(BARE_KIT_ANDROID) || defined(BARE_KIT_LINUX) || defined(BARE_KIT_WINDOWS)
#include "standalone/ipc.h"
#endif

// Implemented per platform, and called by `bare_ipc_poll_create()` and
// `bare_ipc_poll_destroy()`, which own the memory of the poll.
int
bare_ipc__poll_init(bare_ipc_poll_t *poll, bare_ipc_t *ipc);

void
bare_ipc__poll_destroy(bare_ipc_poll_t *poll);

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_IPC_H
