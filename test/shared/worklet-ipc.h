#ifndef BARE_WORKLET_IPC_H
#define BARE_WORKLET_IPC_H

#include "../../shared/ipc.h"
#include "../../shared/worklet.h"

typedef struct bare_worklet_ipc_s bare_worklet_ipc_t;
typedef void (*bare_worklet_ipc_read_cb)(bare_worklet_ipc_t *ipc, ssize_t len, const char *data);
typedef void (*bare_worklet_ipc_write_cb)(bare_worklet_ipc_t *ipc, int status);

struct bare_worklet_ipc_s {
  bare_ipc_t ipc;
  bare_ipc_poll_t poll;

  bare_worklet_ipc_read_cb readable;
  bare_worklet_ipc_write_cb writable;

  char *buffer;
  size_t length;
  size_t offset;

  void *data;
};

static inline int
bare_worklet_ipc_alloc(bare_worklet_ipc_t **result);
static inline int
bare_worklet_ipc_init(bare_worklet_ipc_t *ipc, bare_worklet_t *worklet);
static inline void
bare_worklet_ipc_destroy(bare_worklet_ipc_t *ipc);
static inline void
bare_worklet_ipc_read(bare_worklet_ipc_t *ipc, bare_worklet_ipc_read_cb cb);
static inline void
bare_worklet_ipc_write(bare_worklet_ipc_t *ipc, const char *data, size_t len, bare_worklet_ipc_write_cb cb);
static void
bare_worklet_ipc__on_poll(bare_ipc_poll_t *poll, int events);
static inline void
bare_worklet_ipc__readable(bare_worklet_ipc_t *ipc);
static inline void
bare_worklet_ipc__writable(bare_worklet_ipc_t *ipc);
static inline void
bare_worklet_ipc__set_readable(bare_worklet_ipc_t *ipc, bare_worklet_ipc_read_cb cb);
static inline void
bare_worklet_ipc__set_writable(bare_worklet_ipc_t *ipc, bare_worklet_ipc_write_cb cb);
static inline void
bare_worklet_ipc__update(bare_worklet_ipc_t *ipc);

static void
bare_worklet_ipc__on_poll(bare_ipc_poll_t *poll, int events) {
  bare_worklet_ipc_t *ipc = (bare_worklet_ipc_t *) bare_ipc_poll_get_data(poll);

  if (events & bare_ipc_readable) bare_worklet_ipc__readable(ipc);
  if (events & bare_ipc_writable) bare_worklet_ipc__writable(ipc);
}

static inline int
bare_worklet_ipc_alloc(bare_worklet_ipc_t **result) {
  bare_worklet_ipc_t *ipc = malloc(sizeof(bare_worklet_ipc_t));

  if (ipc == NULL) {
    return -1;
  }

  *result = ipc;

  return 0;
}

static inline int
bare_worklet_ipc_init(bare_worklet_ipc_t *ipc, bare_worklet_t *worklet) {
  int err;

  err = bare_ipc_init(&ipc->ipc, worklet);
  assert(err == 0);

  err = bare_ipc_poll_init(&ipc->poll, &ipc->ipc);
  assert(err == 0);

  bare_ipc_poll_set_data(&ipc->poll, (void *) ipc);

  ipc->readable = NULL;
  ipc->writable = NULL;

  return 0;
}

static inline void
bare_worklet_ipc_destroy(bare_worklet_ipc_t *ipc) {
  bare_ipc_poll_destroy(&ipc->poll);
  bare_ipc_destroy(&ipc->ipc);
}

static inline void
bare_worklet_ipc__set_readable(bare_worklet_ipc_t *ipc, bare_worklet_ipc_read_cb cb) {
  ipc->readable = cb;
  bare_worklet_ipc__update(ipc);
}

static inline void
bare_worklet_ipc__set_writable(bare_worklet_ipc_t *ipc, bare_worklet_ipc_write_cb cb) {
  ipc->writable = cb;
  bare_worklet_ipc__update(ipc);
}

static inline void
bare_worklet_ipc__update(bare_worklet_ipc_t *ipc) {
  int events = 0;

  if (ipc->readable != NULL) events |= bare_ipc_readable;
  if (ipc->writable != NULL) events |= bare_ipc_writable;

  int err;

  if (events) {
    err = bare_ipc_poll_start(&ipc->poll, events, bare_worklet_ipc__on_poll);
  } else {
    err = bare_ipc_poll_stop(&ipc->poll);
  }
  assert(err == 0);
}

static inline void
bare_worklet_ipc__readable(bare_worklet_ipc_t *ipc) {
  char data[BARE_IPC_READ_BUFFER_SIZE];
  size_t len;
  int err = bare_ipc_read(&ipc->ipc, (void **) &data, &len);
  assert(err == 0 || err == bare_ipc_would_block);

  if (err != bare_ipc_would_block) {
    char *buffer = malloc(len);
    memcpy(buffer, data, len);
    bare_worklet_ipc_read_cb cb = ipc->readable;
    bare_worklet_ipc__set_readable(ipc, NULL);
    cb(ipc, len, buffer);
  }
}

static inline void
bare_worklet_ipc_read(bare_worklet_ipc_t *ipc, bare_worklet_ipc_read_cb cb) {
  char data[BARE_IPC_READ_BUFFER_SIZE];
  size_t len;
  int err = bare_ipc_read(&ipc->ipc, (void **) &data, &len);
  assert(err == 0 || err == bare_ipc_would_block);

  if (err != bare_ipc_would_block) {
    char *buffer = malloc(len);
    memcpy(buffer, data, len);
    cb(ipc, len, buffer);
  } else {
    bare_worklet_ipc__set_readable(ipc, cb);
  }
}

static inline void
bare_worklet_ipc__writable(bare_worklet_ipc_t *ipc) {
  int written = bare_ipc_write(&ipc->ipc, &ipc->buffer[ipc->offset], ipc->length - ipc->offset);
  assert(written >= 0 || written == bare_ipc_would_block);

  ipc->offset += written == bare_ipc_would_block ? 0 : written;

  if (ipc->offset == ipc->length) {
    free(ipc->buffer);
    ipc->length = 0;
    ipc->offset = 0;
    bare_worklet_ipc_write_cb cb = ipc->writable;
    bare_worklet_ipc__set_writable(ipc, NULL);
    cb(ipc, 0);
  }
}

static inline void
bare_worklet_ipc_write(bare_worklet_ipc_t *ipc, const char *data, size_t len, bare_worklet_ipc_write_cb cb) {
  int written = bare_ipc_write(&ipc->ipc, data, len);
  assert(written >= 0 || written == bare_ipc_would_block);

  if (written == len) {
    cb(ipc, 0);
  } else {
    ipc->buffer = malloc(len);
    ipc->length = len;
    ipc->offset = written;
    bare_worklet_ipc__set_writable(ipc, cb);
  }
}

#endif // BARE_WORKLET_IPC_H
