#ifndef BARE_KIT_APPLE_SUSPENSION_H
#define BARE_KIT_APPLE_SUSPENSION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdatomic.h>

#include "../suspension.h"

struct bare_suspension_s {
#if defined(BARE_KIT_IOS)
  _Atomic unsigned long task;
#endif
};

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_APPLE_SUSPENSION_H
