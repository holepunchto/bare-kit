#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <assert.h>
#include <io.h>

#include "ipc.h"

enum {
  bare_ipc_poll__close,
  bare_ipc_poll__start,
};

DWORD WINAPI
bare_ipc_poll__readable(LPVOID lpParameter) {
  bare_ipc_poll_t *poll = (bare_ipc_poll_t *) lpParameter;
  HANDLE handle = (HANDLE) _get_osfhandle(poll->ipc->incoming);
  assert(handle != INVALID_HANDLE_VALUE);
  DWORD transferred;

  while (WAIT_OBJECT_0 < WaitForMultipleObjects(BARE_IPC_POLL_NUM_EVENTS, poll->reader.events, FALSE, INFINITE)) {
    if (ReadFile(handle, NULL, 0, NULL, &poll->ipc->overlapped.incoming) || GetLastError() == ERROR_IO_PENDING) {
      if (GetOverlappedResult(handle, &poll->ipc->overlapped.incoming, &transferred, TRUE)) {
        if (poll->cb) poll->cb(poll, bare_ipc_readable);
      }
    }
  }

  ResetEvent(poll->reader.events[bare_ipc_poll__close]);

  return 0;
}

DWORD WINAPI
bare_ipc_poll__writable(LPVOID lpParameter) {
  bare_ipc_poll_t *poll = (bare_ipc_poll_t *) lpParameter;

  while (WAIT_OBJECT_0 < WaitForMultipleObjects(BARE_IPC_POLL_NUM_EVENTS, poll->writer.events, FALSE, INFINITE)) {
    Sleep(16);
    if (poll->cb) poll->cb(poll, bare_ipc_writable);
  }

  ResetEvent(poll->writer.events[bare_ipc_poll__close]);

  return 0;
}

int
bare_ipc_poll_alloc(bare_ipc_poll_t **result) {
  bare_ipc_poll_t *poll = malloc(sizeof(bare_ipc_poll_t));

  if (poll == NULL) return -1;

  *result = poll;

  return 0;
}

int
bare_ipc_poll_init(bare_ipc_poll_t *poll, bare_ipc_t *ipc) {
  poll->ipc = ipc;
  poll->events = 0;
  poll->cb = NULL;

  for (int i = 0; i < BARE_IPC_POLL_NUM_EVENTS; i++) {
    poll->reader.events[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
    assert(poll->reader.events[i] != NULL);

    poll->writer.events[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
    assert(poll->writer.events[i] != NULL);
  }

  poll->reader.thread = CreateThread(NULL, 0, bare_ipc_poll__readable, (LPVOID) poll, 0, NULL);
  assert(poll->reader.thread != NULL);

  poll->writer.thread = CreateThread(NULL, 0, bare_ipc_poll__writable, (LPVOID) poll, 0, NULL);
  assert(poll->writer.thread != NULL);

  return 0;
}

void
bare_ipc_poll_destroy(bare_ipc_poll_t *poll) {
  int err;
  err = bare_ipc_poll_stop(poll);
  assert(err == 0);

  SetEvent(poll->reader.events[bare_ipc_poll__close]);
  SetEvent(poll->writer.events[bare_ipc_poll__close]);

  HANDLE threads[2] = {poll->reader.thread, poll->writer.thread};
  DWORD res = WaitForMultipleObjects(2, threads, TRUE, INFINITE);
  assert(res == WAIT_OBJECT_0);

  poll->reader.thread = NULL;
  poll->writer.thread = NULL;

  for (int i = 0; i < 2; i++) {
    CloseHandle(threads[i]);
  }

  for (int i = 0; i < BARE_IPC_POLL_NUM_EVENTS; i++) {
    CloseHandle(poll->reader.events[i]);
    poll->reader.events[i] = NULL;
  }

  for (int i = 0; i < BARE_IPC_POLL_NUM_EVENTS; i++) {
    CloseHandle(poll->writer.events[i]);
    poll->writer.events[i] = NULL;
  }
}

void *
bare_ipc_poll_get_data(bare_ipc_poll_t *poll) {
  return poll->data;
}

void
bare_ipc_poll_set_data(bare_ipc_poll_t *poll, void *data) {
  poll->data = data;
}

bare_ipc_t *
bare_ipc_poll_get_ipc(bare_ipc_poll_t *poll) {
  return poll->ipc;
}

int
bare_ipc_poll_start(bare_ipc_poll_t *poll, int events, bare_ipc_poll_cb cb) {
  if (events == 0) return bare_ipc_poll_stop(poll);

  if ((events & bare_ipc_readable) == 0) {
    if ((poll->events & bare_ipc_readable) != 0) {
      ResetEvent(poll->reader.events[bare_ipc_poll__start]);
    }
  } else {
    if ((poll->events & bare_ipc_readable) == 0) {
      SetEvent(poll->reader.events[bare_ipc_poll__start]);
    }
  }

  if ((events & bare_ipc_writable) == 0) {
    if ((poll->events & bare_ipc_writable) != 0) {
      ResetEvent(poll->writer.events[bare_ipc_poll__start]);
    }
  } else {
    if ((poll->events & bare_ipc_writable) == 0) {
      SetEvent(poll->writer.events[bare_ipc_poll__start]);
    }
  }

  poll->events = events;
  poll->cb = cb;

  return 0;
}

int
bare_ipc_poll_stop(bare_ipc_poll_t *poll) {
  ResetEvent(poll->reader.events[bare_ipc_poll__start]);
  ResetEvent(poll->writer.events[bare_ipc_poll__start]);

  poll->events = 0;
  poll->cb = NULL;

  return 0;
}
