package to.holepunch.bare.kit;

import android.os.Looper;
import android.util.Log;
import java.nio.charset.StandardCharsets;
import org.junit.Test;

public class WorkletTest {
  @Test
  public void
  CreateWorklet() {
    Worklet worklet = new Worklet(null);

    worklet.start("/app.js", "BareKit.IPC.on('data', (data) => BareKit.IPC.write(data)).write('Hello!')", StandardCharsets.UTF_8, null);

    Looper.prepare();

    Looper looper = Looper.myLooper();

    IPC ipc = new IPC(worklet);

    ipc.readable(() -> {
      String data1 = ipc.read(StandardCharsets.UTF_8);

      if (data1 == null) return;

      ipc.readable(null);

      Log.v("BareKit", data1);

      ipc.writable(() -> {
        boolean sent = ipc.write("Hello back!", StandardCharsets.UTF_8);

        if (sent) {
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

              looper.quit();
            });
          }
        }
      });
    });

    Looper.loop();

    ipc.close();
  }
  
  @Test
  public void
  StressIPC() {
    Worklet worklet = new Worklet(null);

    worklet.start("/app.js", "BareKit.IPC.on('data', (data) => BareKit.IPC.write(data))", StandardCharsets.UTF_8, null);

    Looper.prepare();

    Looper looper = Looper.myLooper();

    IPC ipc = new IPC(worklet);
    
    ipc.readable(() -> {
      while (true) {
        String data = ipc.read(StandardCharsets.UTF_8);

        if (data == null) return;

        Log.v("BareKit", data);
      }
    });

    ipc.writable(() -> {
      int i = 0;
      while (i < 10000) {
        boolean sent = ipc.write("Hello!", StandardCharsets.UTF_8);

        if (sent) i++;
        else break;
      }
    });

    Looper.loop();

    ipc.close();
  }
}
