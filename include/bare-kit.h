#ifndef BARE_KIT_H
#define BARE_KIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

typedef struct bare_worklet_s bare_worklet_t;
typedef struct bare_worklet_options_s bare_worklet_options_t;
typedef struct bare_ipc_s bare_ipc_t;
typedef struct bare_ipc_poll_s bare_ipc_poll_t;

typedef void (*bare_worklet_suspend_cb)(int linger, void *data);
typedef void (*bare_worklet_wakeup_cb)(int deadline, void *data);
typedef void (*bare_worklet_idle_cb)(void *data);
typedef void (*bare_worklet_resume_cb)(void *data);
typedef void (*bare_worklet_exit_cb)(int exit_code, void *data);
typedef void (*bare_ipc_poll_cb)(bare_ipc_poll_t *, int events);

#define BARE_IPC_READ_BUFFER_SIZE (64 * 1024)

enum {
  bare_ipc_readable = 0x1,
  bare_ipc_writable = 0x2,
};

enum {
  bare_ipc_would_block = -1,
  bare_ipc_error = -2,
};

/** @version 0 */
struct bare_worklet_options_s {
  int version;

  /**
   * The memory limit of each JavaScript heap. By default, the limit will be
   * inferred based on the amount of physical memory of the device.
   *
   * Note that the limit applies individually to each thread, including the
   * main thread.
   *
   * @since 0
   */
  size_t memory_limit;

  /**
   * The directory in which assets should be stored. The worklet requires both
   * read and write access to the directory. By default, assets may not be
   * referenced and doing so will result in a runtime error.
   *
   * @since 0
   */
  const char *assets;
};

/**
 * Enable trade-off of performance for memory. Must be configured before
 * starting the first worklet.
 */
int
bare_worklet_optimize_for_memory(bool enabled);

/**
 * Tear down the state shared by the worklets of the process. Every worklet must
 * have been terminated or destroyed first, and those still tearing down are
 * waited for. Must be called at most once, after which starting a worklet
 * fails. A host that runs worklets for as long as it lives need not call it.
 */
int
bare_worklet_teardown(void);

/**
 * Create a worklet. The options are copied and may be `NULL`.
 */
int
bare_worklet_create(const bare_worklet_options_t *options, bare_worklet_t **result);

/**
 * Terminate the worklet if it is still running and hand it over to tear itself
 * down, without blocking. The worklet must not be used afterwards, but its ipc
 * may be destroyed before or after it.
 */
void
bare_worklet_destroy(bare_worklet_t *worklet);

/**
 * Lifecycle callbacks, called on the worklet thread. They must be configured
 * before starting the worklet.
 */
int
bare_worklet_on_suspend(bare_worklet_t *worklet, bare_worklet_suspend_cb cb, void *data);

int
bare_worklet_on_wakeup(bare_worklet_t *worklet, bare_worklet_wakeup_cb cb, void *data);

int
bare_worklet_on_idle(bare_worklet_t *worklet, bare_worklet_idle_cb cb, void *data);

int
bare_worklet_on_resume(bare_worklet_t *worklet, bare_worklet_resume_cb cb, void *data);

/**
 * Called exactly once on the worklet thread with the exit code of the worklet
 * once it has torn down, whether it exited on its own, was terminated, or was
 * destroyed, so `data` must be kept until then. An ipc of the worklet has
 * reached end-of-stream by then.
 */
int
bare_worklet_on_exit(bare_worklet_t *worklet, bare_worklet_exit_cb cb, void *data);

void *
bare_worklet_get_data(bare_worklet_t *worklet);

void
bare_worklet_set_data(bare_worklet_t *worklet, void *data);

/**
 * Start the worklet on a thread of its own, running `filename` with `argv` as
 * `Bare.argv`. If `source` is `NULL`, the contents of `filename` will instead
 * be read at runtime. The arguments need only remain valid until this function
 * returns.
 */
int
bare_worklet_start(bare_worklet_t *worklet, const char *filename, const void *source, size_t len, int argc, const char *argv[]);

/**
 * Suspending, resuming, waking up, or terminating a worklet that has not
 * started or has torn down is a no-op.
 */
int
bare_worklet_suspend(bare_worklet_t *worklet, int linger);

int
bare_worklet_resume(bare_worklet_t *worklet);

int
bare_worklet_wakeup(bare_worklet_t *worklet, int deadline);

int
bare_worklet_terminate(bare_worklet_t *worklet);

/**
 * Create the host end of the ipc of a worklet, which is exposed as `Bare.IPC`
 * in the worklet. Fails if the worklet already has an ipc or has been
 * terminated.
 */
int
bare_ipc_create(bare_worklet_t *worklet, bare_ipc_t **result);

/**
 * Close the host end, after which reads return end-of-stream and writes fail.
 * Closing twice is a no-op.
 */
void
bare_ipc_close(bare_ipc_t *ipc);

/**
 * Close and free the ipc. May be called before or after `bare_worklet_destroy()`,
 * and from a poll callback of the ipc itself.
 */
void
bare_ipc_destroy(bare_ipc_t *ipc);

/**
 * Read up to `len` bytes, which must not be 0, into `data`. Returns the number
 * of bytes read, 0 at end-of-stream, or `bare_ipc_would_block`.
 */
int
bare_ipc_read(bare_ipc_t *ipc, void *data, size_t len);

/**
 * Write `len` bytes from `data`. Returns the number of bytes written,
 * `bare_ipc_would_block` if there is no room, or `bare_ipc_error` once the ipc
 * is closed.
 */
int
bare_ipc_write(bare_ipc_t *ipc, const void *data, size_t len);

/**
 * Create a poll of the ipc, which calls back when it becomes readable or
 * writable. Callbacks run on a thread owned by the ipc, which on Android is
 * attached to the JVM, and may stop or destroy the poll or its ipc.
 */
int
bare_ipc_poll_create(bare_ipc_t *ipc, bare_ipc_poll_t **result);

void
bare_ipc_poll_destroy(bare_ipc_poll_t *poll);

void *
bare_ipc_poll_get_data(bare_ipc_poll_t *poll);

void
bare_ipc_poll_set_data(bare_ipc_poll_t *poll, void *data);

bare_ipc_t *
bare_ipc_poll_get_ipc(bare_ipc_poll_t *poll);

/**
 * Start polling for `events`. Polls are level-triggered, so the callback is
 * called again for as long as the events remain ready.
 */
int
bare_ipc_poll_start(bare_ipc_poll_t *poll, int events, bare_ipc_poll_cb cb);

int
bare_ipc_poll_stop(bare_ipc_poll_t *poll);

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_H
