#ifndef BARE_KIT_IPC_H
#define BARE_KIT_IPC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "queue.h"
#include "worklet.h"

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
  bare_queue_port_t *port;
  bare_ipc_poll_t *poll;

  // The primitive that hops a queue change onto the host run loop. Owned here,
  // for the lifetime of the port, because the worklet signals through it: a poll
  // comes and goes underneath.
  void *signal;
};

// Per platform. `signal` runs on the worklet thread, so it may only touch state
// owned by the ipc.
int
bare_ipc__signal_init(bare_ipc_t *ipc);

void
bare_ipc__signal_destroy(bare_ipc_t *ipc);

void
bare_ipc__signal(bare_ipc_t *ipc);

// Which of the events a poll asked for are ready right now. A wake says the
// queue changed, not how, so the platform asks after each one.
int
bare_ipc__ready(bare_ipc_t *ipc, int events);

#if defined(BARE_KIT_DARWIN) || defined(BARE_KIT_IOS)
#include "apple/ipc.h"
#endif

#if defined(BARE_KIT_ANDROID)
#include "android/ipc.h"
#endif

// Neither Linux nor Windows hands us a host run loop to post to, so both wake on
// a loop of their own.
#if defined(BARE_KIT_LINUX) || defined(BARE_KIT_WINDOWS)
#include "uv/ipc.h"
#endif

int
bare_ipc_alloc(bare_ipc_t **result);

int
bare_ipc_init(bare_ipc_t *ipc, bare_worklet_t *worklet);

// Retires the host end. Must be called before `bare_worklet_destroy`: the queue
// belongs to the worklet, which may free it as soon as the host has signalled it
// to be destroyed. A host that keeps an ipc must therefore keep its worklet.
void
bare_ipc_destroy(bare_ipc_t *ipc);

int
bare_ipc_read(bare_ipc_t *ipc, void **data, size_t *len);

int
bare_ipc_write(bare_ipc_t *ipc, const void *data, size_t len);

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
