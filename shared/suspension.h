#ifndef BARE_KIT_SUSPENSION_H
#define BARE_KIT_SUSPENSION_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bare_suspension_s bare_suspension_t;

#if defined(BARE_KIT_DARWIN) || defined(BARE_KIT_IOS)
#include "apple/suspension.h"
#endif

#if defined(BARE_KIT_ANDROID)
#include "android/suspension.h"
#endif

#if defined(BARE_KIT_LINUX)
#include "linux/suspension.h"
#endif

#if defined(BARE_KIT_WINDOWS)
#include "windows/suspension.h"
#endif

int
bare_suspension_init(bare_suspension_t *suspension);

int
bare_suspension_start(bare_suspension_t *suspension, int timeout);

int
bare_suspension_end(bare_suspension_t *suspension);

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_SUSPENSION_H
