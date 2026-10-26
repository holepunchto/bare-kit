#include <assert.h>
#include <bare.h>
#include <js.h>
#include <rlimit.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#include "platform.h"
#include "suspension.h"
#include "worklet.bundle.h"
#include "worklet.h"

struct bare_worklet_state_s {
  bool finished;

  // Guards the suspension state as well as `bare`, which is cleared before the
  // worklet tears down.
  uv_mutex_t lock;

  bare_t *bare;

  bare_suspension_t suspension;

  bool suspended;
  bool resuming;

  struct {
    bare_worklet_suspend_cb suspend;
    void *suspend_data;

    bare_worklet_wakeup_cb wakeup;
    void *wakeup_data;

    bare_worklet_idle_cb idle;
    void *idle_data;

    bare_worklet_resume_cb resume;
    void *resume_data;

    bare_worklet_exit_cb exit;
    void *exit_data;
  } callbacks;
};

static uv_once_t bare_worklet__init_guard = UV_ONCE_INIT;

// The worklets of the process that have started but not yet torn down.
static uv_mutex_t bare_worklet__running_lock;
static uv_cond_t bare_worklet__running_idle;
static int bare_worklet__running = 0;
static bool bare_worklet__torn_down = false;

static void
bare_worklet__on_init(void) {
  int err;

  err = uv_mutex_init(&bare_worklet__running_lock);
  assert(err == 0);

  err = uv_cond_init(&bare_worklet__running_idle);
  assert(err == 0);

#ifdef SIGPIPE
  signal(SIGPIPE, SIG_IGN);
#endif

  err = rlimit_set(rlimit_open_files, rlimit_infer);
  assert(err == 0);
}

int
bare_worklet_create(const bare_worklet_options_t *options, bare_worklet_t **result) {
  uv_once(&bare_worklet__init_guard, bare_worklet__on_init);

  int err;

  bare_worklet_t *worklet = calloc(1, sizeof(bare_worklet_t));
  bare_worklet_state_t *state = calloc(1, sizeof(bare_worklet_state_t));
  bare_queue_t *queue = bare_queue_create();

  if (worklet == NULL || state == NULL || queue == NULL) {
    if (queue) bare_queue_destroy(queue);

    free(state);
    free(worklet);

    return -1;
  }

  err = uv_mutex_init(&state->lock);
  assert(err == 0);

  worklet->state = state;
  worklet->queue = queue;

  if (options) {
    worklet->options.memory_limit = options->memory_limit;

    if (options->assets) worklet->options.assets = strdup(options->assets);
  }

  *result = worklet;

  return 0;
}

void
bare_worklet_destroy(bare_worklet_t *worklet) {
  if (worklet->thread != 0) {
    // This must not block, as that causes an ANR on Android, so the detached
    // thread tears itself down.
    bare_worklet_terminate(worklet);
    uv_sem_post(worklet->finished);
  } else {
    // Never started, so the worklet's share of the queue is still ours. Closing
    // its end lets an open ipc see EOF.
    uv_mutex_destroy(&worklet->state->lock);

    free(worklet->state);

    bare_queue_close(&worklet->queue->uv_port);
    bare_queue_destroy(worklet->queue);
  }

  free((char *) worklet->options.assets);

  free(worklet);
}

int
bare_worklet_on_suspend(bare_worklet_t *worklet, bare_worklet_suspend_cb cb, void *data) {
  worklet->state->callbacks.suspend = cb;
  worklet->state->callbacks.suspend_data = data;

  return 0;
}

int
bare_worklet_on_wakeup(bare_worklet_t *worklet, bare_worklet_wakeup_cb cb, void *data) {
  worklet->state->callbacks.wakeup = cb;
  worklet->state->callbacks.wakeup_data = data;

  return 0;
}

int
bare_worklet_on_idle(bare_worklet_t *worklet, bare_worklet_idle_cb cb, void *data) {
  worklet->state->callbacks.idle = cb;
  worklet->state->callbacks.idle_data = data;

  return 0;
}

int
bare_worklet_on_resume(bare_worklet_t *worklet, bare_worklet_resume_cb cb, void *data) {
  worklet->state->callbacks.resume = cb;
  worklet->state->callbacks.resume_data = data;

  return 0;
}

int
bare_worklet_on_exit(bare_worklet_t *worklet, bare_worklet_exit_cb cb, void *data) {
  worklet->state->callbacks.exit = cb;
  worklet->state->callbacks.exit_data = data;

  return 0;
}

