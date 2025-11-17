#include "../../shared/worklet.h"

/* On windows this test fails only because windows
 * version of `close()` throws on bad fd rather than return -1
 */
#if defined(BARE_KIT_WINDOWS)
#include <crtdbg.h>

void
invalid_parameter_handler(
  const wchar_t *expression,
  const wchar_t *function,
  const wchar_t *file,
  unsigned int line,
  uintptr_t pReserved
) {}
#endif

int
main() {
#if defined(BARE_KIT_WINDOWS)
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
  _set_invalid_parameter_handler(invalid_parameter_handler);
#endif

  int e;

  bare_worklet_t worklet;
  e = bare_worklet_init(&worklet, NULL);
  assert(e == 0);

  bare_worklet_destroy(&worklet);
}
