#include <assert.h>
#include <bare.h>
#include <js.h>
#include <rlimit.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#if defined(BARE_KIT_WINDOWS)
#include <io.h>
#else
#include <unistd.h>
#endif
#include <utf.h>
#include <uv.h>

#include "events.h"
#include "suspension.h"
#include "worklet.bundle.h"
#include "worklet.h"

struct bare_worklet_state_s {
  bool finished;

  bare_suspension_t suspension;

  struct {
    bare_suspend_cb suspend;
    void *suspend_data;

    bare_wakeup_cb wakeup;
    void *wakeup_data;

    bare_idle_cb idle;
    void *idle_data;

    bare_resume_cb resume;
    void *resume_data;
  } callbacks;
};

static uv_once_t bare_worklet__init_guard = UV_ONCE_INIT;

static void
bare_worklet__on_init(void) {
  int err;

#ifdef SIGPIPE
  signal(SIGPIPE, SIG_IGN);
#endif

  err = rlimit_set(rlimit_open_files, rlimit_infer);
  assert(err == 0);
}

int
bare_worklet_alloc(bare_worklet_t **result) {
  bare_worklet_t *worklet = malloc(sizeof(bare_worklet_t));

  if (worklet == NULL) return -1;

  *result = worklet;

  return 0;
}

int
bare_worklet_init(bare_worklet_t *worklet, const bare_worklet_options_t *options) {
  uv_once(&bare_worklet__init_guard, bare_worklet__on_init);

  int err;

  worklet->thread = 0;

  bare_worklet_state_t *state = malloc(sizeof(bare_worklet_state_t));

  state->finished = false;

  memset(&state->callbacks, 0, sizeof(state->callbacks));

  worklet->state = state;

  memset(&worklet->options, 0, sizeof(worklet->options));

  if (options) {
    worklet->options.memory_limit = options->memory_limit;
    worklet->options.assets = options->assets == NULL ? NULL : strdup(options->assets);
  }

  worklet->queue = bare_queue_create();

  if (worklet->queue == NULL) return -1;

  return 0;
}

void
bare_worklet_destroy(bare_worklet_t *worklet) {
  if (worklet->thread != 0) {
    // Stop the worklet if it is still running and wake its loop so the stop
    // takes effect, then unblock it. The thread is detached and tears itself
    // down; this function must never block (blocking here causes ANR on
    // Android). bare_terminate is a no-op if the worklet already exited.
    bare_terminate(worklet->bare);
    uv_async_send(&worklet->queue->async);
    uv_sem_post(worklet->finished);
  } else {
    // Nothing ever took ownership of the queue, so it is ours to free.
    free(worklet->state);

    bare_queue_destroy(worklet->queue);
  }

  free((char *) worklet->options.assets);
}

int
bare_worklet_on_suspend(bare_worklet_t *worklet, bare_suspend_cb cb, void *data) {
  worklet->state->callbacks.suspend = cb;
  worklet->state->callbacks.suspend_data = data;

  return 0;
}

int
bare_worklet_on_wakeup(bare_worklet_t *worklet, bare_wakeup_cb cb, void *data) {
  worklet->state->callbacks.wakeup = cb;
  worklet->state->callbacks.wakeup_data = data;

  return 0;
}

int
bare_worklet_on_idle(bare_worklet_t *worklet, bare_idle_cb cb, void *data) {
  worklet->state->callbacks.idle = cb;
  worklet->state->callbacks.idle_data = data;

  return 0;
}