void *
bare_worklet_get_data(bare_worklet_t *worklet) {
  return worklet->data;
}

void
bare_worklet_set_data(bare_worklet_t *worklet, void *data) {
  worklet->data = data;
}

static js_platform_options_t bare_worklet__platform_options = {
  .version = 1,
};

int
bare_worklet_optimize_for_memory(bool enabled) {
  int err;
  err = uv_os_setenv("UV_THREADPOOL_SIZE", enabled ? "1" : "4");
  assert(err == 0);

  bare_worklet__platform_options.optimize_for_memory = enabled;

  return 0;
}

// Shared by the worklets of the process. It can only be created once, so it lives
// until `bare_worklet_teardown()`, which waits for every running worklet first.
static uv_once_t bare_worklet__platform_guard = UV_ONCE_INIT;
static uv_thread_t bare_worklet__platform_thread;
static uv_async_t bare_worklet__platform_shutdown;
static uv_barrier_t bare_worklet__platform_ready;
static js_platform_t *bare_worklet__platform;

static void
bare_worklet__on_platform_shutdown(uv_async_t *handle) {
  uv_close((uv_handle_t *) handle, NULL);
}

static void
bare_worklet__on_platform_thread(void *opaque) {
  int err;

  int argc = 0;
  char **argv = NULL;

  uv_setup_args(argc, argv);

  uv_loop_t loop;
  err = uv_loop_init(&loop);
  assert(err == 0);

  err = uv_async_init(&loop, &bare_worklet__platform_shutdown, bare_worklet__on_platform_shutdown);
  assert(err == 0);

  err = js_create_platform(&loop, &bare_worklet__platform_options, &bare_worklet__platform);
  assert(err == 0);

  uv_barrier_wait(&bare_worklet__platform_ready);

  err = uv_run(&loop, UV_RUN_DEFAULT);
  assert(err == 0);

  err = js_destroy_platform(bare_worklet__platform);
  assert(err == 0);

  err = uv_run(&loop, UV_RUN_DEFAULT);
  assert(err == 0);

  err = uv_loop_close(&loop);
  assert(err == 0);
}

static void
bare_worklet__on_platform_init(void) {
  int err;

  err = uv_barrier_init(&bare_worklet__platform_ready, 2);
  assert(err == 0);

  err = uv_thread_create(&bare_worklet__platform_thread, bare_worklet__on_platform_thread, NULL);
  assert(err == 0);

  uv_barrier_wait(&bare_worklet__platform_ready);

  uv_barrier_destroy(&bare_worklet__platform_ready);
}

static void
bare_worklet__on_stopped(void) {
  uv_mutex_lock(&bare_worklet__running_lock);

  if (--bare_worklet__running == 0) uv_cond_signal(&bare_worklet__running_idle);

  uv_mutex_unlock(&bare_worklet__running_lock);
}

int
bare_worklet_teardown(void) {
  uv_once(&bare_worklet__init_guard, bare_worklet__on_init);

  int err;

  uv_mutex_lock(&bare_worklet__running_lock);

  bare_worklet__torn_down = true;

  while (bare_worklet__running > 0)
    uv_cond_wait(&bare_worklet__running_idle, &bare_worklet__running_lock);

  uv_mutex_unlock(&bare_worklet__running_lock);

  if (bare_worklet__platform) {
    err = uv_async_send(&bare_worklet__platform_shutdown);
    assert(err == 0);

    err = uv_thread_join(&bare_worklet__platform_thread);
    assert(err == 0);
  }

  return 0;
}

static void
bare_worklet__on_suspend(bare_t *bare, int linger, void *data) {
  bare_worklet_state_t *state = data;

  if (state->finished) return;

  if (state->callbacks.suspend) state->callbacks.suspend(linger, state->callbacks.suspend_data);
}

static void
bare_worklet__on_wakeup(bare_t *bare, int deadline, void *data) {
  bare_worklet_state_t *state = data;

  if (state->finished) return;

  if (state->callbacks.wakeup) state->callbacks.wakeup(deadline, state->callbacks.wakeup_data);
}

