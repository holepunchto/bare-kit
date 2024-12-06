#ifndef BARE_KIT_IPC_H
#define BARE_KIT_IPC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

typedef struct bare_ipc_s bare_ipc_t;
typedef struct bare_ipc_msg_s bare_ipc_msg_t;

struct bare_ipc_s {
  void *context;
  void *socket;

  void *data;
};

struct bare_ipc_msg_s {
  unsigned char _[64] __attribute__((aligned(sizeof(void *))));
};

enum {
  bare_ipc_would_block = -1,
  bare_ipc_error = -2,
};

int
bare_ipc_init(bare_ipc_t *ipc, const char *endpoint);

void
bare_ipc_destroy(bare_ipc_t *ipc);

int
bare_ipc_fd(bare_ipc_t *ipc);

int
bare_ipc_read(bare_ipc_t *ipc, bare_ipc_msg_t *msg, void **data, size_t *len);

int
bare_ipc_write(bare_ipc_t *ipc, bare_ipc_msg_t *msg, const void *data, size_t len);

void
bare_ipc_release(bare_ipc_msg_t *msg);

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_IPC_H
