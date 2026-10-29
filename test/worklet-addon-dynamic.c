#include <assert.h>
#include <stdatomic.h>
#include <string.h>

#include <bare-kit.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

// A native addon loaded by the worklet resolves the symbols of Bare from the
// shared library, which the host loads at runtime rather than links. The host
// links nothing of Bare Kit, so the addon can only bind to the library.

static atomic_bool exited;
static int exit_code;

static void
on_worklet_exit(int code, void *data) {
  exit_code = code;

  atomic_store(&exited, true);
}

static void *
load(const char *name) {
#if defined(_WIN32)
  static HMODULE library = NULL;

  if (library == NULL) library = LoadLibraryA(BARE_KIT);
  assert(library != NULL);

  return (void *) GetProcAddress(library, name);
#else
  static void *library = NULL;

  if (library == NULL) library = dlopen(BARE_KIT, RTLD_NOW | RTLD_LOCAL);
  assert(library != NULL);

  return dlsym(library, name);
#endif
}

static void
sleep_briefly(void) {
#if defined(_WIN32)
  Sleep(10);
#else
  usleep(10000);
#endif
}

int
main() {
  int e;

  int (*worklet_create)(const bare_worklet_options_t *, bare_worklet_t **) = load("bare_worklet_create");
  int (*worklet_on_exit)(bare_worklet_t *, bare_worklet_exit_cb, void *) = load("bare_worklet_on_exit");
  int (*worklet_start)(bare_worklet_t *, const char *, const void *, size_t, int, const char *[]) = load("bare_worklet_start");
  void (*worklet_destroy)(bare_worklet_t *) = load("bare_worklet_destroy");
  int (*worklet_teardown)(void) = load("bare_worklet_teardown");
  int (*ipc_create)(bare_worklet_t *, bare_ipc_t **) = load("bare_ipc_create");
  int (*ipc_read)(bare_ipc_t *, void *, size_t) = load("bare_ipc_read");
  void (*ipc_end)(bare_ipc_t *) = load("bare_ipc_end");
  void (*ipc_destroy)(bare_ipc_t *) = load("bare_ipc_destroy");

  bare_worklet_t *worklet;
  e = worklet_create(NULL, &worklet);
  assert(e == 0);

  e = worklet_on_exit(worklet, on_worklet_exit, NULL);
  assert(e == 0);

  bare_ipc_t *ipc;
  e = ipc_create(worklet, &ipc);
  assert(e == 0);

  char *code = "Bare.IPC.end(new Bare.Addon(new URL(Bare.argv[0])).exports)";

  const char *argv[] = {ADDON};

  e = worklet_start(worklet, "app.js", code, strlen(code), 1, argv);
  assert(e == 0);

  char data[BARE_IPC_READ_BUFFER_SIZE];
  size_t received = 0;
  int len;

  while ((len = ipc_read(ipc, data + received, sizeof(data) - received)) != 0) {
    if (len == bare_ipc_would_block) sleep_briefly();
    else received += len;
  }

  assert(received == strlen("Hello from addon"));
  assert(memcmp(data, "Hello from addon", received) == 0);

  ipc_end(ipc);

  while (!atomic_load(&exited))
    sleep_briefly();

  assert(exit_code == 0);

  ipc_destroy(ipc);

  worklet_destroy(worklet);

  e = worklet_teardown();
  assert(e == 0);
}
