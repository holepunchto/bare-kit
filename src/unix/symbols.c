#define _GNU_SOURCE

#include <dlfcn.h>
#include <stddef.h>

#include "../symbols.h"

void
bare_kit__expose_symbols(void) {
  // Addons resolve the symbols of Bare from the global scope, so promote the
  // library to it in case it was loaded with `RTLD_LOCAL`.
  Dl_info info;

  if (dladdr((void *) bare_kit__expose_symbols, &info) == 0 || info.dli_fname == NULL) return;

  void *library = dlopen(info.dli_fname, RTLD_LAZY | RTLD_NOLOAD | RTLD_GLOBAL);

  if (library) dlclose(library);
}
