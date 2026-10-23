#ifndef BARE_KIT_EVENTS_H
#define BARE_KIT_EVENTS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bare_worklet_state_s bare_worklet_state_t;

void
bare_kit__on_thread_enter(bare_worklet_state_t *state);

void
bare_kit__on_thread_exit(bare_worklet_state_t *state);

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_EVENTS_H
