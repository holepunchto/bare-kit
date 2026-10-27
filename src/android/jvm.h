#ifndef BARE_KIT_ANDROID_JVM_H
#define BARE_KIT_ANDROID_JVM_H

#include <jni.h>

/**
 * The environment of the calling thread, which must be attached to the JVM.
 */
JNIEnv *
bare_kit__get_env(void);

#endif // BARE_KIT_ANDROID_JVM_H
