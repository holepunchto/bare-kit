#ifndef BARE_KIT_IPC_H
#define BARE_KIT_IPC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "worklet.h"

#if defined(BARE_KIT_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define BARE_IPC_WRITE_CHUNK_SIZE 64 * 1024
#endif

#define BARE_IPC_READ_BUFFER_SIZE 64 * 1024

typedef struct bare_ipc_s bare_ipc_t;
typedef struct bare_ipc_poll_s bare_ipc_poll_t;

typedef void (*bare_ipc_poll_cb)(bare_ipc_poll_t *, int events);

struct bare_ipc_s {
  int incoming;
  int outgoing;
  char data[BARE_IPC_READ_BUFFER_SIZE];

#if defined(BARE_KIT_WINDOWS)
  struct {
    OVERLAPPED incoming;
    OVERLAPPED outgoing;
  } overlapped;
#endif
};

enum {
  bare_ipc_readable = 0x1,
  bare_ipc_writable = 0x2,
};

enum {
  bare_ipc_would_block = -1,
  bare_ipc_error = -2,
};

#if defined(BARE_KIT_DARWIN) || defined(BARE_KIT_IOS)
#include "apple/ipc.h"
#endif

#if defined(BARE_KIT_ANDROID)
#include "android/ipc.h"
#endif

#if defined(BARE_KIT_LINUX)
#include "linux/ipc.h"
#endif

#if defined(BARE_KIT_WINDOWS)
#include "windows/ipc.h"
#endif

int
bare_ipc_alloc(bare_ipc_t **result);

int
bare_ipc_init(bare_ipc_t *ipc, bare_worklet_t *worklet);

void
bare_ipc_destroy(bare_ipc_t *ipc);

int
bare_ipc_get_incoming(bare_ipc_t *ipc);

int
bare_ipc_get_outgoing(bare_ipc_t *ipc);

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
