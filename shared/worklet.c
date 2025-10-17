#include <assert.h>
#include <bare.h>
#include <js.h>
#include <log.h>
#include <rlimit.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utf.h>
#include <uv.h>

#include "suspension.h"
#include "worklet.bundle.h"
#include "worklet.h"

static uv_once_t bare_worklet__init_guard = UV_ONCE_INIT;

static void
bare_worklet__on_init(void) {
  int err;

#ifdef SIGPIPE
  signal(SIGPIPE, SIG_IGN);
#endif

  err = log_open("bare", 0);
  assert(err == 0);

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

  worklet->bare = NULL;
  worklet->thread = 0;

  memset(&worklet->options, 0, sizeof(worklet->options));

  if (options) {
    worklet->options.memory_limit = options->memory_limit;
    worklet->options.assets = options->assets == NULL ? NULL : strdup(options->assets);
  }

  err = bare_suspension_init(&worklet->suspension);
  assert(err == 0);

  return 0;
}

void
bare_worklet_destroy(bare_worklet_t *worklet) {
  int err;

  if (worklet->thread != 0) uv_barrier_wait(worklet->finished);

  close(worklet->incoming);
  close(worklet->outgoing);

  free((char *) worklet->options.assets);
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

  err = js_create_external_arraybuffer(env, (void *) payload.base, (size_t) payload.len, NULL, NULL, &args[0]);
  assert(err == 0);

  err = js_create_function(env, "reply", -1, bare_worklet__on_push_reply, data, &args[1]);
  assert(err == 0);

  js_value_t *global;
  err = js_get_global(env, &global);
  assert(err == 0);

  err = js_call_function(env, global, onpush, 2, args, NULL);
  assert(err == 0);

  err = js_detach_arraybuffer(env, args[0]);
  assert(err == 0);
}

static void
bare_worklet__on_finalize(js_env_t *env, void *data, void *finalize_hint) {
  bare_worklet_t *worklet = finalize_hint;

  if (worklet->finalize) worklet->finalize(worklet, &worklet->source.buffer, worklet->finalize_hint);
}

static void
bare_worklet__on_idle(bare_t *bare, void *data) {
  int err;

  bare_worklet_t *worklet = data;

  err = bare_suspension_end(&worklet->suspension);
  assert(err == 0);
}

static void
bare_worklet__on_thread(void *opaque) {
  uv_once(&bare_worklet__platform_guard, bare_worklet__on_platform_init);

  int err;

  bare_worklet_t *worklet = (bare_worklet_t *) opaque;

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

  err = bare_on_idle(bare, bare_worklet__on_idle, worklet);
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

  js_value_t *port;
  err = js_get_named_property(env, exports, "port", &port);
  assert(err == 0);

  js_value_t *incoming;
  err = js_get_named_property(env, port, "incoming", &incoming);
  assert(err == 0);

  err = js_get_value_int32(env, incoming, &worklet->incoming);
  assert(err == 0);

  js_value_t *outgoing;
  err = js_get_named_property(env, port, "outgoing", &outgoing);
  assert(err == 0);

  err = js_get_value_int32(env, outgoing, &worklet->outgoing);
  assert(err == 0);

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
    err = js_create_external_arraybuffer(
      env,
      worklet->source.buffer.base,
      worklet->source.buffer.len,
      bare_worklet__on_finalize,
      worklet,
      &args[1]
    );
    assert(err == 0);
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

  js_call_function(env, module, start, 3, args, NULL);

  err = js_close_handle_scope(env, scope);
  assert(err == 0);

  uv_barrier_t finished;
  err = uv_barrier_init(&finished, 2);
  assert(err == 0);

  worklet->finished = &finished;

  uv_barrier_wait(&worklet->ready);

  err = bare_run(bare, UV_RUN_DEFAULT);
  assert(err == 0);

  uv_barrier_wait(&finished);

  uv_barrier_destroy(&finished);

  err = js_release_threadsafe_function(push, js_threadsafe_function_release);
  assert(err == 0);

  int exit_code;
  err = bare_teardown(bare, UV_RUN_DEFAULT, &exit_code);
  assert(err == 0);

  err = uv_loop_close(&loop);
  assert(err == 0);
}

int
bare_worklet_start(bare_worklet_t *worklet, const char *filename, const uv_buf_t *source, bare_worklet_finalize_cb finalize, void *finalize_hint, int argc, const char *argv[]) {
  int err;

  worklet->filename = filename;
  worklet->finalize = finalize;
  worklet->finalize_hint = finalize_hint;
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

  err = uv_thread_detach(&worklet->thread);
  assert(err == 0);

  uv_barrier_wait(&worklet->ready);

  uv_barrier_destroy(&worklet->ready);

  return 0;
}

int
bare_worklet_suspend(bare_worklet_t *worklet, int linger) {
  int err;

  linger = bare_suspension_start(&worklet->suspension, linger);
  assert(linger >= 0);

  return bare_suspend(worklet->bare, linger);
}

int
bare_worklet_resume(bare_worklet_t *worklet) {
  int err;

  err = bare_suspension_end(&worklet->suspension);
  assert(err == 0);

  return bare_resume(worklet->bare);
}

int
bare_worklet_wakeup(bare_worklet_t *worklet, int deadline) {
  int err;

  deadline = bare_suspension_start(&worklet->suspension, deadline);
  assert(deadline >= 0);

  return bare_wakeup(worklet->bare, deadline);
}

int
bare_worklet_terminate(bare_worklet_t *worklet) {
  int err;

  err = bare_suspension_end(&worklet->suspension);
  assert(err == 0);

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
