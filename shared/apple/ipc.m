#import <assert.h>
#import <stdlib.h>

#import "ipc.h"

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

  poll->queue = dispatch_queue_create("to.holepunch.bare.kit.ipc", DISPATCH_QUEUE_SERIAL);

  poll->reader = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, ipc->incoming, 0, poll->queue);
  poll->writer = dispatch_source_create(DISPATCH_SOURCE_TYPE_WRITE, ipc->outgoing, 0, poll->queue);

  dispatch_source_set_event_handler(poll->reader, ^{
    if ((poll->events & bare_ipc_readable) == 0) return;

    @autoreleasepool {
      poll->cb(poll, bare_ipc_readable);
    }
  });

  dispatch_source_set_event_handler(poll->writer, ^{
    if ((poll->events & bare_ipc_writable) == 0) return;

    @autoreleasepool {
      poll->cb(poll, bare_ipc_writable);
    }
  });

  dispatch_source_set_cancel_handler(poll->reader, ^{
    dispatch_release(poll->reader);
  });

  dispatch_source_set_cancel_handler(poll->writer, ^{
    dispatch_release(poll->writer);
  });

  return 0;
}

void
bare_ipc_poll_destroy(bare_ipc_poll_t *poll) {
  int err;
  err = bare_ipc_poll_stop(poll);
  assert(err == 0);

  dispatch_source_cancel(poll->reader);
  dispatch_source_cancel(poll->writer);

  dispatch_release(poll->queue);
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
    if ((poll->events & bare_ipc_readable) != 0) dispatch_suspend(poll->reader);
  } else {
    if ((poll->events & bare_ipc_readable) == 0) dispatch_resume(poll->reader);
  }

  if ((events & bare_ipc_writable) == 0) {
    if ((poll->events & bare_ipc_writable) != 0) dispatch_suspend(poll->writer);
  } else {
    if ((poll->events & bare_ipc_writable) == 0) dispatch_resume(poll->writer);
  }

  poll->cb = cb;
  poll->events = events;

  return 0;
}

int
bare_ipc_poll_stop(bare_ipc_poll_t *poll) {
  if ((poll->events & bare_ipc_readable) != 0) dispatch_suspend(poll->reader);
  if ((poll->events & bare_ipc_writable) != 0) dispatch_suspend(poll->writer);

  poll->cb = NULL;
  poll->events = 0;

  return 0;
}
