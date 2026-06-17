#ifndef BARE_KIT_PLATFORM_H
#define BARE_KIT_PLATFORM_H

#include "worklet.h"

int
bare_worklet__platform_init(bare_worklet_t *worklet);

void
bare_worklet__platform_on_thread_enter(bare_worklet_t *worklet);

void
bare_worklet__platform_on_thread_exit(bare_worklet_t *worklet);

#endif // BARE_KIT_PLATFORM_H