int
bare_worklet_on_resume(bare_worklet_t *worklet, bare_resume_cb cb, void *data) {
  worklet->state->callbacks.resume = cb;
  worklet->state->callbacks.resume_data = data;

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

static js_value_t *
bare_worklet__on_push_reply(js_env_t *env, js_callback_info_t *info) {
  int err;

  bare_worklet_push_t *push;

  size_t argc = 2;
  js_value_t *argv[2];

  err = js_get_callback_info(env, info, &argc, argv, NULL, (void **) &push);
  assert(err == 0);

  assert(argc == 1 || argc == 2);

  if (argc == 1) {
    err = js_get_null(env, &argv[1]);
    assert(err == 0);
  }

  char *error;

  bool has_error;
  err = js_is_string(env, argv[0], &has_error);
  assert(err == 0);

  if (has_error) {
    size_t len;
    err = js_get_value_string_utf8(env, argv[0], NULL, 0, &len);
    assert(err == 0);

    len += 1 /* NULL */;

    error = malloc(len);

    err = js_get_value_string_utf8(env, argv[0], (utf8_t *) error, len, NULL);
    assert(err == 0);
  }

  uv_buf_t reply;

  bool has_reply;
  err = js_is_typedarray(env, argv[1], &has_reply);
  assert(err == 0);

  if (has_reply) {
    err = js_get_typedarray_info(env, argv[1], NULL, (void **) &reply.base, (size_t *) &reply.len, NULL, NULL);
    assert(err == 0);
  }

  push->cb(push, has_error ? error : NULL, has_reply ? &reply : NULL);

  if (has_error) free(error);

  return NULL;
}

static void
bare_worklet__on_push(js_env_t *env, js_value_t *onpush, void *context, void *data) {
  int err;

  bare_worklet_push_t *push = (bare_worklet_push_t *) data;

  uv_buf_t payload = push->payload;

  js_value_t *args[2];

  void *copy;
  err = js_create_arraybuffer(env, payload.len, &copy, &args[0]);
  assert(err == 0);

  memcpy(copy, payload.base, payload.len);

  err = js_create_function(env, "reply", -1, bare_worklet__on_push_reply, data, &args[1]);
  assert(err == 0);

  js_value_t *global;
  err = js_get_global(env, &global);
  assert(err == 0);

  err = js_call_function(env, global, onpush, 2, args, NULL);
  assert(err == 0);
}

static void
bare_worklet__on_suspend(bare_t *bare, int linger, void *data) {
  bare_worklet_state_t *state = data;

  if (state->finished) return;

  if (state->callbacks.suspend) state->callbacks.suspend(bare, linger, state->callbacks.suspend_data);
}

static void
bare_worklet__on_wakeup(bare_t *bare, int deadline, void *data) {
  bare_worklet_state_t *state = data;

  if (state->finished) return;

  if (state->callbacks.wakeup) state->callbacks.wakeup(bare, deadline, state->callbacks.wakeup_data);
}

static void
bare_worklet__on_idle(bare_t *bare, void *data) {
  int err;

  bare_worklet_state_t *state = data;

  err = bare_suspension_end(&state->suspension);
  assert(err == 0);

  if (state->finished) return;

  if (state->callbacks.idle) state->callbacks.idle(bare, state->callbacks.idle_data);
}

static void
bare_worklet__on_resume(bare_t *bare, void *data) {
  int err;

  bare_worklet_state_t *state = data;

  err = bare_suspension_end(&state->suspension);
  assert(err == 0);

  if (state->finished) return;

  if (state->callbacks.resume) state->callbacks.resume(bare, state->callbacks.resume_data);
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
    void *copy;
    err = js_create_arraybuffer(env, len, &copy, &result);
    assert(err == 0);

    memcpy(copy, data, len);
  }

  return result;
}

// native.write(buffer) -> bytes written, or a negative queue status
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
  err = js_create_int32(env, n, &result);
  assert(err == 0);

  return result;
}

