#import <assert.h>
#import <stdlib.h>

#import "ipc.h"

static const void *const bare_ipc_poll_queue = &bare_ipc_poll_queue;

int
bare_ipc_poll_alloc(bare_ipc_poll_t **result) {
  bare_ipc_poll_t *poll = malloc(sizeof(bare_ipc_poll_t));

  if (poll == NULL) return -1;

  *result = poll;

  return 0;
}

int
bare_ipc_poll_init(bare_ipc_poll_t *poll, bare_ipc_t *ipc) {
  dispatch_queue_t queue = dispatch_queue_create("to.holepunch.bare.kit.ipc", DISPATCH_QUEUE_SERIAL);

  dispatch_queue_set_specific(queue, bare_ipc_poll_queue, (void *) bare_ipc_poll_queue, NULL);

  dispatch_source_t reader = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, ipc->incoming, 0, queue);

  dispatch_source_t writer = dispatch_source_create(DISPATCH_SOURCE_TYPE_WRITE, ipc->outgoing, 0, queue);

  dispatch_source_set_cancel_handler(reader, ^{
    dispatch_release(reader);
  });

  dispatch_source_set_cancel_handler(writer, ^{
    dispatch_release(writer);
  });

  dispatch_source_set_event_handler(reader, ^{
    bare_ipc_poll_cb cb = atomic_load(&poll->cb);

    if (cb == NULL) return;

    @autoreleasepool {
      cb(poll, bare_ipc_readable);
    }
  });

  dispatch_source_set_event_handler(writer, ^{
    bare_ipc_poll_cb cb = atomic_load(&poll->cb);

    if (cb == NULL) return;

    @autoreleasepool {
      cb(poll, bare_ipc_writable);
    }
  });

  poll->ipc = ipc;
  poll->events = 0;
  poll->queue = queue;
  poll->reader = reader;
  poll->writer = writer;

  atomic_init(&poll->cb, NULL);

  return 0;
}

void
bare_ipc_poll_destroy(bare_ipc_poll_t *poll) {
  int err;
  err = bare_ipc_poll_stop(poll);
  assert(err == 0);

  dispatch_resume(poll->reader);
  dispatch_source_cancel(poll->reader);

  dispatch_resume(poll->writer);
  dispatch_source_cancel(poll->writer);

  if (dispatch_get_specific(bare_ipc_poll_queue) == NULL) {
    dispatch_semaphore_t done = dispatch_semaphore_create(0);

    dispatch_async(poll->queue, ^{
      dispatch_semaphore_signal(done);
    });

    dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
  }

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

  poll->events = events;

  atomic_store(&poll->cb, cb);

  return 0;
}

int
bare_ipc_poll_stop(bare_ipc_poll_t *poll) {
  if ((poll->events & bare_ipc_readable) != 0) dispatch_suspend(poll->reader);
  if ((poll->events & bare_ipc_writable) != 0) dispatch_suspend(poll->writer);

  poll->events = 0;

  atomic_store(&poll->cb, NULL);

  return 0;
}