static void
bare_worklet__on_idle(bare_t *bare, void *data) {
  int err;

  bare_worklet_state_t *state = data;

  uv_mutex_lock(&state->lock);

  // A pending resume means the current suspension, if any, was started after
  // this idle was reached and so is not ours to end.
  if (!state->resuming) {
    err = bare_suspension_end(&state->suspension);
    assert(err == 0);
  }

  uv_mutex_unlock(&state->lock);

  if (state->finished) return;

  if (state->callbacks.idle) state->callbacks.idle(state->callbacks.idle_data);
}

static void
bare_worklet__on_resume(bare_t *bare, void *data) {
  bare_worklet_state_t *state = data;

  uv_mutex_lock(&state->lock);

  state->resuming = false;

  uv_mutex_unlock(&state->lock);

  if (state->finished) return;

  if (state->callbacks.resume) state->callbacks.resume(state->callbacks.resume_data);
}

static void
bare_worklet__on_ipc_finalize(js_env_t *env, void *data, void *finalize_hint) {
  free(data);
}

// native.read() -> ArrayBuffer (data) | null (EOF) | undefined (would block)
static js_value_t *
bare_worklet__on_ipc_read(js_env_t *env, js_callback_info_t *info) {
  int err;

  bare_queue_port_t *port;
  err = js_get_callback_info(env, info, NULL, NULL, NULL, (void **) &port);
  assert(err == 0);

  void *data;
  size_t len;
  int status = bare_queue_read(port, &data, &len);

  js_value_t *result;

  if (status == bare_queue_would_block) {
    err = js_get_undefined(env, &result);
    assert(err == 0);
  } else if (len == 0) {
    err = js_get_null(env, &result);
    assert(err == 0);
  } else {
    err = js_create_external_arraybuffer(env, data, len, bare_worklet__on_ipc_finalize, NULL, &result);
    assert(err == 0);
  }

  return result;
}

// native.write(buffer) -> number (written) | -1 (would block) | null (closed)
static js_value_t *
bare_worklet__on_ipc_write(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 1;
  js_value_t *argv[1];

  bare_queue_port_t *port;
  err = js_get_callback_info(env, info, &argc, argv, NULL, (void **) &port);
  assert(err == 0);
  assert(argc == 1);

  void *data;
  size_t len;
  err = js_get_typedarray_info(env, argv[0], NULL, &data, &len, NULL, NULL);
  assert(err == 0);

  int n = bare_queue_write(port, data, len);

  js_value_t *result;

  if (n == bare_queue_closed) {
    err = js_get_null(env, &result);
  } else {
    err = js_create_int32(env, n, &result);
  }
  assert(err == 0);

  return result;
}

static js_value_t *
bare_worklet__on_ipc_end(js_env_t *env, js_callback_info_t *info) {
  int err;

  bare_queue_port_t *port;
  err = js_get_callback_info(env, info, NULL, NULL, NULL, (void **) &port);
  assert(err == 0);

  bare_queue_end(port);

  return NULL;
}

static js_value_t *
bare_worklet__on_ipc_ref(js_env_t *env, js_callback_info_t *info) {
  int err;

  bare_queue_port_t *port;
  err = js_get_callback_info(env, info, NULL, NULL, NULL, (void **) &port);
  assert(err == 0);

  uv_ref((uv_handle_t *) &port->queue->async);

  return NULL;
}

static js_value_t *
bare_worklet__on_ipc_unref(js_env_t *env, js_callback_info_t *info) {
  int err;

  bare_queue_port_t *port;
  err = js_get_callback_info(env, info, NULL, NULL, NULL, (void **) &port);
  assert(err == 0);

  uv_unref((uv_handle_t *) &port->queue->async);

  return NULL;
}

typedef struct {
  js_env_t *env;
  js_ref_t *signal;
} bare_worklet__ipc_t;

static void
bare_worklet__on_ipc_signal(bare_queue_port_t *port) {
  int err;

  bare_worklet__ipc_t *ipc = (bare_worklet__ipc_t *) port->data;

  assert(ipc->signal);

  js_env_t *env = ipc->env;

  js_handle_scope_t *scope;
  err = js_open_handle_scope(env, &scope);
  assert(err == 0);

  js_value_t *fn;
  err = js_get_reference_value(env, ipc->signal, &fn);
  assert(err == 0);

  js_value_t *global;
  err = js_get_global(env, &global);
  assert(err == 0);

  js_call_function(env, global, fn, 0, NULL, NULL);

  err = js_close_handle_scope(env, scope);
  assert(err == 0);
}

