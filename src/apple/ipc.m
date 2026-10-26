#import <dispatch/dispatch.h>
#import <stdlib.h>

#import "../ipc.h"

static const void *const bare_ipc_signal_queue = &bare_ipc_signal_queue;

typedef struct {
  dispatch_queue_t queue;
  dispatch_source_t source;
} bare_ipc_signal_t;

int
bare_ipc__signal_init(bare_ipc_t *ipc) {
  bare_ipc_signal_t *signal = malloc(sizeof(bare_ipc_signal_t));

  if (signal == NULL) return -1;

  dispatch_queue_t queue = dispatch_queue_create("to.holepunch.bare.kit.ipc", DISPATCH_QUEUE_SERIAL);

  dispatch_queue_set_specific(queue, bare_ipc_signal_queue, (void *) signal, NULL);

  dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_DATA_OR, 0, 0, queue);

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

  dispatch_source_t source = signal->source;

  if (dispatch_get_specific(bare_ipc_signal_queue) == signal) {
    // A handler cannot wait for the cancellation it is itself part of.
    dispatch_source_cancel(source);
  } else {
    // Waited for, so that the handlers are released along with the source
    // rather than at some later point.
    dispatch_semaphore_t cancelled = dispatch_semaphore_create(0);

    dispatch_source_set_cancel_handler(source, ^{
      dispatch_semaphore_signal(cancelled);
    });

    dispatch_source_cancel(source);

    dispatch_semaphore_wait(cancelled, DISPATCH_TIME_FOREVER);

    dispatch_release(cancelled);
  }

  dispatch_release(source);

  dispatch_release(signal->queue);

  free(signal);

  ipc->signal = NULL;
}

void
bare_ipc__set_poll(bare_ipc_t *ipc, bare_ipc_poll_t *poll) {
  bare_ipc_signal_t *signal = (bare_ipc_signal_t *) ipc->signal;

  dispatch_block_t block = ^{
    ipc->poll = poll;
  };

  if (dispatch_get_specific(bare_ipc_signal_queue) == signal) block();
  else dispatch_sync(signal->queue, block);
}
