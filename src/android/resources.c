#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <assert.h>
#include <jni.h>
#include <js.h>
#include <stdlib.h>

#include "../resources.h"
#include "jvm.h"

typedef struct {
  jobject assets;
  AAssetManager *manager;
} bare_kit__resources_t;

static js_value_t *
bare_kit__on_read_resource(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 1;
  js_value_t *argv[1];

  bare_kit__resources_t *resources;
  err = js_get_callback_info(env, info, &argc, argv, NULL, (void **) &resources);
  assert(err == 0);

  assert(argc == 1);

  size_t len;
  err = js_get_value_string_utf8(env, argv[0], NULL, 0, &len);
  assert(err == 0);

  utf8_t *name = malloc(len + 1);
  err = js_get_value_string_utf8(env, argv[0], name, len + 1, NULL);
  assert(err == 0);

  AAsset *asset = AAssetManager_open(resources->manager, (const char *) name, AASSET_MODE_STREAMING);

  free(name);

  js_value_t *result;

  if (asset == NULL) {
    err = js_get_null(env, &result);
    assert(err == 0);

    return result;
  }

  size_t length = AAsset_getLength64(asset);

  char *data;
  err = js_create_arraybuffer(env, length, (void **) &data, &result);
  assert(err == 0);

  for (size_t read = 0; read < length;) {
    int n = AAsset_read(asset, data + read, length - read);
    assert(n > 0);

    read += n;
  }

  AAsset_close(asset);

  return result;
}

static void
bare_kit__on_resources_finalize(js_env_t *env, void *data, void *finalize_hint) {
  bare_kit__resources_t *resources = data;

  JNIEnv *jni = bare_kit__get_env();

  (*jni)->DeleteGlobalRef(jni, resources->assets);

  free(resources);
}

// The code cache directory is cleared when the application is updated, so
// extracted resources never go stale.
void
bare_kit__create_resources(js_env_t *env, const bare_worklet_options_t *options, js_value_t **directory, js_value_t **read) {
  int err;

  err = js_get_null(env, directory);
  assert(err == 0);

  err = js_get_null(env, read);
  assert(err == 0);

  jobject context = (jobject) options->android.context;

  if (context == NULL) return;

  JNIEnv *jni = bare_kit__get_env();

  jclass context_class = (*jni)->GetObjectClass(jni, context);

  jmethodID get_code_cache_dir = (*jni)->GetMethodID(jni, context_class, "getCodeCacheDir", "()Ljava/io/File;");
  assert(get_code_cache_dir != NULL);

  jmethodID get_assets = (*jni)->GetMethodID(jni, context_class, "getAssets", "()Landroid/content/res/AssetManager;");
  assert(get_assets != NULL);

  jobject dir = (*jni)->CallObjectMethod(jni, context, get_code_cache_dir);
  assert(dir != NULL);
  assert(!(*jni)->ExceptionCheck(jni));

  jclass file_class = (*jni)->GetObjectClass(jni, dir);

  jmethodID get_path = (*jni)->GetMethodID(jni, file_class, "getPath", "()Ljava/lang/String;");
  assert(get_path != NULL);

  jstring path = (*jni)->CallObjectMethod(jni, dir, get_path);
  assert(path != NULL);
  assert(!(*jni)->ExceptionCheck(jni));

  const char *chars = (*jni)->GetStringUTFChars(jni, path, NULL);

  err = js_create_string_utf8(env, (const utf8_t *) chars, -1, directory);
  assert(err == 0);

  (*jni)->ReleaseStringUTFChars(jni, path, chars);

  jobject assets = (*jni)->CallObjectMethod(jni, context, get_assets);
  assert(assets != NULL);
  assert(!(*jni)->ExceptionCheck(jni));

  bare_kit__resources_t *resources = malloc(sizeof(bare_kit__resources_t));

  resources->assets = (*jni)->NewGlobalRef(jni, assets);
  resources->manager = AAssetManager_fromJava(jni, resources->assets);

  err = js_create_function(env, "readResource", -1, bare_kit__on_read_resource, resources, read);
  assert(err == 0);

  err = js_add_finalizer(env, *read, resources, bare_kit__on_resources_finalize, NULL, NULL);
  assert(err == 0);

  (*jni)->DeleteLocalRef(jni, assets);
  (*jni)->DeleteLocalRef(jni, path);
  (*jni)->DeleteLocalRef(jni, file_class);
  (*jni)->DeleteLocalRef(jni, dir);
  (*jni)->DeleteLocalRef(jni, context_class);
}
