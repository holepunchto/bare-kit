# bare-kit

Bare Kit is a C library that embeds Bare in a host application, running each JavaScript entry point in an isolated thread, called a worklet, with an in-process IPC channel to the host. It aims to be a "one size fits most" embedder that language bindings build on it rather than embedding Bare themselves.

## Usage

```c
#include <bare-kit.h>
#include <stdio.h>
#include <string.h>
#include <uv.h>

static uv_sem_t exited;

static void
on_worklet_exit(int exit_code, void *data) {
  printf("Exited with code %d\n", exit_code);

  uv_sem_post(&exited);
}

static void
on_readable(bare_ipc_poll_t *poll, int events) {
  bare_ipc_t *ipc = bare_ipc_poll_get_ipc(poll);

  char data[BARE_IPC_READ_BUFFER_SIZE];

  int len = bare_ipc_read(ipc, data, sizeof(data));

  if (len > 0) printf("%.*s\n", len, data);
  else if (len == 0) bare_ipc_close(ipc);
}

int
main() {
  uv_sem_init(&exited, 0);

  bare_worklet_options_t options = {
    .version = 0,
    .memory_limit = 24 * 1024 * 1024,
  };

  bare_worklet_t *worklet;
  bare_worklet_create(&options, &worklet);

  bare_worklet_on_exit(worklet, on_worklet_exit, NULL);

  bare_ipc_t *ipc;
  bare_ipc_create(worklet, &ipc);

  bare_ipc_poll_t *poll;
  bare_ipc_poll_create(ipc, &poll);
  bare_ipc_poll_start(poll, bare_ipc_readable, on_readable);

  char *code = "Bare.IPC.on('data', (data) => Bare.IPC.end(data))";

  bare_worklet_start(worklet, "/app.js", code, strlen(code), 0, NULL);

  bare_ipc_write(ipc, "Hello world", 11);

  uv_sem_wait(&exited);

  bare_ipc_poll_destroy(poll);
  bare_ipc_destroy(ipc);
  bare_worklet_destroy(worklet);
}
```

The worklet echoes what the host writes and then ends its side of the IPC. The host prints the echo, closes its own side once it reads the end of the stream, and waits for the worklet to exit.

If `source` is `NULL`, the contents of `filename` are instead read at runtime. A `.bundle` file with assets has them unpacked into the directory given by the `assets` option before it runs.

The API is documented in [`include/bare-kit.h`](include/bare-kit.h).

## Lifecycle

`bare_worklet_start()` runs the worklet on a thread of its own. The worklet exits when its event loop runs out of work, when it calls `Bare.exit()`, or when the host calls `bare_worklet_terminate()`. An open IPC keeps the worklet running until both the worklet and the host have ended their side of it, just like a pipe would.

Once the worklet has exited and torn down, the exit callback is called exactly once on the worklet thread with the exit code. This also happens when the host destroys a running worklet, so the callback is the place to release whatever the host associated with it. By then, any IPC of the worklet has reached the end of the stream, and suspending, resuming, waking up, or terminating the worklet does nothing.

`bare_worklet_destroy()` never blocks. It terminates the worklet if it is still running and hands it over to tear itself down, and it may be called before or after destroying its IPC.

## IPC

The worklet side of the IPC is a duplex stream available as `Bare.IPC`. The host side is read and written with `bare_ipc_read()` and `bare_ipc_write()`, which never block: they return `bare_ipc_would_block` when there is nothing to read or no room to write. A poll calls the host back when the IPC becomes readable or writable. Ending either side is a half-close, so the other side reads the end of the stream but may keep writing.

Poll callbacks run on a thread owned by the IPC rather than the thread that started the poll, on every platform. A callback may stop or destroy its poll, or the IPC itself.

## Suspension

`bare_worklet_suspend()`, `bare_worklet_resume()`, and `bare_worklet_wakeup()` forward the lifecycle of the host application to the worklet, which observes them as the `suspend`, `resume`, and `wakeup` events of `Bare`. A suspension may linger for a number of milliseconds before the worklet goes idle. On iOS, the worklet holds a background task for as long as it lingers.

## Error handling

A worklet runs as a thread inside the host process, so it follows Bare's default handling of uncaught errors: an uncaught exception or unhandled promise rejection in the worklet is printed and the process is aborted. Because the worklet shares the process of the host, this takes the host down with it. This is by design.

To keep a worklet error from aborting the host, register an `uncaughtException` and an `unhandledRejection` listener inside the worklet, and report the error to the host before exiting:

```js
Bare.on('uncaughtException', (err) => {
  Bare.IPC.write(String(err))

  Bare.exit(1)
})
```

See [`uncaughtException`](https://github.com/holepunchto/bare#bareonuncaughtexception-err) and [`unhandledRejection`](https://github.com/holepunchto/bare#bareonunhandledrejection-reason-promise) for the underlying behavior.

## License

Apache-2.0
