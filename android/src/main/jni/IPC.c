#include <assert.h>
#include <jni.h>
#include <stddef.h>
#include <stdlib.h>

#include <android/looper.h>

#include "../../../../shared/ipc.h"
#include "../../../../shared/worklet.h"

typedef struct {
  bare_ipc_t ipc;
  bare_ipc_poll_t poll;

  JNIEnv *env;

  jclass class;
  jobject self;
} bare_ipc_context_t;

JNIEXPORT jobject JNICALL
Java_to_holepunch_bare_kit_IPC_init(JNIEnv *env, jobject self, jobject handle) {
  int err;

  bare_worklet_t *worklet = (bare_worklet_t *) (*env)->GetDirectBufferAddress(env, handle);

  bare_ipc_context_t *context = malloc(sizeof(bare_ipc_context_t));

  context->env = env;
  context->class = (*env)->NewGlobalRef(env, (*env)->GetObjectClass(env, self));
  context->self = (*env)->NewGlobalRef(env, self);

  err = bare_ipc_init(&context->ipc, worklet);

  if (err < 0) {
    (*env)->DeleteGlobalRef(env, context->class);
    (*env)->DeleteGlobalRef(env, context->self);

    free(context);

    return NULL;
  }

  err = bare_ipc_poll_init(&context->poll, &context->ipc);
  assert(err == 0);

  context->poll.data = (void *) context;

  return (*env)->NewDirectByteBuffer(env, (void *) context, sizeof(bare_ipc_context_t));
}

JNIEXPORT void JNICALL
Java_to_holepunch_bare_kit_IPC_close(JNIEnv *env, jobject self, jobject handle) {
  bare_ipc_context_t *context = (bare_ipc_context_t *) (*env)->GetDirectBufferAddress(env, handle);

  bare_ipc_close(&context->ipc);
}

JNIEXPORT void JNICALL
Java_to_holepunch_bare_kit_IPC_destroy(JNIEnv *env, jobject self, jobject handle) {
  bare_ipc_context_t *context = (bare_ipc_context_t *) (*env)->GetDirectBufferAddress(env, handle);

  bare_ipc_poll_destroy(&context->poll);
  bare_ipc_destroy(&context->ipc);

  (*env)->DeleteGlobalRef(env, context->class);
  (*env)->DeleteGlobalRef(env, context->self);

  free(context);
}

JNIEXPORT jint JNICALL
Java_to_holepunch_bare_kit_IPC_read(JNIEnv *env, jobject self, jobject handle, jobject jbuffer) {
  bare_ipc_t *ipc = (bare_ipc_t *) (*env)->GetDirectBufferAddress(env, handle);

  void *data = (*env)->GetDirectBufferAddress(env, jbuffer);
  size_t len = (size_t) (*env)->GetDirectBufferCapacity(env, jbuffer);

  int read = bare_ipc_read(ipc, data, len);
  assert(read >= 0 || read == bare_ipc_would_block);

  return read;
}

JNIEXPORT jint JNICALL
Java_to_holepunch_bare_kit_IPC_write(JNIEnv *env, jobject self, jobject handle, jobject jsource, jint jlen) {
  int err;

  bare_ipc_t *ipc = (bare_ipc_t *) (*env)->GetDirectBufferAddress(env, handle);

  void *data = (*env)->GetDirectBufferAddress(env, jsource);
  int len = (int) jlen;
  err = bare_ipc_write(ipc, data, len);

  if (err == bare_ipc_would_block) return 0;
  if (err < 0) return -1;

  return err;
}

// A single call into Java, as the handlers it runs may close the IPC and free
// the context.
static void
bare_ipc__on_poll(bare_ipc_poll_t *poll, int events) {
  bare_ipc_context_t *context = (bare_ipc_context_t *) poll->data;

  JNIEnv *env = context->env;

  jmethodID method = (*env)->GetMethodID(env, context->class, "poll", "(ZZ)V");

  (*env)->CallVoidMethod(env, context->self, method, (jboolean) ((events & bare_ipc_readable) != 0), (jboolean) ((events & bare_ipc_writable) != 0));
}

JNIEXPORT void JNICALL
Java_to_holepunch_bare_kit_IPC_update(JNIEnv *env, jobject self, jobject handle, jboolean readable, jboolean writable) {
  bare_ipc_context_t *context = (bare_ipc_context_t *) (*env)->GetDirectBufferAddress(env, handle);

  int events = 0;

  if (readable) events |= bare_ipc_readable;
  if (writable) events |= bare_ipc_writable;

  int err;

  if (events) {
    err = bare_ipc_poll_start(&context->poll, events, bare_ipc__on_poll);
    assert(err == 0);
  } else {
    err = bare_ipc_poll_stop(&context->poll);
    assert(err == 0);
  }
}
