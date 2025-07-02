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

  if (options.assets) {
    (*env)->ReleaseStringUTFChars(env, jassets, options.assets);
  }

  return (*env)->NewDirectByteBuffer(env, (void *) context, sizeof(bare_worklet_context_t));
}

static void
bare_worklet__on_finalize(bare_worklet_t *worklet, const uv_buf_t *source, void *finalize_hint) {
  int err;

  bare_worklet_context_t *context = (bare_worklet_context_t *) worklet->data;

  JNIEnv *env;

  err = (*context->vm)->AttachCurrentThread(context->vm, &env, NULL);
  assert(err == 0);

  jobject ref = finalize_hint;

  (*env)->DeleteGlobalRef(env, ref);

  (*context->vm)->DetachCurrentThread(context->vm);
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
    err = bare_worklet_start(worklet, filename, NULL, NULL, NULL, argc, argv);
    assert(err == 0);
  } else {
    jobject ref = (*env)->NewGlobalRef(env, jsource);

    char *base = (*env)->GetDirectBufferAddress(env, jsource);

    int len = (int) jlen;

    uv_buf_t source = uv_buf_init(base, len);

    err = bare_worklet_start(worklet, filename, &source, bare_worklet__on_finalize, ref, argc, argv);
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

  err = bare_worklet_suspend(worklet, linger, NULL);
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

JNIEXPORT jint JNICALL
Java_to_holepunch_bare_kit_Worklet_incoming(JNIEnv *env, jobject self, jobject handle) {
  bare_worklet_t *worklet = (bare_worklet_t *) (*env)->GetDirectBufferAddress(env, handle);

  return worklet->incoming;
}

JNIEXPORT jint JNICALL
Java_to_holepunch_bare_kit_Worklet_outgoing(JNIEnv *env, jobject self, jobject handle) {
  bare_worklet_t *worklet = (bare_worklet_t *) (*env)->GetDirectBufferAddress(env, handle);

  return worklet->outgoing;
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
