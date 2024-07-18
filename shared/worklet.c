#include <assert.h>
#include <bare.h>
#include <js.h>
#include <stddef.h>
#include <utf.h>
#include <uv.h>

#include "worklet.bundle.h"
#include "worklet.h"

int
bare_worklet_init (bare_worklet_t *worklet, const bare_worklet_options_t *options) {
  int err;

  worklet->bare = NULL;

  memset(&worklet->options, 0, sizeof(worklet->options));

  if (options) worklet->options = *options;

  err = uv_sem_init(&worklet->ready, 0);
  assert(err == 0);

  err = uv_mutex_init(&worklet->lock);
  assert(err == 0);

  return 0;
}

void
bare_worklet_destroy (bare_worklet_t *worklet) {
  int err;

  err = uv_thread_join(&worklet->thread);
  assert(err == 0);

  uv_sem_destroy(&worklet->ready);

  uv_mutex_destroy(&worklet->lock);
}

static js_value_t *
bare_worklet__on_push_reply (js_env_t *env, js_callback_info_t *info) {
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
  err = js_is_error(env, argv[0], &has_error);
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
bare_worklet__on_push (js_env_t *env, js_value_t *onpush, void *context, void *data) {
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
bare_worklet__on_thread (void *opaque) {
  int err;

  bare_worklet_t *worklet = (bare_worklet_t *) opaque;

  int argc = 0;
  char **argv = NULL;

  argv = uv_setup_args(argc, argv);

  uv_loop_t loop;
  err = uv_loop_init(&loop);
  assert(err == 0);

  js_platform_t *platform;
  {
    js_platform_options_t options = {
      .version = 1,
      .optimize_for_memory = worklet->options.optimize_for_memory,
    };

    err = js_create_platform(&loop, &options, &platform);
  }
  assert(err == 0);

  js_env_t *env;
  {
    bare_options_t options = {
      .version = 0,
      .memory_limit = worklet->options.memory_limit,
    };

    err = bare_setup(&loop, platform, &env, argc, argv, &options, &worklet->bare);
  }
  assert(err == 0);

  bare_t *bare = worklet->bare;

  js_handle_scope_t *scope;
  err = js_open_handle_scope(env, &scope);
  assert(err == 0);

  uv_buf_t source = uv_buf_init((char *) worklet_bundle, worklet_bundle_len);

  js_value_t *module;
  err = bare_load(bare, "/worklet.bundle", &source, &module);
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

  js_value_t *push;
  err = js_get_named_property(env, exports, "push", &push);
  assert(err == 0);

  err = js_create_threadsafe_function(env, push, 64, 1, NULL, NULL, (void *) worklet, bare_worklet__on_push, &worklet->push);
  assert(err == 0);

  err = js_close_handle_scope(env, scope);
  assert(err == 0);

  bare_load(bare, worklet->filename, &worklet->source, NULL);

  uv_sem_post(&worklet->ready);

  err = bare_run(bare);
  assert(err == 0);

  uv_mutex_lock(&worklet->lock);

  worklet->bare = NULL;

  uv_mutex_unlock(&worklet->lock);

  err = js_release_threadsafe_function(worklet->push, js_threadsafe_function_release);
  assert(err == 0);

  int exit_code;
  err = bare_teardown(bare, &exit_code);
  assert(err == 0);

  err = js_destroy_platform(platform);
  assert(err == 0);

  err = uv_run(&loop, UV_RUN_DEFAULT);
  assert(err == 0);

  err = uv_loop_close(&loop);
  assert(err == 0);
}

int
bare_worklet_start (bare_worklet_t *worklet, const char *filename, const uv_buf_t *source) {
  int err;

  worklet->filename = filename;
  worklet->source = *source;

  err = uv_thread_create(&worklet->thread, bare_worklet__on_thread, (void *) worklet);
  if (err < 0) return err;

  uv_sem_wait(&worklet->ready);

  return 0;
}

int
bare_worklet_suspend (bare_worklet_t *worklet, int linger) {
  int err;

  uv_mutex_lock(&worklet->lock);

  if (worklet->bare) err = bare_suspend(worklet->bare, linger);
  else err = -1;

  uv_mutex_unlock(&worklet->lock);

  return err;
}

int
bare_worklet_resume (bare_worklet_t *worklet) {
  int err;

  uv_mutex_lock(&worklet->lock);

  if (worklet->bare) err = bare_resume(worklet->bare);
  else err = -1;

  uv_mutex_unlock(&worklet->lock);

  return err;
}

int
bare_worklet_terminate (bare_worklet_t *worklet) {
  int err;

  uv_mutex_lock(&worklet->lock);

  if (worklet->bare) err = bare_terminate(worklet->bare);
  else err = -1;

  uv_mutex_unlock(&worklet->lock);

  return err;
}

int
bare_worklet_push (bare_worklet_t *worklet, bare_worklet_push_t *req, const uv_buf_t *payload, bare_worklet_push_cb cb) {
  int err;

  req->worklet = worklet;
  req->payload = *payload;
  req->cb = cb;

  uv_mutex_lock(&worklet->lock);

  if (worklet->bare) err = js_call_threadsafe_function(worklet->push, (void *) req, js_threadsafe_function_blocking);
  else err = -1;

  uv_mutex_unlock(&worklet->lock);

  return err;
}