static void
bare_worklet__on_thread(void *opaque) {
  uv_once(&bare_worklet__platform_guard, bare_worklet__on_platform_init);

  int err;

  bare_worklet_t *worklet = (bare_worklet_t *) opaque;

  bare_worklet_state_t *state = worklet->state;

  bare_kit__on_thread_enter();

  uv_sem_t finished;
  err = uv_sem_init(&finished, 0);
  assert(err == 0);

  worklet->finished = &finished;

  err = bare_suspension_init(&state->suspension);
  assert(err == 0);

  uv_loop_t loop;
  err = uv_loop_init(&loop);
  assert(err == 0);

  bare_t *bare;

  bare_options_t options = {
    .version = 0,
    .memory_limit = worklet->options.memory_limit,
  };

  js_env_t *env;
  err = bare_setup(&loop, bare_worklet__platform, &env, worklet->argc, worklet->argv, &options, &bare);
  assert(err == 0);

  bare_kit__publish_context(bare);

  err = bare_on_suspend(bare, bare_worklet__on_suspend, state);
  assert(err == 0);

  err = bare_on_wakeup(bare, bare_worklet__on_wakeup, state);
  assert(err == 0);

  err = bare_on_idle(bare, bare_worklet__on_idle, state);
  assert(err == 0);

  err = bare_on_resume(bare, bare_worklet__on_resume, state);
  assert(err == 0);

  uv_mutex_lock(&state->lock);

  state->bare = bare;

  uv_mutex_unlock(&state->lock);

  js_handle_scope_t *scope;
  err = js_open_handle_scope(env, &scope);
  assert(err == 0);

  uv_buf_t source = uv_buf_init((char *) worklet_bundle, worklet_bundle_len);

  js_value_t *module;
  err = bare_load(bare, "bare:/worklet.bundle", &source, &module);
  assert(err == 0);

  js_value_t *exports;
  err = js_get_named_property(env, module, "exports", &exports);
  assert(err == 0);

  // Copied out, as `worklet` is owned by the host and may be destroyed once
  // signalled. The ipc state lives here, on the worklet thread, for the same
  // reason.
  bare_queue_t *queue = worklet->queue;

  bare_worklet__ipc_t ipc = {env, NULL};

  bare_queue_port_t *ipc_port = bare_queue_open_uv(queue, &loop, bare_worklet__on_ipc_signal, &ipc);

  js_value_t *native;
  err = js_create_object(env, &native);
  assert(err == 0);

#define V(name, cb) \
  { \
    js_value_t *fn; \
    err = js_create_function(env, name, -1, cb, ipc_port, &fn); \
    assert(err == 0); \
    err = js_set_named_property(env, native, name, fn); \
    assert(err == 0); \
  }

  V("read", bare_worklet__on_ipc_read);
  V("write", bare_worklet__on_ipc_write);
  V("end", bare_worklet__on_ipc_end);
  V("ref", bare_worklet__on_ipc_ref);
  V("unref", bare_worklet__on_ipc_unref);
#undef V

  js_value_t *open_ipc;
  err = js_get_named_property(env, exports, "openIPC", &open_ipc);
  assert(err == 0);

  js_value_t *on_signal;
  err = js_call_function(env, exports, open_ipc, 1, &native, &on_signal);
  assert(err == 0);

  err = js_create_reference(env, on_signal, 1, &ipc.signal);
  assert(err == 0);

  js_value_t *start;
  err = js_get_named_property(env, exports, "start", &start);
  assert(err == 0);

  js_value_t *args[3];

  err = js_create_string_utf8(env, (const utf8_t *) worklet->filename, -1, &args[0]);
  assert(err == 0);

  if (worklet->source) {
    void *data;
    err = js_create_arraybuffer(env, worklet->len, &data, &args[1]);
    assert(err == 0);

    memcpy(data, worklet->source, worklet->len);
  } else {
    err = js_get_null(env, &args[1]);
    assert(err == 0);
  }

  if (worklet->options.assets) {
    err = js_create_string_utf8(env, (const utf8_t *) worklet->options.assets, -1, &args[2]);
    assert(err == 0);
  } else {
    err = js_get_null(env, &args[2]);
    assert(err == 0);
  }

  uv_barrier_wait(&worklet->ready);

  js_call_function(env, module, start, 3, args, NULL);

  err = js_close_handle_scope(env, scope);
  assert(err == 0);

  err = bare_run(bare, UV_RUN_DEFAULT);
  assert(err == 0);

  state->finished = true;

  // Closed natively rather than from JS, so the host sees EOF on every exit path.
  bare_queue_close(ipc_port);

  // From here the host no longer reaches the worklet, which can tear down
  // without waiting to be destroyed.
  uv_mutex_lock(&state->lock);

  state->bare = NULL;

  err = bare_suspension_end(&state->suspension);
  assert(err == 0);

  uv_mutex_unlock(&state->lock);

  // Before deleting the reference, so no late signal reaches it.
  bare_queue_shutdown_uv(queue);

  err = js_delete_reference(env, ipc.signal);
  assert(err == 0);

  int exit_code;
  err = bare_teardown(bare, UV_RUN_DEFAULT, &exit_code);
  assert(err == 0);

  err = uv_loop_close(&loop);
  assert(err == 0);

  bare_suspension_destroy(&state->suspension);

  if (state->callbacks.exit) state->callbacks.exit(exit_code, state->callbacks.exit_data);

  bare_worklet__on_stopped();

  // The host may still open an ipc on the queue until it destroys the worklet.
  uv_sem_wait(&finished);

  uv_sem_destroy(&finished);

  bare_queue_destroy(queue);

  bare_kit__on_thread_exit();

  uv_mutex_destroy(&state->lock);

  free(state);
}

