#ifndef BARE_KIT_WINDOWS_UNISTD_H
#define BARE_KIT_WINDOWS_UNISTD_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <io.h>

#define close  _close
#define dup    _dup
#define strdup _strdup

#define read(fd, buf, count) ({ \
  INT res = -1; \
  HANDLE handle = (HANDLE) _get_osfhandle(fd); \
  if (handle == INVALID_HANDLE_VALUE) { \
    _set_errno(EBADF); \
  } else { \
    if (ReadFile(handle, buf, count, NULL, &ipc->overlapped.incoming) || GetLastError() == ERROR_IO_PENDING) { \
      if (!GetOverlappedResult(handle, &ipc->overlapped.incoming, (unsigned long *) &res, FALSE)) { \
        if (CancelIoEx(handle, &ipc->overlapped.incoming) || GetLastError() == ERROR_NOT_FOUND) { \
          if (!GetOverlappedResult(handle, &ipc->overlapped.incoming, (unsigned long *) &res, FALSE)) { \
            DWORD err = GetLastError(); \
            if (err == ERROR_OPERATION_ABORTED) { \
              _set_errno(EWOULDBLOCK) == 0; \
              res = -1; \
            } else if (err == ERROR_IO_INCOMPLETE) { \
              GetOverlappedResult(handle, &ipc->overlapped.incoming, (unsigned long *) &res, TRUE); \
            } else { \
              _set_errno(EIO); \
            } \
          } \
        } else { \
          _set_errno(EIO); \
        } \
      } \
    } else { \
      _set_errno(EIO); \
    } \
  } \
  res; \
})

#define write(fd, buf, count) ({ \
  INT res = -1; \
  HANDLE handle = (HANDLE) _get_osfhandle(fd); \
  if (handle == INVALID_HANDLE_VALUE) { \
    _set_errno(EBADF); \
  } else { \
    if (WriteFile(handle, buf, min(count, BARE_IPC_WRITE_CHUNK_SIZE), NULL, &ipc->overlapped.outgoing) || GetLastError() == ERROR_IO_PENDING) { \
      if (!GetOverlappedResult(handle, &ipc->overlapped.outgoing, (unsigned long *) &res, FALSE)) { \
        if (CancelIoEx(handle, &ipc->overlapped.outgoing) || GetLastError() == ERROR_NOT_FOUND) { \
          if (!GetOverlappedResult(handle, &ipc->overlapped.outgoing, (unsigned long *) &res, FALSE)) { \
            DWORD err = GetLastError(); \
            if (err == ERROR_OPERATION_ABORTED) { \
              _set_errno(EWOULDBLOCK); \
              res = -1; \
            } else if (err == ERROR_IO_INCOMPLETE) { \
              GetOverlappedResult(handle, &ipc->overlapped.outgoing, (unsigned long *) &res, TRUE); \
            } else { \
              _set_errno(EIO); \
            } \
          } \
        } else { \
          _set_errno(EIO); \
        } \
      } \
    } else { \
      _set_errno(EIO); \
    } \
  } \
  res; \
})

#endif // BARE_KIT_WINDOWS_UNISTD_H
