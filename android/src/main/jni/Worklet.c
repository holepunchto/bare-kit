#include <assert.h>
#include <jni.h>
#include <stdlib.h>

#include <android/file_descriptor_jni.h>

#include "../../../../shared/worklet.h"

JNIEXPORT jobject JNICALL
Java_to_holepunch_bare_kit_Worklet_init (JNIEnv *env, jobject self) {
  int err;

  bare_worklet_t *worklet = malloc(sizeof(bare_worklet_t));

  jobject handle = (*env)->NewDirectByteBuffer(env, (void *) worklet, sizeof(bare_worklet_t));

  err = bare_worklet_init(worklet);
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