int
bare_worklet_start(bare_worklet_t *worklet, const char *filename, const void *source, size_t len, int argc, const char *argv[]) {
  int err;

  worklet->filename = filename;
  worklet->source = source;
  worklet->len = len;
  worklet->argc = argc;
  worklet->argv = argv;

  uv_mutex_lock(&bare_worklet__running_lock);

  bool torn_down = bare_worklet__torn_down;

  if (!torn_down) bare_worklet__running++;

  uv_mutex_unlock(&bare_worklet__running_lock);

  if (torn_down) return -1;

  err = uv_barrier_init(&worklet->ready, 2);
  assert(err == 0);

  err = uv_thread_create(&worklet->thread, bare_worklet__on_thread, (void *) worklet);
  if (err < 0) {
    uv_barrier_destroy(&worklet->ready);

    worklet->thread = 0;

    bare_worklet__on_stopped();

    return err;
  }

  err = uv_thread_detach(&worklet->thread);
  assert(err == 0);

  uv_barrier_wait(&worklet->ready);

  uv_barrier_destroy(&worklet->ready);

  return 0;
}

int
bare_worklet_suspend(bare_worklet_t *worklet, int linger) {
  int err = 0;

  bare_worklet_state_t *state = worklet->state;

  uv_mutex_lock(&state->lock);

  if (state->bare) {
    linger = bare_suspension_start(&state->suspension, linger);
    assert(linger >= 0);

    state->suspended = true;

    err = bare_suspend(state->bare, linger);
  }

  uv_mutex_unlock(&state->lock);

  return err;
}

int
bare_worklet_resume(bare_worklet_t *worklet) {
  int err = 0;

  bare_worklet_state_t *state = worklet->state;

  uv_mutex_lock(&state->lock);

  if (state->bare) {
    err = bare_suspension_end(&state->suspension);
    assert(err == 0);

    // Only a resume that follows a suspend is guaranteed to be observed by the
    // worklet thread, as Bare ignores resuming a worklet that is already active.
    if (state->suspended) {
      state->suspended = false;
      state->resuming = true;
    }

    err = bare_resume(state->bare);
  }

  uv_mutex_unlock(&state->lock);

  return err;
}

int
bare_worklet_wakeup(bare_worklet_t *worklet, int deadline) {
  int err = 0;

  bare_worklet_state_t *state = worklet->state;

  uv_mutex_lock(&state->lock);

  if (state->bare) {
    deadline = bare_suspension_start(&state->suspension, deadline);
    assert(deadline >= 0);

    err = bare_wakeup(state->bare, deadline);
  }

  uv_mutex_unlock(&state->lock);

  return err;
}

int
bare_worklet_terminate(bare_worklet_t *worklet) {
  int err = 0;

  bare_worklet_state_t *state = worklet->state;

  uv_mutex_lock(&state->lock);

  if (state->bare) err = bare_terminate(state->bare);

  uv_mutex_unlock(&state->lock);

  return err;
}