static js_value_t *
bare_worklet__on_ipc_close(js_env_t *env, js_callback_info_t *info) {
  int err;

  bare_queue_port_t *port;
  err = js_get_callback_info(env, info, NULL, NULL, NULL, (void **) &port);
  assert(err == 0);

  bare_queue_close(port);

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

// Runs on the worklet loop when the host has touched the queue; drives the JS
// duplex to re-check reads and any pending write.
static void
bare_worklet__on_ipc_signal(bare_queue_port_t *port) {
  int err;

  bare_worklet_t *worklet = (bare_worklet_t *) port->data;

  js_env_t *env = worklet->env;

  js_handle_scope_t *scope;
  err = js_open_handle_scope(env, &scope);
  assert(err == 0);

  js_value_t *fn;
  err = js_get_reference_value(env, worklet->ipc_signal, &fn);
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

  bare_kit__on_thread_enter(state);

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

  err = bare_on_suspend(bare, bare_worklet__on_suspend, state);
  assert(err == 0);

  err = bare_on_wakeup(bare, bare_worklet__on_wakeup, state);
  assert(err == 0);

  err = bare_on_idle(bare, bare_worklet__on_idle, state);
  assert(err == 0);

  err = bare_on_resume(bare, bare_worklet__on_resume, state);
  assert(err == 0);

  worklet->bare = bare;

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

  worklet->env = env;

  // Held for the rest of this function: the host may destroy its worklet, and
  // with it every field read from it, the moment this thread hands back control.
  bare_queue_t *queue = worklet->queue;

  bare_queue_port_t *ipc_port = bare_queue_open_uv(queue, &loop, bare_worklet__on_ipc_signal);

  ipc_port->data = worklet;

  js_value_t *native;
  err = js_create_object(env, &native);
  assert(err == 0);

#define BARE_WORKLET_IPC_FN(name, cb) \
  do { \
    js_value_t *fn; \
    err = js_create_function(env, name, -1, cb, ipc_port, &fn); \
    assert(err == 0); \
    err = js_set_named_property(env, native, name, fn); \
    assert(err == 0); \
  } while (0)

  BARE_WORKLET_IPC_FN("read", bare_worklet__on_ipc_read);
  BARE_WORKLET_IPC_FN("write", bare_worklet__on_ipc_write);
  BARE_WORKLET_IPC_FN("close", bare_worklet__on_ipc_close);
  BARE_WORKLET_IPC_FN("ref", bare_worklet__on_ipc_ref);
  BARE_WORKLET_IPC_FN("unref", bare_worklet__on_ipc_unref);

#undef BARE_WORKLET_IPC_FN

  js_value_t *open_ipc;
  err = js_get_named_property(env, exports, "openIPC", &open_ipc);
  assert(err == 0);

  js_value_t *on_signal;
  err = js_call_function(env, exports, open_ipc, 1, &native, &on_signal);
  assert(err == 0);

  err = js_create_reference(env, on_signal, 1, &worklet->ipc_signal);
  assert(err == 0);

  js_ref_t *ipc_signal = worklet->ipc_signal;

  js_value_t *fn;
  err = js_get_named_property(env, exports, "push", &fn);
  assert(err == 0);

  js_threadsafe_function_t *push;
  err = js_create_threadsafe_function(env, fn, 64, 1, NULL, NULL, (void *) worklet, bare_worklet__on_push, &push);
  assert(err == 0);

  err = js_unref_threadsafe_function(env, push);
  assert(err == 0);

  worklet->push = push;

  js_value_t *start;
  err = js_get_named_property(env, exports, "start", &start);
  assert(err == 0);

  js_value_t *args[3];

  err = js_create_string_utf8(env, (const utf8_t *) worklet->filename, -1, &args[0]);
  assert(err == 0);

  if (worklet->source.type == bare_worklet_source_buffer) {
    const uv_buf_t source = worklet->source.buffer;

    void *data;
    err = js_create_arraybuffer(env, source.len, &data, &args[1]);
    assert(err == 0);

    memcpy(data, source.base, source.len);
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

  // Signal EOF to the host by closing our end of the queue. This is in-process
  // (a status flag plus a host wakeup), so it works on every exit path without
  // pumping the loop or scheduling async work from the exit event.
  bare_queue_close(ipc_port);

  uv_sem_wait(&finished);

  uv_sem_destroy(&finished);

  err = js_release_threadsafe_function(push, js_threadsafe_function_release);
  assert(err == 0);

  // Closing the async here stops further signals; teardown's loop run flushes
  // it. Do this before deleting the reference so no late signal dereferences it.
  bare_queue_shutdown_uv(queue);

  err = js_delete_reference(env, ipc_signal);
  assert(err == 0);

  int exit_code;
  err = bare_teardown(bare, UV_RUN_DEFAULT, &exit_code);
  assert(err == 0);

  err = uv_loop_close(&loop);
  assert(err == 0);

  // Ours since the host started us, and nothing may reach it now: the host was
  // required to retire its end before signalling us to be destroyed.
  bare_queue_destroy(queue);

  err = bare_suspension_end(&state->suspension);
  assert(err == 0);

  bare_kit__on_thread_exit(state);

  free(state);
}

int
bare_worklet_start(bare_worklet_t *worklet, const char *filename, const uv_buf_t *source, int argc, const char *argv[]) {
  int err;

  worklet->filename = filename;
  worklet->argc = argc;
  worklet->argv = argv;

  if (source) {
    worklet->source.type = bare_worklet_source_buffer;
    worklet->source.buffer = *source;
  } else {
    worklet->source.type = bare_worklet_source_none;
  }

  err = uv_barrier_init(&worklet->ready, 2);
  assert(err == 0);

  err = uv_thread_create(&worklet->thread, bare_worklet__on_thread, (void *) worklet);
  if (err < 0) {
    uv_barrier_destroy(&worklet->ready);

    return err;
  }

  uv_barrier_wait(&worklet->ready);

  uv_barrier_destroy(&worklet->ready);

  return 0;
}

int
bare_worklet_suspend(bare_worklet_t *worklet, int linger) {
  int err;

  linger = bare_suspension_start(&worklet->state->suspension, linger);
  assert(linger >= 0);

  return bare_suspend(worklet->bare, linger);
}

int
bare_worklet_resume(bare_worklet_t *worklet) {
  return bare_resume(worklet->bare);
}

int
bare_worklet_wakeup(bare_worklet_t *worklet, int deadline) {
  int err;

  deadline = bare_suspension_start(&worklet->state->suspension, deadline);
  assert(deadline >= 0);

  return bare_wakeup(worklet->bare, deadline);
}

int
bare_worklet_terminate(bare_worklet_t *worklet) {
  return bare_terminate(worklet->bare);
}

int
bare_worklet_push(bare_worklet_t *worklet, bare_worklet_push_t *req, const uv_buf_t *payload, bare_worklet_push_cb cb) {
  req->worklet = worklet;
  req->payload = *payload;
  req->cb = cb;

  return js_call_threadsafe_function(worklet->push, (void *) req, js_threadsafe_function_blocking);
}

void *
bare_worklet_push_get_data(bare_worklet_push_t *req) {
  return req->data;
}

void
bare_worklet_push_set_data(bare_worklet_push_t *req, void *data) {
  req->data = data;
}
