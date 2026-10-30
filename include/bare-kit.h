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
   * Options that are specific to a platform. Only the member of the platform
   * that the worklet runs on is read. The size of the union is fixed by
   * `reserved`, so that fields added to the end of the options keep their
   * offsets as the members of the union grow.
   *
   * @since 0
   */
  union {
    /**
     * Options that apply on Android.
     */
    struct {
      /**
       * The Java virtual machine, as a `JavaVM *`, which is published to the
       * addons of the worklet under `bare.android.jvm.v1`. By default, the
       * virtual machine that loaded Bare Kit through `JNI_OnLoad()` is used, so
       * it must be given if Bare Kit is loaded by other means. There is only
       * one per process.
       */
      void *jvm;

      /**
       * The application context, as a JNI reference to an
       * `android.content.Context`, which is published to the addons of the
       * worklet under `bare.android.context.v1`. The worklet takes a global
       * reference of its own, so the reference need only be valid for the call.
       */
      void *context;
    } android;

    void *reserved[8];
  } platform;
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
 * Create a worklet. The options are copied and may be `NULL`. On Android, the
 * calling thread must be attached to the JVM if an application context is
 * given.
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

// Lifecycle callbacks, called on the worklet thread. They must be configured
// before starting the worklet.

/**
 * Called when the worklet suspends, with the number of milliseconds that it
 * lingers for before going idle.
 */
int
bare_worklet_on_suspend(bare_worklet_t *worklet, bare_worklet_suspend_cb cb, void *data);

/**
 * Called when a suspended worklet wakes up, with the number of milliseconds
 * that it has to perform background work before going idle again.
 */
int
bare_worklet_on_wakeup(bare_worklet_t *worklet, bare_worklet_wakeup_cb cb, void *data);

/**
 * Called when a suspended worklet goes idle, after which it does no work until
 * it is resumed or woken up.
 */
int
bare_worklet_on_idle(bare_worklet_t *worklet, bare_worklet_idle_cb cb, void *data);

/**
 * Called when a suspended worklet resumes.
 */
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

/**
 * Get the data that was associated with the worklet by
 * `bare_worklet_set_data()`.
 */
void *
bare_worklet_get_data(bare_worklet_t *worklet);

/**
 * Associate `data` with the worklet, for the host to retrieve with
 * `bare_worklet_get_data()`. The worklet does not use it.
 */
void
bare_worklet_set_data(bare_worklet_t *worklet, void *data);

/**
 * Start the worklet on a thread of its own, running `filename` with `argv` as
 * `Bare.argv`. If `source` is `NULL`, the contents of `filename` will instead
 * be read at runtime. The arguments need only remain valid until this function
 * returns.
 *
 * A relative `filename` names a resource of the application. On Apple
 * platforms, it resolves against the resources of the application bundle,
 * which extensions share with their application. On Android, it resolves
 * against the code cache directory of the application context, which must then
 * be given. The resources listed in `<name>.assets`, one `/<path>` per line,
 * are extracted there from `<name>/<path>` in the APK before `<name>.bundle`
 * first runs, and a `NULL` source is read from the APK.
 */
int
bare_worklet_start(bare_worklet_t *worklet, const char *filename, const void *source, size_t len, int argc, const char *argv[]);

// Suspending, resuming, waking up, or terminating a worklet is safe from any
// thread, and is a no-op if the worklet has not started or has torn down.

/**
 * Suspend the worklet, which lingers for `linger` milliseconds before going
 * idle. A negative `linger` lingers for 30 seconds. On iOS, the worklet holds a
 * background task for as long as it lingers, so `linger` is capped by the
 * background time remaining to the application, which a negative `linger`
 * lingers for instead.
 */
int
bare_worklet_suspend(bare_worklet_t *worklet, int linger);

/**
 * Resume the worklet. If it has not yet gone idle, the suspension is
 * cancelled.
 */
int
bare_worklet_resume(bare_worklet_t *worklet);

/**
 * Wake up a suspended worklet and give it `deadline` milliseconds to perform
 * background work before going idle again. A negative `deadline`, and the cap
 * on iOS, are handled as for the `linger` of `bare_worklet_suspend()`.
 */
int
bare_worklet_wakeup(bare_worklet_t *worklet, int deadline);

/**
 * Terminate the worklet as soon as possible. The exit callback is called once
 * it has torn down.
 */
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
 * End the host end, a half-close after which `Bare.IPC` in the worklet reads
 * end-of-stream once drained, while the host keeps reading. Writes fail
 * afterwards. Ending twice, or after closing, is a no-op.
 */
void
bare_ipc_end(bare_ipc_t *ipc);

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

/**
 * Stop and free the poll. If its callback is running on another thread, this
 * waits for the callback to return. May be called from the callback itself.
 */
void
bare_ipc_poll_destroy(bare_ipc_poll_t *poll);

/**
 * Get the data that was associated with the poll by
 * `bare_ipc_poll_set_data()`.
 */
void *
bare_ipc_poll_get_data(bare_ipc_poll_t *poll);

/**
 * Associate `data` with the poll, for the host to retrieve with
 * `bare_ipc_poll_get_data()`, such as from the callback.
 */
void
bare_ipc_poll_set_data(bare_ipc_poll_t *poll, void *data);

/**
 * Get the ipc that the poll was created for.
 */
bare_ipc_t *
bare_ipc_poll_get_ipc(bare_ipc_poll_t *poll);

/**
 * Start polling for `events`, replacing the events and callback of a poll that
 * is already started. Polls are level-triggered, so the callback is called
 * again for as long as the events remain ready.
 */
int
bare_ipc_poll_start(bare_ipc_poll_t *poll, int events, bare_ipc_poll_cb cb);

/**
 * Stop polling, after which the callback is no longer called. Starting a poll
 * with no `events` stops it too.
 */
int
bare_ipc_poll_stop(bare_ipc_poll_t *poll);

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_H
