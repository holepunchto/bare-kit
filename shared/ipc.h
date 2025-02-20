#ifndef BARE_KIT_IPC_H
#define BARE_KIT_IPC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#define BARE_IPC_READ_BUFFER_SIZE 64 * 1024

typedef struct bare_ipc_s bare_ipc_t;

struct bare_ipc_s {
  int incoming;
  int outgoing;
  char data[BARE_IPC_READ_BUFFER_SIZE];
};

enum {
  bare_ipc_would_block = -1,
  bare_ipc_error = -2,
};

int
bare_ipc_init(bare_ipc_t *ipc, int incoming, int outgoing);

int
bare_ipc_read(bare_ipc_t *ipc, void **data, size_t *len);

int
bare_ipc_write(bare_ipc_t *ipc, const void *data, size_t len);

void
bare_ipc_destroy(bare_ipc_t *ipc);

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_IPC_H
