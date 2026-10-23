#ifndef BARE_KIT_APPLE_SUSPENSION_H
#define BARE_KIT_APPLE_SUSPENSION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../suspension.h"

struct bare_suspension_s {
#if defined(BARE_KIT_IOS)
  // Retained, as an expiration handler may outlive the suspension.
  void *task;
#endif
};

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_APPLE_SUSPENSION_H
