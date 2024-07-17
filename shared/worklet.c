#include <assert.h>
#include <bare.h>
#include <js.h>
#include <stddef.h>
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

void
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

  err = js_close_handle_scope(env, scope);
  assert(err == 0);

  uv_sem_post(&worklet->ready);

  bare_load(bare, worklet->filename, &worklet->source, NULL);

  err = bare_run(bare);
  assert(err == 0);

  uv_mutex_lock(&worklet->lock);

  worklet->bare = NULL;

  uv_mutex_unlock(&worklet->lock);

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
