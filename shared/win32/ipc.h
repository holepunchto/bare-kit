#ifndef BARE_KIT_WIN32_IPC_H
#define BARE_KIT_WIN32_IPC_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <uv.h>
#include <windows.h>

#include "../ipc.h"

struct bare_ipc_s {
  int incoming;
  int outgoing;

  struct {
    char base[BARE_IPC_READ_BUFFER_SIZE];
    size_t len;
  } read_buffer;

  uv_buf_t write_buffer;

  struct {
    uv_pipe_t incoming;
    uv_pipe_t outgoing;
  } pipe;

  uv_loop_t loop;
  uv_async_t close;
  uv_async_t read;
  uv_async_t write;

  SYNCHRONIZATION_BARRIER ready;
  HANDLE reading;
  HANDLE writing;
  HANDLE thread;

  bare_ipc_poll_t *poll;
};

struct bare_ipc_poll_s {
  bare_ipc_t *ipc;

  int events;

  bare_ipc_poll_cb cb;

  void *data;
};

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_WIN32_IPC_H
