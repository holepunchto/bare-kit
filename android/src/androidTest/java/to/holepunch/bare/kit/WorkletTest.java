package to.holepunch.bare.kit;

import android.os.Looper;
import android.util.Log;
import java.nio.charset.StandardCharsets;
import org.junit.Test;

public class WorkletTest {
  @Test
  public void
  CreateWorklet() {
    Log.v("BareKit", "Creating worklet!");
    Worklet worklet = new Worklet(null);
    Log.v("BareKit", "Worklet created!");

    Log.v("BareKit", "Starting worklet!");
    worklet.start("/app.js", "BareKit.IPC.on('data', (data) => BareKit.IPC.write(data)).write('Hello!')", StandardCharsets.UTF_8, null);
    Log.v("BareKit", "Worklet started!");

    Looper.prepare();

    Looper looper = Looper.myLooper();

    Log.v("BareKit", "Creating IPC!");
    IPC ipc = new IPC(worklet);
    Log.v("BareKit", "IPC created!");

    ipc.readable(() -> {
      Log.v("BareKit", "Reading data 1!");
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
              Log.v("BareKit", "Reading data 2!");
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
}
