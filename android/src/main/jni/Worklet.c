#include <assert.h>
#include <jni.h>
#include <stdlib.h>

#include "../../../../shared/worklet.h"

typedef struct {
  bare_worklet_t worklet;

  JavaVM *vm;
} bare_worklet_context_t;

typedef struct {
  bare_worklet_push_t req;

  JavaVM *vm;

  jclass class;
  jobject payload;
  jobject callback;
} bare_worklet_push_context_t;

static void
bare_worklet_set_context_class_loader(JNIEnv *env) {
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
bare_worklet_on_thread_enter(void **thread_data, void *data) {
  int err;

  bare_worklet_context_t *context = (bare_worklet_context_t *) data;

  JNIEnv *env;
  err = (*context->vm)->GetEnv(context->vm, (void **) &env, JNI_VERSION_1_6);

  if (err == JNI_EDETACHED) {
    JavaVMAttachArgs args = {
      JNI_VERSION_1_6,
      "bare-worklet",
      NULL,
    };

    err = (*context->vm)->AttachCurrentThread(context->vm, &env, &args);
    assert(err == JNI_OK);

    *thread_data = context->vm;
  } else {
    assert(err == JNI_OK);
    *thread_data = NULL;
  }

  bare_worklet_set_context_class_loader(env);
}

static void
bare_worklet_on_thread_exit(void *thread_data, void *data) {
  (void) data;

  if (thread_data == NULL) return;

  JavaVM *vm = (JavaVM *) thread_data;
  int err = (*vm)->DetachCurrentThread(vm);
  assert(err == JNI_OK);
}

JNIEXPORT jobject JNICALL
Java_to_holepunch_bare_kit_Worklet_init(JNIEnv *env, jobject self, jint jmemory_limit, jobject jassets) {
  int err;

  bare_worklet_context_t *context = malloc(sizeof(bare_worklet_context_t));

  context->worklet.data = (void *) context;

  (*env)->GetJavaVM(env, &context->vm);

  bare_worklet_options_t options;

  options.memory_limit = (int) jmemory_limit;

  if ((*env)->IsSameObject(env, jassets, NULL)) {
    options.assets = NULL;
  } else {
    options.assets = (*env)->GetStringUTFChars(env, jassets, NULL);
  }

  err = bare_worklet_init(&context->worklet, &options);
  assert(err == 0);

  err = bare_worklet_on_thread_enter(&context->worklet, bare_worklet_on_thread_enter, context);
  assert(err == 0);

  err = bare_worklet_on_thread_exit(&context->worklet, bare_worklet_on_thread_exit, NULL);
  assert(err == 0);

  if (options.assets) {
    (*env)->ReleaseStringUTFChars(env, jassets, options.assets);
  }

  return (*env)->NewDirectByteBuffer(env, (void *) context, sizeof(bare_worklet_context_t));
}

JNIEXPORT void JNICALL
Java_to_holepunch_bare_kit_Worklet_start(JNIEnv *env, jobject self, jobject handle, jstring jfilename, jobject jsource, jint jlen, jobjectArray jarguments) {
  int err;

  bare_worklet_t *worklet = (bare_worklet_t *) (*env)->GetDirectBufferAddress(env, handle);

  const char *filename = (*env)->GetStringUTFChars(env, jfilename, NULL);

  int argc = (*env)->IsSameObject(env, jarguments, NULL) ? 0 : (*env)->GetArrayLength(env, jarguments);

  const char **argv = calloc(argc, sizeof(char *));

  for (int i = 0; i < argc; i++) {
    jstring arg = (jstring) (*env)->GetObjectArrayElement(env, jarguments, i);

    argv[i] = (*env)->GetStringUTFChars(env, arg, NULL);
  }

  if ((*env)->IsSameObject(env, jsource, NULL)) {
    err = bare_worklet_start(worklet, filename, NULL, argc, argv);
    assert(err == 0);
  } else {
    char *base = (*env)->GetDirectBufferAddress(env, jsource);

    int len = (int) jlen;

    uv_buf_t source = uv_buf_init(base, len);

    err = bare_worklet_start(worklet, filename, &source, argc, argv);
    assert(err == 0);
  }

  for (int i = 0; i < argc; i++) {
    jstring arg = (jstring) (*env)->GetObjectArrayElement(env, jarguments, i);

    (*env)->ReleaseStringUTFChars(env, arg, argv[i]);
  }

  (*env)->ReleaseStringUTFChars(env, jfilename, filename);
}

JNIEXPORT void JNICALL
Java_to_holepunch_bare_kit_Worklet_suspend(JNIEnv *env, jobject self, jobject handle, jint jlinger) {
  int err;

  int linger = (int) jlinger;

  bare_worklet_t *worklet = (bare_worklet_t *) (*env)->GetDirectBufferAddress(env, handle);

  err = bare_worklet_suspend(worklet, linger);
  assert(err == 0);
}

JNIEXPORT void JNICALL
Java_to_holepunch_bare_kit_Worklet_resume(JNIEnv *env, jobject self, jobject handle) {
  int err;

  bare_worklet_t *worklet = (bare_worklet_t *) (*env)->GetDirectBufferAddress(env, handle);

  err = bare_worklet_resume(worklet);
  assert(err == 0);
}

JNIEXPORT void JNICALL
Java_to_holepunch_bare_kit_Worklet_terminate(JNIEnv *env, jobject self, jobject handle) {
  int err;

  bare_worklet_t *worklet = (bare_worklet_t *) (*env)->GetDirectBufferAddress(env, handle);

  err = bare_worklet_terminate(worklet);
  assert(err == 0);

  bare_worklet_destroy(worklet);

  free(worklet);
}

static void
bare_worklet__on_push(bare_worklet_push_t *req, const char *error, const uv_buf_t *reply) {
  int err;

  bare_worklet_push_context_t *context = (bare_worklet_push_context_t *) req->data;

  JNIEnv *env;

  err = (*context->vm)->AttachCurrentThread(context->vm, &env, NULL);
  assert(err == 0);

  jmethodID apply = (*env)->GetMethodID(env, context->class, "apply", "(Ljava/nio/ByteBuffer;Ljava/lang/String;)V");

  jobject jerror;

  if (error) {
    jerror = (*env)->NewStringUTF(env, error);
  } else {
    jerror = NULL;
  }

  jobject jreply;

  if (reply) {
    jreply = (*env)->NewDirectByteBuffer(env, (void *) reply->base, reply->len);
  } else {
    jreply = NULL;
  }

  (*env)->CallVoidMethod(env, context->callback, apply, jreply, jerror);

  (*env)->DeleteGlobalRef(env, context->class);
  (*env)->DeleteGlobalRef(env, context->payload);
  (*env)->DeleteGlobalRef(env, context->callback);

  (*context->vm)->DetachCurrentThread(context->vm);

  free(req);
}

JNIEXPORT void JNICALL
Java_to_holepunch_bare_kit_Worklet_push(JNIEnv *env, jobject self, jobject handle, jobject jpayload, jint jlen, jobject jcallback) {
  int err;

  bare_worklet_t *worklet = (bare_worklet_t *) (*env)->GetDirectBufferAddress(env, handle);

  char *base = (*env)->GetDirectBufferAddress(env, jpayload);

  int len = (int) jlen;

  uv_buf_t payload = uv_buf_init(base, len);

  bare_worklet_push_context_t *context = malloc(sizeof(bare_worklet_push_context_t));

  context->req.data = (void *) context;

  (*env)->GetJavaVM(env, &context->vm);

  context->class = (*env)->NewGlobalRef(env, (*env)->GetObjectClass(env, jcallback));
  context->payload = (*env)->NewGlobalRef(env, jpayload);
  context->callback = (*env)->NewGlobalRef(env, jcallback);

  err = bare_worklet_push(worklet, &context->req, &payload, bare_worklet__on_push);
  assert(err == 0);
}
