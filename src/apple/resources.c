#include <CoreFoundation/CoreFoundation.h>
#include <assert.h>
#include <js.h>
#include <limits.h>
#include <stdbool.h>

#include "../resources.h"

static bool
bare_kit__has_extension(CFURLRef url, CFStringRef extension) {
  CFStringRef actual = CFURLCopyPathExtension(url);

  if (actual == NULL) return false;

  bool equal = CFStringCompare(actual, extension, 0) == kCFCompareEqualTo;

  CFRelease(actual);

  return equal;
}

// An application extension is embedded in the bundle of its application, whose
// resources it shares. An executable outside of a bundle has no resources.
static CFURLRef
bare_kit__copy_application_url(void) {
  CFBundleRef main = CFBundleGetMainBundle();

  if (main == NULL) return NULL;

  CFURLRef url = CFBundleCopyBundleURL(main);

  if (url == NULL) return NULL;

  if (bare_kit__has_extension(url, CFSTR("appex"))) {
    while (!bare_kit__has_extension(url, CFSTR("app"))) {
      CFURLRef parent = CFURLCreateCopyDeletingLastPathComponent(NULL, url);

      bool root = CFEqual(parent, url);

      CFRelease(url);

      url = parent;

      if (root) break;
    }
  }

  if (bare_kit__has_extension(url, CFSTR("app"))) return url;

  CFRelease(url);

  return NULL;
}

void
bare_kit__create_resources(js_env_t *env, const bare_worklet_options_t *options, js_value_t **directory, js_value_t **read) {
  int err;

  err = js_get_null(env, directory);
  assert(err == 0);

  err = js_get_null(env, read);
  assert(err == 0);

  CFURLRef url = bare_kit__copy_application_url();

  if (url == NULL) return;

  CFBundleRef bundle = CFBundleCreate(NULL, url);

  CFRelease(url);

  if (bundle == NULL) return;

  CFURLRef resources = CFBundleCopyResourcesDirectoryURL(bundle);

  CFRelease(bundle);

  if (resources == NULL) return;

  CFURLRef absolute = CFURLCopyAbsoluteURL(resources);

  CFRelease(resources);

  char path[PATH_MAX];

  bool ok = CFURLGetFileSystemRepresentation(absolute, true, (UInt8 *) path, PATH_MAX);

  CFRelease(absolute);

  if (!ok) return;

  err = js_create_string_utf8(env, (const utf8_t *) path, -1, directory);
  assert(err == 0);
}
