#include "../../shared/worklet.h"

#if defined(BARE_KIT_WINDOWS)
#include <crtdbg.h>
#include <stdlib.h>

void
invalid_parameter_handler(
  const wchar_t *expression,
  const wchar_t *function,
  const wchar_t *file,
  unsigned int line,
  uintptr_t pReserved
) {};
#endif

int
main() {
  int e;

#if defined(BARE_KIT_WINDOWS)
  _set_invalid_parameter_handler(invalid_parameter_handler);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
#endif

  bare_worklet_t worklet;
  e = bare_worklet_init(&worklet, NULL);
  assert(e == 0);

  bare_worklet_destroy(&worklet);
}
