package to.holepunch.bare.kit;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.util.Log;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicInteger;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

public class WorkletTest {
  @Test
  public void
  CreateWorklet() throws InterruptedException {
    HandlerThread thread = new HandlerThread("CreateWorklet");
    thread.start();

    Handler handler = new Handler(thread.getLooper());

    CountDownLatch latch = new CountDownLatch(1);

    handler.post(() -> {
      Looper looper = Looper.myLooper();

      Worklet worklet = new Worklet(null);
      worklet.start("/app.js", "BareKit.IPC.on('data', (data) => BareKit.IPC.write(data)).write('Hello!')", StandardCharsets.UTF_8, null);

      IPC ipc = new IPC(worklet);

      ipc.readable(() -> {
        String data1 = ipc.read(StandardCharsets.UTF_8);

        if (data1 == null) return;

        ipc.readable(null);

        Log.v("BareKit", data1);

        ipc.writable(() -> {
          boolean success = ipc.write("Hello back!", StandardCharsets.UTF_8);

          if (success) {
            ipc.writable(null);

            String data2 = ipc.read(StandardCharsets.UTF_8);

            if (data2 != null) {
              Log.v("BareKit", data2);

              looper.quit();
            } else {
              ipc.readable(() -> {
                String data3 = ipc.read(StandardCharsets.UTF_8);

                if (data3 == null) return;

                ipc.readable(null);

                Log.v("BareKit", data3);

                ipc.close();

                worklet.terminate();

                latch.countDown();
              });
            }
          }
        });
      });
    });

    latch.await();

    thread.quit();
  }

  @Test
  public void
  StressIPC() throws InterruptedException {
    HandlerThread thread = new HandlerThread("StressIPC");
    thread.start();

    Handler handler = new Handler(thread.getLooper());

    CountDownLatch latch = new CountDownLatch(1);

    handler.post(() -> {
      Looper looper = Looper.myLooper();

      Worklet worklet = new Worklet(null);
      worklet.start("/app.js", "BareKit.IPC.on('data', (data) => BareKit.IPC.write(data))", StandardCharsets.UTF_8, null);

      IPC ipc = new IPC(worklet);

      StringBuilder buffer = new StringBuilder();

      AtomicInteger received = new AtomicInteger(0);

      ipc.readable(() -> {
        while (true) {
          String data = ipc.read(StandardCharsets.UTF_8);

          if (data == null) break;

          buffer.append(data);

          int i;

          while ((i = buffer.indexOf("\n")) != -1) {
            String message = buffer.substring(0, i);

            buffer.delete(0, i + 1);

            if (message.isEmpty()) continue;

            received.incrementAndGet();
          }
        }

        if (received.get() == 10000) {
          Log.v("BareKit", "Read " + received.get() + " messages");

          ipc.readable(null);
          ipc.close();

          worklet.terminate();

          latch.countDown();
        }
      });

      AtomicInteger sent = new AtomicInteger(0);

      ipc.writable(() -> {
        while (sent.get() < 10000) {
          boolean success = ipc.write("Hello!\n", StandardCharsets.UTF_8);

          if (success) sent.incrementAndGet();
          else break;
        }

        if (sent.get() == 10000) {
          Log.v("BareKit", "Wrote " + sent.get() + " messages");

          ipc.writable(null);
        }
      });
    });

    latch.await();

    thread.quit();
  }
}
