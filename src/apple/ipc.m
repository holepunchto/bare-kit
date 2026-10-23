#import <assert.h>
#import <dispatch/dispatch.h>
#import <stdlib.h>

#import "ipc.h"

static const void *const bare_ipc_signal_queue = &bare_ipc_signal_queue;

typedef struct {
  dispatch_queue_t queue;
  dispatch_source_t source;
} bare_ipc_signal_t;

// Runs the block in order with the handlers, waiting for any running handler to
// finish first, unless called from that handler.
static void
bare_ipc__sync(bare_ipc_signal_t *signal, dispatch_block_t block) {
  if (dispatch_get_specific(bare_ipc_signal_queue) == signal) block();
  else dispatch_sync(signal->queue, block);
}

int
bare_ipc__signal_init(bare_ipc_t *ipc) {
  bare_ipc_signal_t *signal = malloc(sizeof(bare_ipc_signal_t));

  if (signal == NULL) return -1;

  dispatch_queue_t queue = dispatch_queue_create("to.holepunch.bare.kit.ipc", DISPATCH_QUEUE_SERIAL);

  dispatch_queue_set_specific(queue, bare_ipc_signal_queue, (void *) signal, NULL);

  dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_DATA_OR, 0, 0, queue);

  dispatch_source_set_cancel_handler(source, ^{
    dispatch_release(source);
  });

  dispatch_source_set_event_handler(source, ^{
    @autoreleasepool {
      bare_ipc__dispatch(ipc);
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

  bare_ipc__sync(signal, ^{
                 });

  dispatch_release(signal->queue);

  free(signal);

  ipc->signal = NULL;
}

int
bare_ipc__poll_init(bare_ipc_poll_t *poll, bare_ipc_t *ipc) {
  poll->ipc = ipc;
  poll->data = NULL;

  atomic_init(&poll->events, 0);
  atomic_init(&poll->cb, NULL);

  bare_ipc__sync((bare_ipc_signal_t *) ipc->signal, ^{
    ipc->poll = poll;
  });

  return 0;
}

void
bare_ipc__poll_destroy(bare_ipc_poll_t *poll) {
  int err;
  err = bare_ipc_poll_stop(poll);
  assert(err == 0);

  bare_ipc_t *ipc = poll->ipc;

  // So the caller may free the poll once this returns.
  bare_ipc__sync((bare_ipc_signal_t *) ipc->signal, ^{
    ipc->poll = NULL;
  });
}

int
bare_ipc_poll_start(bare_ipc_poll_t *poll, int events, bare_ipc_poll_cb cb) {
  if (events == 0) return bare_ipc_poll_stop(poll);

  poll->events = events;

  atomic_store(&poll->cb, cb);

  // Level-triggered: signal once in case the events are already ready.
  bare_ipc__signal(poll->ipc);

  return 0;
}

int
bare_ipc_poll_stop(bare_ipc_poll_t *poll) {
  poll->events = 0;

  atomic_store(&poll->cb, NULL);

  return 0;
}
