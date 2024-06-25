#ifndef BARE_KIT_WORKLET_H
#define BARE_KIT_WORKLET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <bare.h>
#include <uv.h>

typedef struct bare_worklet_s bare_worklet_t;

struct bare_worklet_s {
  bare_t *bare;

  const char *filename;
  uv_buf_t source;

  uv_thread_t thread;
  uv_sem_t ready;
  uv_mutex_t lock;

  uv_file incoming;
  uv_file outgoing;
};

int
bare_worklet_init (bare_worklet_t *worklet);

void
bare_worklet_destroy (bare_worklet_t *worklet);

int
bare_worklet_start (bare_worklet_t *worklet, const char *filename, const uv_buf_t *source);

int
bare_worklet_suspend (bare_worklet_t *worklet, int linger);

int
bare_worklet_resume (bare_worklet_t *worklet);

int
bare_worklet_terminate (bare_worklet_t *worklet);

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_WORKLET_H
