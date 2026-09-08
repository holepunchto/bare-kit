#include <dlfcn.h>
#include <jni.h>
#include <pthread.h>
#include <stddef.h>

#include "jvm.h"

static JavaVM *bare_kit__jvm = NULL;

static pthread_once_t bare_kit__jvm_guard = PTHREAD_ONCE_INIT;

typedef jint (*bare_kit__jni_get_created_java_vms_fn)(JavaVM **, jsize, jsize *);

// `JNI_GetCreatedJavaVMs()` lives in `libnativehelper.so`, which is only a
// public library from API 31. Resolve it at runtime so that the library keeps
// loading on older releases, where the VM is instead expected to be provided
// through `JNI_OnLoad()`.
static void
bare_kit__jvm_resolve(void) {
  if (bare_kit__jvm) return;

  void *handle = dlopen("libnativehelper.so", RTLD_NOW | RTLD_LOCAL);
  if (handle == NULL) return;

  bare_kit__jni_get_created_java_vms_fn get_created_java_vms = (bare_kit__jni_get_created_java_vms_fn) dlsym(handle, "JNI_GetCreatedJavaVMs");
  if (get_created_java_vms == NULL) return;

  JavaVM *vm;
  jsize len;

  jint err = get_created_java_vms(&vm, 1, &len);
  if (err != JNI_OK || len != 1) return;

  bare_kit__jvm = vm;
}

void
bare_kit__jvm_set(JavaVM *vm) {
  bare_kit__jvm = vm;
}

JavaVM *
bare_kit__jvm_get(void) {
  pthread_once(&bare_kit__jvm_guard, bare_kit__jvm_resolve);

  return bare_kit__jvm;
}
