package to.holepunch.bare.kit;

import android.os.Handler;
import android.os.Looper;
import java.io.Closeable;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.Channels;
import java.nio.channels.ReadableByteChannel;
import java.nio.channels.WritableByteChannel;
import java.nio.charset.Charset;
import java.util.Arrays;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import to.holepunch.bare.kit.Worklet;

public class IPC implements Closeable {
  @FunctionalInterface
  public interface ReadCallback<T> {
    void
    apply (T result, Throwable exception);
  }

  @FunctionalInterface
  public interface WriteCallback {
    void
    apply (Throwable exception);
  }

  private static WriteCallback defaultWriteCallback = (exception) -> {
    if (exception != null) throw new RuntimeException(exception);
  };

  private FileInputStream incoming;
  private FileOutputStream outgoing;

  private ReadableByteChannel reader;
  private WritableByteChannel writer;

  private Handler handler;
  private ExecutorService executor;

  private ByteBuffer buffer;

  public IPC(FileDescriptor incoming, FileDescriptor outgoing) {
    this.incoming = new FileInputStream(incoming);
    this.outgoing = new FileOutputStream(outgoing);

    reader = Channels.newChannel(this.incoming);
    writer = Channels.newChannel(this.outgoing);

    handler = Handler.createAsync(Looper.getMainLooper());
    executor = Executors.newWorkStealingPool();

    buffer = ByteBuffer.allocateDirect(65536);
  }

  public IPC(Worklet worklet) {
    this(worklet.incoming(), worklet.outgoing());
  }

  public void
  read (ReadCallback<ByteBuffer> callback) {
    executor.submit(() -> {
      try {
        int read = reader.read(buffer);

        if (read <= 0) {
          handler.post(() -> callback.apply(null, null));
        } else {
          buffer.flip();

          ByteBuffer result = ByteBuffer.allocateDirect(read);
          result.put(buffer);
          result.flip();

          handler.post(() -> callback.apply(result, null));
        }

        buffer.clear();
      } catch (IOException exception) {
        handler.post(() -> callback.apply(null, exception));
      }
    });
  }

  public void
  read (Charset charset, ReadCallback<String> callback) {
    executor.submit(() -> {
      try {
        int read = reader.read(buffer);

        if (read <= 0) {
          handler.post(() -> callback.apply(null, null));
        } else {
          buffer.flip();

          String result = charset.decode(buffer).toString();

          handler.post(() -> callback.apply(result, null));
        }

        buffer.clear();
      } catch (IOException exception) {
        handler.post(() -> callback.apply(null, exception));
      }
    });
  }

  public void
  read (String charset, ReadCallback<String> callback) {
    read(Charset.forName(charset), callback);
  }

  public void
  write (ByteBuffer data, WriteCallback callback) {
    executor.submit(() -> {
      try {
        writer.write(data);

        handler.post(() -> callback.apply(null));
      } catch (IOException exception) {
        handler.post(() -> callback.apply(exception));
      }
    });
  }

  public void
  write (ByteBuffer data) {
    write(data, defaultWriteCallback);
  }

  public void
  write (String data, Charset charset, WriteCallback callback) {
    write(ByteBuffer.wrap(data.getBytes(charset)), callback);
  }

  public void
  write (String data, Charset charset) {
    write(data, charset, defaultWriteCallback);
  }

  public void
  write (String data, String charset, WriteCallback callback) {
    write(data, Charset.forName(charset), callback);
  }

  public void
  write (String data, String charset) {
    write(data, charset, defaultWriteCallback);
  }

  public void
  close () throws IOException {
    executor.shutdown();

    try {
      if (!executor.awaitTermination(5, TimeUnit.SECONDS)) {
        executor.shutdownNow();
      }
    } catch (InterruptedException exception) {
      executor.shutdownNow();

      Thread.currentThread().interrupt();
    }

    reader.close();
    writer.close();

    incoming.close();
    outgoing.close();
  }
}
