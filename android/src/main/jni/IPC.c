#include <assert.h>
#include <jni.h>
#include <stddef.h>
#include <stdlib.h>

#include <android/looper.h>

#include "../../../../shared/android/ipc.h"
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
Java_to_holepunch_bare_kit_IPC_init(JNIEnv *env, jobject self, jint incoming, jint outgoing) {
  int err;

  bare_ipc_context_t *context = malloc(sizeof(bare_ipc_context_t));

  context->poll.data = (void *) context;

  context->env = env;
  context->class = (*env)->NewGlobalRef(env, (*env)->GetObjectClass(env, self));
  context->self = (*env)->NewGlobalRef(env, self);

  err = bare_ipc_init(&context->ipc, (int) incoming, (int) outgoing);
  assert(err == 0);

  err = bare_ipc_poll_init(&context->poll, &context->ipc);
  assert(err == 0);

  return (*env)->NewDirectByteBuffer(env, (void *) context, sizeof(bare_ipc_context_t));
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

JNIEXPORT jobject JNICALL
Java_to_holepunch_bare_kit_IPC_read(JNIEnv *env, jobject self, jobject handle) {
  int err;

  bare_ipc_t *ipc = (bare_ipc_t *) (*env)->GetDirectBufferAddress(env, handle);

  void *data;
  size_t len;
  err = bare_ipc_read(ipc, &data, &len);
  assert(err == 0 || err == bare_ipc_would_block);

  if (err == bare_ipc_would_block) {
    return NULL;
  }

  return (*env)->NewDirectByteBuffer(env, data, len);
}

JNIEXPORT jint JNICALL
Java_to_holepunch_bare_kit_IPC_write(JNIEnv *env, jobject self, jobject handle, jobject jsource, jint jlen) {
  int err;

  bare_ipc_t *ipc = (bare_ipc_t *) (*env)->GetDirectBufferAddress(env, handle);

  void *data = (*env)->GetDirectBufferAddress(env, jsource);
  int len = (int) jlen;
  err = bare_ipc_write(ipc, data, len);
  assert(err >= 0 || err == bare_ipc_would_block);

  if (err == bare_ipc_would_block) {
    return 0;
  }

  return err;
}

static void
bare_ipc__on_poll(bare_ipc_poll_t *poll, int events) {
  bare_ipc_context_t *context = (bare_ipc_context_t *) poll->data;

  JNIEnv *env = context->env;

  if (events & bare_ipc_readable) {
    jmethodID readable = (*env)->GetMethodID(env, context->class, "readable", "()V");

    (*env)->CallVoidMethod(env, context->self, readable);
  }

  if (events & bare_ipc_writable) {
    jmethodID writable = (*env)->GetMethodID(env, context->class, "writable", "()V");

    (*env)->CallVoidMethod(env, context->self, writable);
  }
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
