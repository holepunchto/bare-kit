#include <assert.h>
#include <jni.h>

#include "../platform.h"
#include "../worklet.h"
#include "worklet.h"

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

static void
bare_worklet_android_on_thread_enter(void *data) {
  int err;

  JavaVM *vm = (JavaVM *) data;

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

static void
bare_worklet_android_on_thread_exit(void *data) {
  JavaVM *vm = (JavaVM *) data;

  JNIEnv *env;
  int err = (*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_6);
  assert(err == JNI_OK);

  err = (*vm)->DetachCurrentThread(vm);
  assert(err == JNI_OK);
}

int
bare_worklet_android_attach_vm(bare_worklet_t *worklet, JavaVM *vm) {
  int err;

  if (vm == NULL) return -1;

  err = bare_worklet_on_thread_enter(worklet, bare_worklet_android_on_thread_enter, vm);
  if (err < 0) return err;

  err = bare_worklet_on_thread_exit(worklet, bare_worklet_android_on_thread_exit, vm);
  if (err < 0) return err;

  return 0;
}

void
bare_worklet__platform_on_thread_enter(bare_worklet_t *worklet) {}

void
bare_worklet__platform_on_thread_exit(bare_worklet_t *worklet) {}

int
bare_worklet__platform_init(bare_worklet_t *worklet) {
  return 0;
}
