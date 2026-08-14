#import <assert.h>
#import <stdlib.h>

#import "ipc.h"

static const void *const bare_ipc_poll_queue = &bare_ipc_poll_queue;

// Waits for the serial queue to reach the current point, unless we are already
// on it, in which case the handler is this call and there is nothing to wait for.
static void
bare_ipc__drain(dispatch_queue_t queue) {
  if (dispatch_get_specific(bare_ipc_poll_queue) != NULL) return;

  dispatch_semaphore_t done = dispatch_semaphore_create(0);

  dispatch_async(queue, ^{
    dispatch_semaphore_signal(done);
  });

  dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
}

typedef struct {
  dispatch_queue_t queue;
  dispatch_source_t source;
} bare_ipc_signal_t;

int
bare_ipc__signal_init(bare_ipc_t *ipc) {
  bare_ipc_signal_t *signal = malloc(sizeof(bare_ipc_signal_t));

  if (signal == NULL) return -1;

  dispatch_queue_t queue = dispatch_queue_create("to.holepunch.bare.kit.ipc", DISPATCH_QUEUE_SERIAL);

  dispatch_queue_set_specific(queue, bare_ipc_poll_queue, (void *) bare_ipc_poll_queue, NULL);

  // A manual source the queue signal triggers; the handler reports whichever
  // requested events are actually ready of whichever poll is subscribed.
  dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_DATA_OR, 0, 0, queue);

  dispatch_source_set_cancel_handler(source, ^{
    dispatch_release(source);
  });

  dispatch_source_set_event_handler(source, ^{
    bare_ipc_poll_t *poll = ipc->poll;

    if (poll == NULL) return;

    bare_ipc_poll_cb cb = atomic_load(&poll->cb);

    if (cb == NULL) return;

    int ready = bare_ipc__ready(ipc, poll->events);

    if (ready == 0) return;

    @autoreleasepool {
      cb(poll, ready);
    }
  });

  signal->queue = queue;
  signal->source = source;

  ipc->signal = signal;

  dispatch_resume(source);

  return 0;
}

void
bare_ipc__signal(bare_ipc_t *ipc) {
  bare_ipc_signal_t *signal = (bare_ipc_signal_t *) ipc->signal;

  dispatch_source_merge_data(signal->source, 1);
}

void
bare_ipc__signal_destroy(bare_ipc_t *ipc) {
  bare_ipc_signal_t *signal = (bare_ipc_signal_t *) ipc->signal;

  dispatch_source_cancel(signal->source);

  bare_ipc__drain(signal->queue);

  dispatch_release(signal->queue);

  free(signal);

  ipc->signal = NULL;
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
  poll->data = NULL;

  atomic_init(&poll->cb, NULL);

  ipc->poll = poll;

  return 0;
}

void
bare_ipc_poll_destroy(bare_ipc_poll_t *poll) {
  bare_ipc_t *ipc = poll->ipc;

  atomic_store(&poll->cb, NULL);

  ipc->poll = NULL;

  bare_ipc_signal_t *signal = (bare_ipc_signal_t *) ipc->signal;

  // Let any handler already running finish before the caller frees the poll.
  bare_ipc__drain(signal->queue);
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

  poll->events = events;

  atomic_store(&poll->cb, cb);

  // Level-trigger: re-check immediately in case data or space is already ready.
  bare_ipc__signal(poll->ipc);

  return 0;
}

int
bare_ipc_poll_stop(bare_ipc_poll_t *poll) {
  poll->events = 0;

  atomic_store(&poll->cb, NULL);

  return 0;
}
