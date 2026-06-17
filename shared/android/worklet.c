#include <assert.h>
#include <dlfcn.h>
#include <jni.h>
#include <stddef.h>
#include <uv.h>

#include "../platform.h"
#include "../worklet.h"

typedef jint (*bare_worklet_android_get_created_java_vms_fn)(JavaVM **, jsize, jsize *);

static uv_once_t bare_worklet_android_get_created_java_vms_guard = UV_ONCE_INIT;
static bare_worklet_android_get_created_java_vms_fn bare_worklet_android_get_created_java_vms_fn_ = NULL;

static void
bare_worklet_android_on_get_created_java_vms_init() {
  void *symbol = dlsym(RTLD_DEFAULT, "JNI_GetCreatedJavaVMs");

  if (symbol) {
    bare_worklet_android_get_created_java_vms_fn_ = (bare_worklet_android_get_created_java_vms_fn) symbol;
    return;
  }

  const char *libraries[] = {
    "libart.so",
    "libnativehelper.so",
  };

  for (size_t i = 0; i < sizeof(libraries) / sizeof(libraries[0]); i++) {
    void *handle = dlopen(libraries[i], RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL) continue;

    symbol = dlsym(handle, "JNI_GetCreatedJavaVMs");
    if (symbol == NULL) continue;

    bare_worklet_android_get_created_java_vms_fn_ = (bare_worklet_android_get_created_java_vms_fn) symbol;
    return;
  }

  assert(bare_worklet_android_get_created_java_vms_fn_ != NULL);
}

static bare_worklet_android_get_created_java_vms_fn
bare_worklet_android_get_created_java_vms() {
  uv_once(&bare_worklet_android_get_created_java_vms_guard, bare_worklet_android_on_get_created_java_vms_init);

  return bare_worklet_android_get_created_java_vms_fn_;
}

static uv_once_t bare_worklet_android_java_vm_guard = UV_ONCE_INIT;
static JavaVM *bare_worklet_android_java_vm = NULL;

static void
bare_worklet_android_on_java_vm_init() {
  jsize count = 0;
  int err = bare_worklet_android_get_created_java_vms()(&bare_worklet_android_java_vm, 1, &count);
  assert(err == JNI_OK);
  assert(count == 1);
}

static JavaVM *
bare_worklet_android_get_java_vm() {
  uv_once(&bare_worklet_android_java_vm_guard, bare_worklet_android_on_java_vm_init);

  return bare_worklet_android_java_vm;
}

static void
bare_worklet_android_set_context_class_loader(JNIEnv *env) {
  jclass thread_class = (*env)->FindClass(env, "java/lang/Thread");
  assert(thread_class != NULL);

  jmethodID current_thread = (*env)->GetStaticMethodID(env, thread_class, "currentThread", "()Ljava/lang/Thread;");
  assert(current_thread != NULL);

  jobject thread = (*env)->CallStaticObjectMethod(env, thread_class, current_thread);
  assert(thread != NULL);
  assert(!(*env)->ExceptionCheck(env));

  jclass activity_thread_class = (*env)->FindClass(env, "android/app/ActivityThread");
  assert(activity_thread_class != NULL);

  jmethodID current_application = (*env)->GetStaticMethodID(env, activity_thread_class, "currentApplication", "()Landroid/app/Application;");
  assert(current_application != NULL);

  jobject application = (*env)->CallStaticObjectMethod(env, activity_thread_class, current_application);
  assert(application != NULL);
  assert(!(*env)->ExceptionCheck(env));

  jclass application_class = (*env)->GetObjectClass(env, application);
  assert(application_class != NULL);

  jmethodID get_class_loader = (*env)->GetMethodID(env, application_class, "getClassLoader", "()Ljava/lang/ClassLoader;");
  assert(get_class_loader != NULL);

  jobject class_loader = (*env)->CallObjectMethod(env, application, get_class_loader);
  assert(class_loader != NULL);
  assert(!(*env)->ExceptionCheck(env));

  jmethodID set_context_class_loader = (*env)->GetMethodID(env, thread_class, "setContextClassLoader", "(Ljava/lang/ClassLoader;)V");
  assert(set_context_class_loader != NULL);

  (*env)->CallVoidMethod(env, thread, set_context_class_loader, class_loader);
  assert(!(*env)->ExceptionCheck(env));

  (*env)->DeleteLocalRef(env, class_loader);
  (*env)->DeleteLocalRef(env, application_class);
  (*env)->DeleteLocalRef(env, application);
  (*env)->DeleteLocalRef(env, activity_thread_class);
  (*env)->DeleteLocalRef(env, thread);
  (*env)->DeleteLocalRef(env, thread_class);
}

void
bare_worklet__platform_on_thread_enter(bare_worklet_t *worklet) {
  int err;

  JavaVM *vm = bare_worklet_android_get_java_vm();

  JNIEnv *env;
  err = (*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_6);
  assert(err == JNI_EDETACHED);

  JavaVMAttachArgs args = {
    JNI_VERSION_1_6,
    "bare-worklet",
    NULL,
  };

  err = (*vm)->AttachCurrentThread(vm, &env, &args);
  assert(err == JNI_OK);

  bare_worklet_android_set_context_class_loader(env);
}

void
bare_worklet__platform_on_thread_exit(bare_worklet_t *worklet) {
  JavaVM *vm = bare_worklet_android_get_java_vm();

  JNIEnv *env;
  int err = (*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_6);
  assert(err == JNI_OK);

  err = (*vm)->DetachCurrentThread(vm);
  assert(err == JNI_OK);
}

int
bare_worklet__platform_init(bare_worklet_t *worklet) {
  bare_worklet_android_get_java_vm();

  return 0;
}
