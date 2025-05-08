#include <assert.h>
#include <jni.h>
#include <stddef.h>
#include <stdlib.h>

#include <android/looper.h>

#include "../../../../shared/ipc.h"
#include "../../../../shared/worklet.h"

typedef struct {
  bare_ipc_t handle;

  JNIEnv *env;
  ALooper *looper;

  int incoming;
  int outgoing;

  jclass class;
  jobject self;
} bare_ipc_context_t;

JNIEXPORT jobject JNICALL
Java_to_holepunch_bare_kit_IPC_init(JNIEnv *env, jobject self, jint jincoming, jint joutgoing) {
  int err;

  bare_ipc_context_t *context = malloc(sizeof(bare_ipc_context_t));

  context->env = env;
  context->looper = ALooper_forThread();
  context->class = (*env)->NewGlobalRef(env, (*env)->GetObjectClass(env, self));
  context->self = (*env)->NewGlobalRef(env, self);
  context->incoming = (int) jincoming;
  context->outgoing = (int) joutgoing;

  err = bare_ipc_init(&context->handle, context->incoming, context->outgoing);
  assert(err == 0);

  return (*env)->NewDirectByteBuffer(env, (void *) context, sizeof(bare_ipc_context_t));
}

JNIEXPORT void JNICALL
Java_to_holepunch_bare_kit_IPC_destroy(JNIEnv *env, jobject self, jobject handle) {
  bare_ipc_context_t *context = (bare_ipc_context_t *) (*env)->GetDirectBufferAddress(env, handle);

  bare_ipc_destroy(&context->handle);

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

static int
bare_ipc__on_readable(int fd, int events, void *data) {
  bare_ipc_context_t *context = (bare_ipc_context_t *) data;

  JNIEnv *env = context->env;

  jmethodID readable = (*env)->GetMethodID(env, context->class, "readable", "()Z");

  return (*env)->CallBooleanMethod(env, context->self, readable);
}

JNIEXPORT void JNICALL
Java_to_holepunch_bare_kit_IPC_readable(JNIEnv *env, jobject self, jobject handle, jboolean reset) {
  bare_ipc_context_t *context = (bare_ipc_context_t *) (*env)->GetDirectBufferAddress(env, handle);

  int err;

  if (reset) {
    err = ALooper_removeFd(context->looper, context->incoming);
  } else {
    err = ALooper_addFd(context->looper, context->incoming, ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT, bare_ipc__on_readable, (void *) context);
  }

  assert(err == 1);
}

static int
bare_ipc__on_writable(int fd, int events, void *data) {
  bare_ipc_context_t *context = (bare_ipc_context_t *) data;

  JNIEnv *env = context->env;

  jmethodID writable = (*env)->GetMethodID(env, context->class, "writable", "()Z");

  return (*env)->CallBooleanMethod(env, context->self, writable);
}

JNIEXPORT void JNICALL
Java_to_holepunch_bare_kit_IPC_writable(JNIEnv *env, jobject self, jobject handle, jboolean reset) {
  bare_ipc_context_t *context = (bare_ipc_context_t *) (*env)->GetDirectBufferAddress(env, handle);

  int err;

  if (reset) {
    err = ALooper_removeFd(context->looper, context->outgoing);
  } else {
    err = ALooper_addFd(context->looper, context->outgoing, ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_OUTPUT, bare_ipc__on_writable, (void *) context);
  }

  assert(err == 1);
}
