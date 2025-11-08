#ifndef BARE_KIT_WINDOWS_UNISTD_H
#define BARE_KIT_WINDOWS_UNISTD_H
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <io.h>

#define close  _close
#define dup    _dup
#define strdup _strdup

#define read(fd, buf, count) ({ \
  ssize_t res = 0; \
  DWORD err; \
  HANDLE handle = (HANDLE) _get_osfhandle(fd); \
  assert(handle != INVALID_HANDLE_VALUE); \
  OVERLAPPED overlapped = {0, 0, 0, 0, 0}; \
  BOOL status = ReadFile(handle, buf, count, NULL, &overlapped); \
  HANDLE_ASYNC_IO \
  res; \
})

#define write(fd, buf, count) ({ \
  ssize_t res = 0; \
  DWORD err; \
  HANDLE handle = (HANDLE) _get_osfhandle(fd); \
  assert(handle != INVALID_HANDLE_VALUE); \
  OVERLAPPED overlapped = {0, 0, 0, 0, 0}; \
  BOOL status = WriteFile(handle, buf, count, NULL, &overlapped); \
  HANDLE_ASYNC_IO \
  res; \
})

#define HANDLE_ASYNC_IO \
  if (status) { \
    status = GetOverlappedResult(handle, &overlapped, (unsigned long *) &res, FALSE); \
\
    if (!status) { \
      err = GetLastError(); \
      res = -1; \
\
      if (err == ERROR_IO_INCOMPLETE) { \
        assert(_set_errno(EWOULDBLOCK) == 0); \
      } else { \
        assert(_set_errno(EIO) == 0); \
      } \
\
      CancelIo(handle); \
    } \
  } else { \
    err = GetLastError(); \
    res = -1; \
\
    if (err == ERROR_IO_PENDING) { \
      assert(_set_errno(EWOULDBLOCK) == 0); \
    } else { \
      assert(_set_errno(EIO) == 0); \
    } \
\
    CancelIo(handle); \
  }

#endif // BARE_KIT_WINDOWS_UNISTD_H
