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

  int fd;

  jclass class;
  jobject self;
} bare_ipc_context_t;

typedef struct {
  bare_ipc_msg_t handle;
} bare_ipc_msg_context_t;

JNIEXPORT jobject JNICALL
Java_to_holepunch_bare_kit_IPC_init(JNIEnv *env, jobject self, jobject jendpoint) {
  int err;

  bare_ipc_context_t *context = malloc(sizeof(bare_ipc_context_t));

  context->handle.data = (void *) context;

  context->env = env;
  context->looper = ALooper_forThread();

  context->class = (*env)->NewGlobalRef(env, (*env)->GetObjectClass(env, self));
  context->self = (*env)->NewGlobalRef(env, self);

  const char *endpoint = (*env)->GetStringUTFChars(env, jendpoint, NULL);

  err = bare_ipc_init(&context->handle, endpoint);
  assert(err == 0);

  (*env)->ReleaseStringUTFChars(env, jendpoint, endpoint);

  context->fd = bare_ipc_fd(&context->handle);

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
Java_to_holepunch_bare_kit_IPC_message(JNIEnv *env, jobject self) {
  bare_ipc_msg_context_t *context = malloc(sizeof(bare_ipc_msg_context_t));

  return (*env)->NewDirectByteBuffer(env, (void *) context, sizeof(bare_ipc_msg_context_t));
}

JNIEXPORT jobject JNICALL
Java_to_holepunch_bare_kit_IPC_read(JNIEnv *env, jobject self, jobject handle, jobject message_handle) {
  int err;

  bare_ipc_t *ipc = (bare_ipc_t *) (*env)->GetDirectBufferAddress(env, handle);

  bare_ipc_msg_t *msg = (bare_ipc_msg_t *) (*env)->GetDirectBufferAddress(env, message_handle);

  void *data;
  size_t len;

  err = bare_ipc_read(ipc, msg, &data, &len);
  assert(err == 0 || err == bare_ipc_would_block);

  if (err == bare_ipc_would_block) {
    return NULL;
  }

  return (*env)->NewDirectByteBuffer(env, data, len);
}

JNIEXPORT jboolean JNICALL
Java_to_holepunch_bare_kit_IPC_write(JNIEnv *env, jobject self, jobject handle, jobject message_handle, jobject jsource, jint jlen) {
  int err;

  bare_ipc_t *ipc = (bare_ipc_t *) (*env)->GetDirectBufferAddress(env, handle);

  bare_ipc_msg_t *msg = (bare_ipc_msg_t *) (*env)->GetDirectBufferAddress(env, message_handle);

  void *data = (*env)->GetDirectBufferAddress(env, jsource);

  int len = (int) jlen;

  err = bare_ipc_write(ipc, msg, data, len);
  assert(err == 0 || err == bare_ipc_would_block);

  if (err == bare_ipc_would_block) {
    return false;
  }

  return true;
}

JNIEXPORT void JNICALL
Java_to_holepunch_bare_kit_IPC_release(JNIEnv *env, jobject self, jobject handle) {
  bare_ipc_msg_t *msg = (bare_ipc_msg_t *) (*env)->GetDirectBufferAddress(env, handle);

  bare_ipc_release(msg);

  free(msg);
}

static int
bare_ipc__on_poll(int fd, int events, void *data) {
  bare_ipc_context_t *context = (bare_ipc_context_t *) data;

  JNIEnv *env = context->env;

  jmethodID poll = (*env)->GetMethodID(env, context->class, "poll", "(I)Z");

  return (*env)->CallBooleanMethod(env, context->self, poll, events);
}

JNIEXPORT void JNICALL
Java_to_holepunch_bare_kit_IPC_poll(JNIEnv *env, jobject self, jobject handle, jint events) {
  int err;

  bare_ipc_context_t *context = (bare_ipc_context_t *) (*env)->GetDirectBufferAddress(env, handle);

  if (events) {
    err = ALooper_addFd(context->looper, context->fd, ALOOPER_POLL_CALLBACK, events, bare_ipc__on_poll, (void *) context);
    assert(err == 1);
  } else {
    err = ALooper_removeFd(context->looper, context->fd);
    assert(err == 1);
  }
}
