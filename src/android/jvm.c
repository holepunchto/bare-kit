#include <jni.h>
#include <stddef.h>

#include "jvm.h"

static JavaVM *bare_kit__jvm = NULL;

void
bare_kit__jvm_set(JavaVM *vm) {
  bare_kit__jvm = vm;
}

JavaVM *
bare_kit__jvm_get(void) {
  return bare_kit__jvm;
}

JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reserved) {
  bare_kit__jvm_set(vm);

  return JNI_VERSION_1_6;
}
