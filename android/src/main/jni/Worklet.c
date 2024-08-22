#include <assert.h>
#include <jni.h>
#include <log.h>
#include <stdlib.h>

#include <android/file_descriptor_jni.h>

#include "../../../../shared/worklet.h"

typedef struct {
  bare_worklet_push_t req;

  JavaVM *vm;

  jclass class;
  jobject payload;
  jobject callback;
} bare_worklet_push_context_t;

JNIEXPORT jobject JNICALL
Java_to_holepunch_bare_kit_Worklet_init (JNIEnv *env, jobject self) {
  int err;

  err = log_open("bare", 0);
  assert(err == 0);

  bare_worklet_t *worklet = malloc(sizeof(bare_worklet_t));

  jobject handle = (*env)->NewDirectByteBuffer(env, (void *) worklet, sizeof(bare_worklet_t));

  err = bare_worklet_init(worklet, NULL);
  assert(err == 0);

  return handle;
}

JNIEXPORT void JNICALL
Java_to_holepunch_bare_kit_Worklet_start (JNIEnv *env, jobject self, jobject handle, jstring jfilename, jobject jsource, jint jlen) {
  int err;

  bare_worklet_t *worklet = (bare_worklet_t *) (*env)->GetDirectBufferAddress(env, handle);

  const char *filename = (*env)->GetStringUTFChars(env, jfilename, NULL);

  char *base = (*env)->GetDirectBufferAddress(env, jsource);

  int len = (int) jlen;

  uv_buf_t source = uv_buf_init(base, len);

  err = bare_worklet_start(worklet, filename, &source);
  assert(err == 0);

  (*env)->ReleaseStringUTFChars(env, jfilename, filename);
}

JNIEXPORT void JNICALL
Java_to_holepunch_bare_kit_Worklet_suspend (JNIEnv *env, jobject self, jobject handle, jint jlinger) {
  int err;

  int linger = (int) jlinger;

  bare_worklet_t *worklet = (bare_worklet_t *) (*env)->GetDirectBufferAddress(env, handle);

  err = bare_worklet_suspend(worklet, linger);
  assert(err == 0);
}

JNIEXPORT void JNICALL
Java_to_holepunch_bare_kit_Worklet_resume (JNIEnv *env, jobject self, jobject handle) {
  int err;

  bare_worklet_t *worklet = (bare_worklet_t *) (*env)->GetDirectBufferAddress(env, handle);

  err = bare_worklet_resume(worklet);
  assert(err == 0);
}

JNIEXPORT void JNICALL
Java_to_holepunch_bare_kit_Worklet_terminate (JNIEnv *env, jobject self, jobject handle) {
  int err;

  bare_worklet_t *worklet = (bare_worklet_t *) (*env)->GetDirectBufferAddress(env, handle);

  err = bare_worklet_terminate(worklet);
  assert(err == 0);

  bare_worklet_destroy(worklet);

  free(worklet);

  err = log_close();
  assert(err == 0);
}

JNIEXPORT jobject JNICALL
Java_to_holepunch_bare_kit_Worklet_incoming (JNIEnv *env, jobject self, jobject handle) {
  int err;

  bare_worklet_t *worklet = (bare_worklet_t *) (*env)->GetDirectBufferAddress(env, handle);

  jobject fd = AFileDescriptor_create(env);

  if ((*env)->ExceptionCheck(env)) return NULL;

  AFileDescriptor_setFd(env, fd, worklet->incoming);

  return fd;
}

JNIEXPORT jobject JNICALL
Java_to_holepunch_bare_kit_Worklet_outgoing (JNIEnv *env, jobject self, jobject handle) {
  int err;

  bare_worklet_t *worklet = (bare_worklet_t *) (*env)->GetDirectBufferAddress(env, handle);

  jobject fd = AFileDescriptor_create(env);

  if ((*env)->ExceptionCheck(env)) return NULL;

  AFileDescriptor_setFd(env, fd, worklet->outgoing);

  return fd;
}

static void
bare_worklet__on_push (bare_worklet_push_t *req, const char *error, const uv_buf_t *reply) {
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
Java_to_holepunch_bare_kit_Worklet_push (JNIEnv *env, jobject self, jobject handle, jobject jpayload, jint jlen, jobject jcallback) {
  int err;

  bare_worklet_t *worklet = (bare_worklet_t *) (*env)->GetDirectBufferAddress(env, handle);

  char *base = (*env)->GetDirectBufferAddress(env, jpayload);

  int len = (int) jlen;

  uv_buf_t payload = uv_buf_init(base, len);

  bare_worklet_push_context_t *context = malloc(sizeof(bare_worklet_push_context_t));

  (*env)->GetJavaVM(env, &context->vm);

  context->class = (*env)->NewGlobalRef(env, (*env)->FindClass(env, "to/holepunch/bare/kit/Worklet$NativePushCallback"));
  context->payload = (*env)->NewGlobalRef(env, jpayload);
  context->callback = (*env)->NewGlobalRef(env, jcallback);

  context->req.data = (void *) context;

  err = bare_worklet_push(worklet, &context->req, &payload, bare_worklet__on_push);
  assert(err == 0);
}
