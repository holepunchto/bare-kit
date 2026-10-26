#include <assert.h>
#include <jni.h>
#include <stdbool.h>
#include <stddef.h>

#include "../platform.h"

static JavaVM *_Atomic bare_kit__jvm = NULL;

JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reserved) {
  bare_kit__jvm = vm;

  return JNI_VERSION_1_6;
}

static void
bare_kit__set_context_class_loader(JNIEnv *env) {
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
bare_kit__on_thread_enter(void) {
  int err;

  JavaVM *vm = bare_kit__jvm;
  assert(vm != NULL);

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

  bare_kit__set_context_class_loader(env);
}

void
bare_kit__on_thread_exit(void) {
  int err;

  JavaVM *vm = bare_kit__jvm;
  assert(vm != NULL);

  JNIEnv *env;
  err = (*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_6);
  assert(err == JNI_OK);

  err = (*vm)->DetachCurrentThread(vm);
  assert(err == JNI_OK);
}

void *
bare_kit__retain_context(void *context) {
  int err;

  JavaVM *vm = bare_kit__jvm;
  assert(vm != NULL);

  JNIEnv *env;
  err = (*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_6);
  assert(err == JNI_OK);

  return (*env)->NewGlobalRef(env, (jobject) context);
}

// The worklet may be destroyed from a thread that isn't attached to the JVM,
// in which case the thread is attached for the duration of the call.
void
bare_kit__release_context(void *context) {
  int err;

  JavaVM *vm = bare_kit__jvm;
  assert(vm != NULL);

  JNIEnv *env;
  err = (*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_6);

  bool attached = err == JNI_EDETACHED;

  if (attached) {
    err = (*vm)->AttachCurrentThread(vm, &env, NULL);
  }

  assert(err == JNI_OK);

  (*env)->DeleteGlobalRef(env, (jobject) context);

  if (attached) {
    err = (*vm)->DetachCurrentThread(vm);
    assert(err == JNI_OK);
  }
}

static void
bare_kit__on_context_destroy(const char *key, void *value) {
  int err;

  JavaVM *vm = bare_kit__jvm;

  JNIEnv *env;
  err = (*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_6);
  assert(err == JNI_OK);

  (*env)->DeleteGlobalRef(env, (jobject) value);
}

void
bare_kit__publish_context(bare_t *bare, const bare_worklet_options_t *options) {
  int err;

  JavaVM *vm = bare_kit__jvm;
  assert(vm != NULL);

  err = bare_context_set(bare, "bare.android.jvm.v1", vm, NULL);
  assert(err == 0);

  if (options->android.context) {
    JNIEnv *env;
    err = (*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_6);
    assert(err == JNI_OK);

    err = bare_context_set(bare, "bare.android.context.v1", (*env)->NewGlobalRef(env, (jobject) options->android.context), bare_kit__on_context_destroy);
    assert(err == 0);
  }
}
