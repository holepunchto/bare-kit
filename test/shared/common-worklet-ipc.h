#include <assert.h>

#include "../../shared/ipc.h"
#include "../../shared/worklet.h"

#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

typedef struct bare_kit_context_s bare_kit_context_t;
typedef void (*bare_kit_read_cb)(bare_kit_context_t *context, const char *data, size_t len);
typedef void (*bare_kit_write_cb)(bare_kit_context_t *context);

struct bare_kit_context_s {
  bare_worklet_t worklet;
  bare_ipc_t ipc;
  bare_ipc_poll_t poll;

  void (*readable)(bare_kit_context_t *);
  void (*writable)(bare_kit_context_t *);

  bare_kit_read_cb pending_read;
  bare_kit_write_cb pending_write;

  char *data;
  size_t len;
};

static void
bare_kit_update(bare_kit_context_t *context);
static void
bare_kit_read(bare_kit_context_t *context, bare_kit_read_cb cb);
static void
bare_kit_write(bare_kit_context_t *context, char *data, size_t len, bare_kit_write_cb cb);
static void
bare_kit__readable(bare_kit_context_t *context);
static void
bare_kit__writable(bare_kit_context_t *context);

static void
bare_kit__poll(bare_ipc_poll_t *poll, int events) {
  bare_kit_context_t *context = (bare_kit_context_t *) poll->data;

  if ((events & bare_ipc_readable) != 0) context->readable(context);
  if ((events & bare_ipc_writable) != 0) context->writable(context);
}

static void
bare_kit_update(bare_kit_context_t *context) {
  int events = 0;

  if (context->readable) events |= bare_ipc_readable;
  if (context->writable) events |= bare_ipc_writable;

  int err;

  if (events) {
    err = bare_ipc_poll_start(&context->poll, events, bare_kit__poll);
  } else {
    err = bare_ipc_poll_stop(&context->poll);
  }
  assert(err == 0);
}

static void
bare_kit__readable(bare_kit_context_t *context) {
  void *data;
  size_t len;
  int err = bare_ipc_read(&context->ipc, &data, &len);
  assert(err == 0 || err == bare_ipc_would_block);

  if (err != bare_ipc_would_block) {
    bare_kit_read_cb cb = context->pending_read;
    context->pending_read = NULL;
    context->readable = NULL;
    bare_kit_update(context);
    cb(context, data, len);
  }
}

static void
bare_kit_read(bare_kit_context_t *context, bare_kit_read_cb cb) {
  void *data;
  size_t len;
  int err = bare_ipc_read(&context->ipc, &data, &len);
  assert(err == 0 || err == bare_ipc_would_block);

  if (err != bare_ipc_would_block) {
    cb(context, data, len);
  } else {
    context->pending_read = cb;
    context->readable = bare_kit__readable;
    bare_kit_update(context);
  }
}

static void
bare_kit__writable(bare_kit_context_t *context) {
  int err = bare_ipc_write(&context->ipc, context->data, context->len);
  assert(err >= 0 || err == bare_ipc_would_block);

  if (err == context->len) {
    bare_kit_write_cb cb = context->pending_write;
    context->data = NULL;
    context->len = 0;
    context->pending_write = NULL;
    context->writable = NULL;
    bare_kit_update(context);
    cb(context);
  } else {
    context->data = &context->data[max(err, 0)];
    context->len -= max(err, 0);
  }
}

static void
bare_kit_write(bare_kit_context_t *context, char *data, size_t len, bare_kit_write_cb cb) {
  int err = bare_ipc_write(&context->ipc, data, len);
  assert(err >= 0 || err == bare_ipc_would_block);

  if (err == context->len) {
    cb(context);
  } else {
    context->data = &data[max(err, 0)];
    context->len = len - max(err, 0);
    context->pending_write = cb;
    context->writable = bare_kit__writable;
    bare_kit_update(context);
  }
}
