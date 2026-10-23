#ifndef BARE_KIT_IPC_H
#define BARE_KIT_IPC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "queue.h"
#include "worklet.h"

#define BARE_IPC_READ_BUFFER_SIZE (64 * 1024)

typedef struct bare_ipc_s bare_ipc_t;
typedef struct bare_ipc_poll_s bare_ipc_poll_t;

typedef void (*bare_ipc_poll_cb)(bare_ipc_poll_t *, int events);

enum {
  bare_ipc_readable = 0x1,
  bare_ipc_writable = 0x2,
};

enum {
  bare_ipc_would_block = -1,
  bare_ipc_error = -2,
};

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

#if defined(BARE_KIT_ANDROID)
#include "android/ipc.h"
#endif

// Linux and Windows have no host run loop to post to, so they run their own.
#if defined(BARE_KIT_LINUX) || defined(BARE_KIT_WINDOWS)
#include "uv/ipc.h"
#endif

int
bare_ipc_alloc(bare_ipc_t **result);

// Fails if the worklet already has an ipc.
int
bare_ipc_init(bare_ipc_t *ipc, bare_worklet_t *worklet);

// Closes the host end, after which reads return EOF and writes fail. Closing
// twice is a no-op.
void
bare_ipc_close(bare_ipc_t *ipc);

// May be called before or after `bare_worklet_destroy`, and from a poll callback
// of the ipc itself.
void
bare_ipc_destroy(bare_ipc_t *ipc);

// Reads up to `len` bytes, which must not be 0, into the caller's `data`.
// Returns the number of bytes read, 0 at EOF, or `bare_ipc_would_block`.
int
bare_ipc_read(bare_ipc_t *ipc, void *data, size_t len);

int
bare_ipc_write(bare_ipc_t *ipc, const void *data, size_t len);

// On Android, a poll must be started, stopped and destroyed on the thread that
// initialized it, whose looper runs its callback.
int
bare_ipc_poll_alloc(bare_ipc_poll_t **result);

int
bare_ipc_poll_init(bare_ipc_poll_t *poll, bare_ipc_t *ipc);

void
bare_ipc_poll_destroy(bare_ipc_poll_t *poll);

void *
bare_ipc_poll_get_data(bare_ipc_poll_t *poll);

void
bare_ipc_poll_set_data(bare_ipc_poll_t *poll, void *data);

bare_ipc_t *
bare_ipc_poll_get_ipc(bare_ipc_poll_t *poll);

int
bare_ipc_poll_start(bare_ipc_poll_t *poll, int events, bare_ipc_poll_cb cb);

int
bare_ipc_poll_stop(bare_ipc_poll_t *poll);

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_IPC_H
