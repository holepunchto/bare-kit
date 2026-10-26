#ifndef BARE_KIT_SUSPENSION_H
#define BARE_KIT_SUSPENSION_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bare_suspension_s bare_suspension_t;

struct bare_suspension_s {
#if defined(BARE_KIT_IOS)
  // Retained, as an expiration handler may outlive the suspension.
  void *task;
#endif
};

int
bare_suspension_init(bare_suspension_t *suspension);

int
bare_suspension_start(bare_suspension_t *suspension, int timeout);

int
bare_suspension_end(bare_suspension_t *suspension);

void
bare_suspension_destroy(bare_suspension_t *suspension);

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_SUSPENSION_H
