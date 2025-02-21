#ifndef BARE_KIT_WORKLET_H
#define BARE_KIT_WORKLET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <bare.h>
#include <js.h>
#include <utf.h>
#include <uv.h>

typedef struct bare_worklet_s bare_worklet_t;
typedef struct bare_worklet_options_s bare_worklet_options_t;
typedef struct bare_worklet_push_s bare_worklet_push_t;

typedef void (*bare_worklet_push_cb)(bare_worklet_push_t *, const char *error, const uv_buf_t *reply);

struct bare_worklet_options_s {
  /**
   * The memory limit of each JavaScript heap. By default, the limit will be
   * inferred based on the amount of physical memory of the device.
   *
   * Note that the limit applies individually to each thread, including the
   * main thread.
   */
  size_t memory_limit;

  /**
   * The directory in which assets should be stored. The worklet require both
   * read and write access to the directory. By default, assets may not be
   * referenced and doing so will result in a runtime error.
   */
  const char *assets;
};

struct bare_worklet_s {
  bare_t *bare;

  bare_worklet_options_t options;

  const char *filename;
  const uv_buf_t *source;

  int argc;
  const char **argv;

  uv_thread_t thread;
  uv_sem_t ready;
  uv_mutex_t lock;
  uv_async_t signal;

  uv_file incoming;
  uv_file outgoing;

  js_threadsafe_function_t *push;

  void *data;
};

struct bare_worklet_push_s {
  bare_worklet_t *worklet;

  bare_worklet_push_cb cb;

  uv_buf_t payload;

  void *data;
};

/**
 * Enable trade-off of performance for memory. Must be configured before
 * starting the first worklet.
 */
int
bare_worklet_optimize_for_memory(bool enabled);

int
bare_worklet_init(bare_worklet_t *worklet, const bare_worklet_options_t *options);

void
bare_worklet_destroy(bare_worklet_t *worklet);

int
bare_worklet_start(bare_worklet_t *worklet, const char *filename, const uv_buf_t *source, int argc, const char *argv[]);

int
bare_worklet_suspend(bare_worklet_t *worklet, int linger);

int
bare_worklet_resume(bare_worklet_t *worklet);

int
bare_worklet_terminate(bare_worklet_t *worklet);

int
bare_worklet_push(bare_worklet_t *worklet, bare_worklet_push_t *req, const uv_buf_t *payload, bare_worklet_push_cb cb);

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_WORKLET_H
