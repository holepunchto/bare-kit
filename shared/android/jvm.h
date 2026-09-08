#ifndef BARE_KIT_ANDROID_JVM_H
#define BARE_KIT_ANDROID_JVM_H

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

void
bare_kit__jvm_set(JavaVM *vm);

JavaVM *
bare_kit__jvm_get(void);

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_ANDROID_JVM_H
