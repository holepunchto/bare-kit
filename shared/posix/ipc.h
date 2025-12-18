#ifndef BARE_KIT_POSIX_IPC_H
#define BARE_KIT_POSIX_IPC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../ipc.h"

struct bare_ipc_s {
  int incoming;
  int outgoing;

  char data[BARE_IPC_READ_BUFFER_SIZE];
};

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_POSIX_IPC_H
