#include <assert.h>
#include <jni.h>
#include <stdlib.h>

#include <android/looper.h>

#include "../../../../shared/ipc.h"
#include "../../../../shared/worklet.h"

typedef struct {
  bare_ipc_t handle;

  ALooper *looper;
} bare_ipc_context_t;

JNIEXPORT jobject JNICALL
Java_to_holepunch_bare_kit_IPC_init(JNIEnv *env, jobject self, jobject jendpoint) {
  int err;

  bare_ipc_context_t *context = malloc(sizeof(bare_ipc_context_t));

  context->handle.data = (void *) context;

  const char *endpoint = (*env)->GetStringUTFChars(env, jendpoint, NULL);

  err = bare_ipc_init(&context->handle, endpoint);
  assert(err == 0);

  (*env)->ReleaseStringUTFChars(env, jendpoint, endpoint);

  jobject handle = (*env)->NewDirectByteBuffer(env, (void *) context, sizeof(bare_ipc_context_t));

  return handle;
}

JNIEXPORT void JNICALL
Java_to_holepunch_bare_kit_IPC_close(JNIEnv *env, jobject self, jobject handle) {
  bare_ipc_t *ipc = (bare_ipc_t *) (*env)->GetDirectBufferAddress(env, handle);

  bare_ipc_destroy(ipc);

  free(ipc);
}
