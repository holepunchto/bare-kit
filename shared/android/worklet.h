#ifndef BARE_KIT_ANDROID_WORKLET_H
#define BARE_KIT_ANDROID_WORKLET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <jni.h>

#include "../worklet.h"

int
bare_worklet_android_attach_vm(bare_worklet_t *worklet, JavaVM *java_vm);

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_ANDROID_WORKLET_H
