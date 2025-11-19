#include <uv.h>

#include "../../shared/worklet.h"

int
main() {
  int e;

  bare_worklet_t worklet;
  e = bare_worklet_init(&worklet, NULL);
  assert(e == 0);

  char *code = "console.log('Hello world')";

  uv_buf_t source = uv_buf_init(code, strlen(code));

  e = bare_worklet_start(&worklet, "app.js", &source, NULL, NULL);
  assert(e == 0);

  e = bare_worklet_suspend(&worklet, 0);
  assert(e == 0);

  uv_sleep(100); // Let suspension flush

  e = bare_worklet_resume(&worklet);
  assert(e == 0);

  uv_sleep(100); // Let resumption flush

  e = bare_worklet_terminate(&worklet);
  assert(e == 0);

  bare_worklet_destroy(&worklet);
}
