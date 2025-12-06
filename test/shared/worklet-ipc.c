#include <uv.h>

#include "../../shared/ipc.h"
#include "../../shared/worklet.h"

const char *response = "Hello back!";
uv_async_t finished;

void
on_poll(bare_ipc_poll_t *poll, int events) {
  int err;
  void *data;
  size_t len;

  err = bare_ipc_poll_stop(poll);
  assert(err == 0);

  if ((events & bare_ipc_readable) != 0) {
    err = bare_ipc_read(poll->ipc, &data, &len);
    assert(err == 0);

    if (strncmp(data, response, len) == 0) {
      printf("%.*s", (int) len, (char *) data);
      uv_async_send(&finished);
      return;
    } else {
      printf("%.*s\n", (int) len, (char *) data);

      err = bare_ipc_write(poll->ipc, response, strlen(response));
      assert(err >= 0 || err == bare_ipc_would_block);

      if (err != bare_ipc_would_block) {
        err = bare_ipc_read(poll->ipc, &data, &len);
        assert(err == 0 || err == bare_ipc_would_block);

        if (err != bare_ipc_would_block) {
          assert(strncmp(data, response, len) == 0);
          printf("%.*s\n", (int) len, (char *) data);
          uv_async_send(&finished);
          return;
        } else {
          err = bare_ipc_poll_start(poll, bare_ipc_readable, on_poll);
          assert(err == 0);
        }
      } else {
        err = bare_ipc_poll_start(poll, bare_ipc_writable, on_poll);
        assert(err == 0);
      }
    }
  }

  if ((events & bare_ipc_writable) != 0) {
    err = bare_ipc_write(poll->ipc, response, strlen(response));
    assert(err >= 0);

    err = bare_ipc_read(poll->ipc, &data, &len);
    assert(err == 0 || err == bare_ipc_would_block);

    if (err != bare_ipc_would_block) {
      assert(strncmp(data, response, len) == 0);
      printf("%.*s\n", (int) len, (char *) data);
      uv_async_send(&finished);
      return;
    } else {
      err = bare_ipc_poll_start(poll, bare_ipc_readable, on_poll);
      assert(err == 0);
    }
  }
}

void
on_finish(uv_async_t *handle) {
  uv_close((uv_handle_t *) handle, NULL);
}

int
main() {
  int err;

  uv_loop_t *loop = uv_default_loop();
  err = uv_async_init(loop, &finished, on_finish);
  assert(err == 0);

  bare_worklet_t worklet;
  err = bare_worklet_init(&worklet, NULL);
  assert(err == 0);

  char *code = "BareKit.IPC.on('data', (data) => BareKit.IPC.write(data)).write('Hello!')";
  uv_buf_t source = uv_buf_init(code, strlen(code));
  err = bare_worklet_start(&worklet, "app.js", &source, 0, NULL);
  assert(err == 0);

  bare_ipc_t ipc;
  err = bare_ipc_init(&ipc, &worklet);
  assert(err == 0);

  bare_ipc_poll_t poll;
  err = bare_ipc_poll_init(&poll, &ipc);
  assert(err == 0);

  void *data;
  size_t len;
  err = bare_ipc_read(&ipc, &data, &len);
  assert(err == 0 || err == bare_ipc_would_block);

  if (err != bare_ipc_would_block) {
    printf("%.*s\n", (int) len, (char *) data);

    err = bare_ipc_write(&ipc, response, strlen(response));
    assert(err >= 0 || err == bare_ipc_would_block);

    if (err != bare_ipc_would_block) {
      err = bare_ipc_read(&ipc, &data, &len);
      assert(err == 0 || err == bare_ipc_would_block);

      if (err != bare_ipc_would_block) {
        assert(strncmp(data, response, len) == 0);
        printf("%.*s\n", (int) len, (char *) data);
      } else {
        err = bare_ipc_poll_start(&poll, bare_ipc_readable, on_poll);
        assert(err == 0);

        err = uv_run(loop, UV_RUN_DEFAULT);
        assert(err == 0);
      }
    } else {
      err = bare_ipc_poll_start(&poll, bare_ipc_writable, on_poll);
      assert(err == 0);

      err = uv_run(loop, UV_RUN_DEFAULT);
      assert(err == 0);
    }
  } else {
    err = bare_ipc_poll_start(&poll, bare_ipc_readable, on_poll);
    assert(err == 0);

    err = uv_run(loop, UV_RUN_DEFAULT);
    assert(err == 0);
  }

  err = uv_loop_close(loop);
  assert(err == 0);

  err = bare_worklet_terminate(&worklet);
  assert(err == 0);

  bare_ipc_poll_destroy(&poll);

  bare_ipc_destroy(&ipc);

  bare_worklet_destroy(&worklet);
}
