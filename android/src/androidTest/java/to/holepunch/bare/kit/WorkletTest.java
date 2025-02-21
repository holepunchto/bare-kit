package to.holepunch.bare.kit;

import android.os.Looper;
import android.util.Log;
import java.nio.charset.StandardCharsets;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import java.util.concurrent.atomic.AtomicInteger;

public class WorkletTest {
    private Looper looper;

    @Before
    public void setup() {
        if (Looper.myLooper() == null) {
            Looper.prepare();
            looper = Looper.myLooper();
        }
    }

    @After
    public void teardown() {
        if (looper != null) {
            looper.quit();
        }
    }

    // @Test
    // public void CreateWorklet() {
    //     Log.v("BareKit", "Creating worklet!");
    //     Worklet worklet = new Worklet(null);
    //     Log.v("BareKit", "Worklet created!");
    //     Log.v("BareKit", "Starting worklet!");
    //     worklet.start("/app.js", "BareKit.IPC.on('data', (data) => BareKit.IPC.write(data)).write('Hello!')", StandardCharsets.UTF_8, null);
    //     Log.v("BareKit", "Worklet started!");
    //     
    //     Log.v("BareKit", "Creating IPC!");
    //     IPC ipc = new IPC(worklet);
    //     Log.v("BareKit", "IPC created!");
    //     
    //     ipc.readable(() -> {
    //         Log.v("BareKit", "Reading data 1!");
    //         String data1 = ipc.read(StandardCharsets.UTF_8);
    //         if (data1 == null) return;
    //         ipc.readable(null);
    //         Log.v("BareKit", data1);
    //         ipc.writable(() -> {
    //             boolean sent = ipc.write("Hello back!", StandardCharsets.UTF_8);
    //             if (sent) {
    //                 ipc.writable(null);
    //                 String data2 = ipc.read(StandardCharsets.UTF_8);
    //                 if (data2 != null) {
    //                     Log.v("BareKit", data2);
    //                     looper.quit();
    //                 } else {
    //                     ipc.readable(() -> {
    //                         Log.v("BareKit", "Reading data 2!");
    //                         String data3 = ipc.read(StandardCharsets.UTF_8);
    //                         if (data3 == null) return;
    //                         ipc.readable(null);
    //                         Log.v("BareKit", data3);
    //                         looper.quit();
    //                     });
    //                 }
    //             }
    //         });
    //     });
    //     
    //     Looper.loop();
    //     ipc.close();
    // }

  @Test
  public void StressIPC() {
      Worklet worklet = new Worklet(null);
      worklet.start("/app.js", "BareKit.IPC.on('data', (data) => BareKit.IPC.write(data + '\\n'))", StandardCharsets.UTF_8, null);
      
      IPC ipc = new IPC(worklet);
      
      StringBuilder buffer = new StringBuilder();  // ✅ Accumulates data
      AtomicInteger messageCount = new AtomicInteger(0); // ✅ Counts messages
      
      ipc.readable(() -> {
          Log.v("BareKit", "Reading data!");
          while (true) {
              String chunk = ipc.read(StandardCharsets.UTF_8);
              if (chunk == null) break;  // No more data, stop reading

              buffer.append(chunk); // ✅ Append to the buffer

              // Process full messages
              int newlineIndex;
              while ((newlineIndex = buffer.indexOf("\n")) != -1) {
                  String message = buffer.substring(0, newlineIndex);
                  buffer.delete(0, newlineIndex + 1);  // ✅ Remove processed message
                  
                  if (message.isEmpty()) continue;  // Skip empty messages
                  messageCount.incrementAndGet();
                  // Log.v("BareKit", "Received: " + message);
              }
          }
          Log.v("BareKit", "Done reading " + messageCount.get() + " messages!");
          Looper.myLooper().quit();  // ✅ Quit the loop after reading everything
      });

      ipc.writable(() -> {
          Log.v("BareKit", "Writing data!");
          int i = 0;
          while (i < 10000) {
              boolean sent = ipc.write("Hello!\n", StandardCharsets.UTF_8);
              if (sent) i++;
              else break;
          }
          // ipc.writable(null);
          Log.v("BareKit", "Done writing " + i + " messages!");
      });

      Looper.loop();
      ipc.close();
  }
}
