#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <uv.h>

#include <bare-kit.h>

const char *response = "Hello back!";
uv_async_t finished;

void
on_poll(bare_ipc_poll_t *poll, int events) {
  int err;
  char data[BARE_IPC_READ_BUFFER_SIZE];
  int len;

  err = bare_ipc_poll_stop(poll);
  assert(err == 0);

  if ((events & bare_ipc_readable) != 0) {
    len = bare_ipc_read(bare_ipc_poll_get_ipc(poll), data, sizeof(data));
    assert(len > 0);

    if (strncmp(data, response, len) == 0) {
      printf("%.*s", (int) len, (char *) data);
      uv_async_send(&finished);
      return;
    } else {
      printf("%.*s\n", (int) len, (char *) data);

      err = bare_ipc_write(bare_ipc_poll_get_ipc(poll), response, strlen(response));
      assert(err >= 0 || err == bare_ipc_would_block);

      if (err != bare_ipc_would_block) {
        len = bare_ipc_read(bare_ipc_poll_get_ipc(poll), data, sizeof(data));
        assert(len >= 0 || len == bare_ipc_would_block);

        if (len != bare_ipc_would_block) {
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
    err = bare_ipc_write(bare_ipc_poll_get_ipc(poll), response, strlen(response));
    assert(err >= 0);

    len = bare_ipc_read(bare_ipc_poll_get_ipc(poll), data, sizeof(data));
    assert(len >= 0 || len == bare_ipc_would_block);

    if (len != bare_ipc_would_block) {
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

  bare_worklet_t *worklet;
  err = bare_worklet_create(NULL, &worklet);
  assert(err == 0);

  char *code = "BareKit.IPC.on('data', (data) => BareKit.IPC.write(data)).write('Hello!')";
  err = bare_worklet_start(worklet, "app.js", code, strlen(code), 0, NULL);
  assert(err == 0);

  bare_ipc_t *ipc;
  err = bare_ipc_create(worklet, &ipc);
  assert(err == 0);

  bare_ipc_poll_t *poll;
  err = bare_ipc_poll_create(ipc, &poll);
  assert(err == 0);

  char data[BARE_IPC_READ_BUFFER_SIZE];
  int len;
  len = bare_ipc_read(ipc, data, sizeof(data));
  assert(len >= 0 || len == bare_ipc_would_block);

  if (len != bare_ipc_would_block) {
    printf("%.*s\n", (int) len, (char *) data);

    err = bare_ipc_write(ipc, response, strlen(response));
    assert(err >= 0 || err == bare_ipc_would_block);

    if (err != bare_ipc_would_block) {
      len = bare_ipc_read(ipc, data, sizeof(data));
      assert(len >= 0 || len == bare_ipc_would_block);

      if (len != bare_ipc_would_block) {
        assert(strncmp(data, response, len) == 0);
        printf("%.*s\n", (int) len, (char *) data);
      } else {
        err = bare_ipc_poll_start(poll, bare_ipc_readable, on_poll);
        assert(err == 0);

        err = uv_run(loop, UV_RUN_DEFAULT);
        assert(err == 0);
      }
    } else {
      err = bare_ipc_poll_start(poll, bare_ipc_writable, on_poll);
      assert(err == 0);

      err = uv_run(loop, UV_RUN_DEFAULT);
      assert(err == 0);
    }
  } else {
    err = bare_ipc_poll_start(poll, bare_ipc_readable, on_poll);
    assert(err == 0);

    err = uv_run(loop, UV_RUN_DEFAULT);
    assert(err == 0);
  }

  // Before the loop closes: the worklet's exit makes this end readable, and the
  // poll's callback posts to this loop.
  bare_ipc_poll_destroy(poll);

  err = uv_loop_close(loop);
  assert(err == 0);

  err = bare_worklet_terminate(worklet);
  assert(err == 0);

  bare_ipc_destroy(ipc);

  bare_worklet_destroy(worklet);
}
