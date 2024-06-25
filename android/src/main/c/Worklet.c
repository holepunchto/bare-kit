#include <assert.h>
#include <jni.h>
#include <stdlib.h>

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
Java_to_holepunch_bare_kit_Worklet_start (JNIEnv *env, jobject self, jobject handle, jstring filename, jobject source) {
  bare_worklet_t *worklet = (bare_worklet_t *) (*env)->GetDirectBufferAddress(env, handle);

  // TODO
}

JNIEXPORT void JNICALL
Java_to_holepunch_bare_kit_Worklet_suspend (JNIEnv *env, jobject self, jobject handle, jint linger) {
  int err;

  bare_worklet_t *worklet = (bare_worklet_t *) (*env)->GetDirectBufferAddress(env, handle);

  err = bare_worklet_suspend(worklet, (int) linger);
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
