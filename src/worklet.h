#ifndef BARE_KIT_WORKLET_H
#define BARE_KIT_WORKLET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <bare-kit.h>
#include <bare.h>
#include <js.h>
#include <utf.h>
#include <uv.h>

#include "queue.h"
#include "suspension.h"

typedef struct bare_worklet_state_s bare_worklet_state_t;

struct bare_worklet_s {
  bare_worklet_options_t options;

  bare_worklet_state_t *state;

  const char *filename;

  struct {
    enum {
      bare_worklet_source_none = 0,
      bare_worklet_source_buffer = 1,
    } type;

    union {
      uv_buf_t buffer;
    };
  } source;

  int argc;
  const char **argv;

  uv_thread_t thread;
  uv_barrier_t ready;
  uv_sem_t *finished;

  bare_queue_t *queue;

  void *data;
};

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_WORKLET_H
