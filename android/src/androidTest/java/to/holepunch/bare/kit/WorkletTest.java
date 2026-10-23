package to.holepunch.bare.kit;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.util.Log;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicInteger;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

public class WorkletTest {
  @Test
  public void
  IPC() throws InterruptedException {
    HandlerThread thread = new HandlerThread("IPC");
    thread.start();

    Handler handler = new Handler(thread.getLooper());

    CountDownLatch latch = new CountDownLatch(1);

    handler.post(() -> {
      Looper looper = Looper.myLooper();

      Worklet worklet = new Worklet(null);
      worklet.start("/app.js", "BareKit.IPC.on('data', (data) => BareKit.IPC.write(data)).write('Hello!')", StandardCharsets.UTF_8, null);

      IPC ipc = new IPC(worklet);

      ipc.read((data1, exception1) -> {
        Log.v("BareKit", StandardCharsets.UTF_8.decode(data1).toString());

        ipc.write(ByteBuffer.wrap("Hello back!".getBytes(StandardCharsets.UTF_8)), (exception2) -> {
          ipc.read((data2, exception3) -> {
            Log.v("BareKit", StandardCharsets.UTF_8.decode(data2).toString());

            worklet.terminate();

            latch.countDown();
          });
        });
      });
    });

    latch.await();

    thread.quit();
  }

  @Test
  public void
  IPCLargeWrite() throws InterruptedException {
    HandlerThread thread = new HandlerThread("IPCLargeWrite");
    thread.start();

    Handler handler = new Handler(thread.getLooper());

    CountDownLatch latch = new CountDownLatch(1);

    handler.post(() -> {
      Looper looper = Looper.myLooper();

      Worklet worklet = new Worklet(null);
      worklet.start("/app.js", "BareKit.IPC.on('data', (data) => BareKit.IPC.write(data))", StandardCharsets.UTF_8, null);

      IPC ipc = new IPC(worklet);

      ipc.write(ByteBuffer.allocateDirect(4 * 1024 * 1024), (exception) -> {
        final AtomicInteger received = new AtomicInteger(0);

        ipc.readable(() -> {
          ByteBuffer data = ipc.read();

          if (received.addAndGet(data.limit()) == 4 * 1024 * 1024) {
            worklet.terminate();

            latch.countDown();
          }
        });
      });
    });

    latch.await();

    thread.quit();
  }

  @Test
  public void
  Push() throws InterruptedException {
    HandlerThread thread = new HandlerThread("Push");
    thread.start();

    Handler handler = new Handler(thread.getLooper());

    CountDownLatch latch = new CountDownLatch(1);

    handler.post(() -> {
      Looper looper = Looper.myLooper();

      Worklet worklet = new Worklet(null);
      worklet.start(
        "/app.js",
        "BareKit.on('push', (json, reply) => {console.log(json.toString()); reply(null, 'Hello, world!')})",
        StandardCharsets.UTF_8,
        null
      );

      worklet.push("Push message", StandardCharsets.UTF_8, looper, (reply, exception) -> {
        if (reply == null) return;

        Log.v("BareKit", reply);

        worklet.terminate();

        latch.countDown();
      });
    });

    latch.await();

    thread.quit();
  }

  @Test
  public void
  IPCAfterTerminate() throws InterruptedException {
    HandlerThread thread = new HandlerThread("IPCAfterTerminate");
    thread.start();

    Handler handler = new Handler(thread.getLooper());

    CountDownLatch latch = new CountDownLatch(1);

    handler.post(() -> {
      Worklet worklet = new Worklet(null);
      worklet.start("/app.js", "", StandardCharsets.UTF_8, null);
      worklet.terminate();

      try {
        new IPC(worklet);
      } catch (IllegalStateException exception) {
        latch.countDown();
      }
    });

    latch.await();

    thread.quit();
  }

  @Test
  public void
  IPCCloseOffLooper() throws InterruptedException {
    HandlerThread thread = new HandlerThread("IPCCloseOffLooper");
    thread.start();

    Handler handler = new Handler(thread.getLooper());

    CountDownLatch opened = new CountDownLatch(1);
    CountDownLatch closed = new CountDownLatch(1);

    final Worklet[] worklet = new Worklet[1];
    final IPC[] ipc = new IPC[1];

    handler.post(() -> {
      worklet[0] = new Worklet(null);
      worklet[0].start("/app.js", "BareKit.IPC.on('data', () => {})", StandardCharsets.UTF_8, null);

      ipc[0] = new IPC(worklet[0]);

      ipc[0].readable(() -> {});

      opened.countDown();
    });

    opened.await();

    ipc[0].close();
    ipc[0].close();

    handler.post(() -> {
      ByteBuffer data = ipc[0].read();

      if (data != null && data.limit() == 0) closed.countDown();

      worklet[0].terminate();
    });

    closed.await();

    thread.quit();
  }

  @Test
  public void
  PushAfterTerminate() throws InterruptedException {
    HandlerThread thread = new HandlerThread("PushAfterTerminate");
    thread.start();

    Handler handler = new Handler(thread.getLooper());

    CountDownLatch latch = new CountDownLatch(1);

    handler.post(() -> {
      Looper looper = Looper.myLooper();

      Worklet worklet = new Worklet(null);
      worklet.start("/app.js", "", StandardCharsets.UTF_8, null);
      worklet.terminate();
      worklet.terminate();

      worklet.push("Push message", StandardCharsets.UTF_8, looper, (reply, exception) -> {
        if (exception != null) latch.countDown();
      });
    });

    latch.await();

    thread.quit();
  }
}
