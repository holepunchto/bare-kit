#include <assert.h>
#include <jni.h>

#include "../context.h"
#include "jvm.h"

void
bare_kit__publish_context(bare_t *bare) {
  int err;

  JavaVM *vm = bare_kit__jvm_get();
  assert(vm != NULL);

  err = bare_context_set(bare, "bare.android.jvm.v1", vm, NULL);
  assert(err == 0);
}
