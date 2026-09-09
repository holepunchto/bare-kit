#include <jni.h>

#include "../../../../shared/android/jvm.h"

JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reserved) {
  bare_kit__jvm_set(vm);

  return JNI_VERSION_1_6;
}
