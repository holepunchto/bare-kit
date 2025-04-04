package to.holepunch.bare.kit;

import android.os.Handler;
import android.os.Looper;
import java.io.Closeable;
import java.io.FileDescriptor;
import java.io.IOException;
import java.io.InputStream;
import java.lang.InterruptedException;
import java.nio.ByteBuffer;
import java.nio.channels.Channels;
import java.nio.channels.ReadableByteChannel;
import java.nio.charset.Charset;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

public class Worklet implements Closeable {
  static {
    System.loadLibrary("bare-kit");
  }

  @FunctionalInterface
  public interface PushCallback<T> {
    void
    apply(T reply, Throwable exception);
  }

  @FunctionalInterface
  private interface NativePushCallback {
    void
    apply(ByteBuffer reply, String exception);
  }

  public static class Options {
    public int memoryLimit = 0;
    public String assets = null;

    public Options
    memoryLimit(int memoryLimit) {
      this.memoryLimit = memoryLimit;
      return this;
    }

    public Options
    assets(String assets) {
      this.assets = assets;
      return this;
    }
  }

  private ByteBuffer handle;
  int incoming;
  int outgoing;

  public Worklet(Options options) {
    if (options == null) options = new Options();

    handle = init(options.memoryLimit, options.assets);
  }

  private native ByteBuffer
  init(int memoryLimit, String assets);

  private native void
  start(ByteBuffer handle, String filename, ByteBuffer source, int len, String[] arguments);

  private native void
  suspend(ByteBuffer handle, int linger);

  private native void
  resume(ByteBuffer handle);

  private native void
  terminate(ByteBuffer handle);

  private native int
  incoming(ByteBuffer handle);

  private native int
  outgoing(ByteBuffer handle);

  private native void
  push(ByteBuffer handle, ByteBuffer payload, int len, NativePushCallback callback);

  private void
  start(String filename, ByteBuffer source, int len, String[] arguments) {
    start(handle, filename, source, len, arguments);

    incoming = incoming(handle);
    outgoing = outgoing(handle);
  }

  public void
  start(String filename, ByteBuffer source, String[] arguments) {
    ByteBuffer buffer;

    if (source == null) {
      start(filename, null, 0, arguments);
    } else {
      if (source.isDirect()) {
        buffer = source;
      } else {
        buffer = ByteBuffer.allocateDirect(source.limit());
        buffer.put(source);
        buffer.flip();
      }

      start(filename, buffer, buffer.limit(), arguments);
    }
  }

  public void
  start(String filename, String source, Charset charset, String[] arguments) {
    start(filename, ByteBuffer.wrap(source.getBytes(charset)), arguments);
  }

  public void
  start(String filename, String source, String charset, String[] arguments) {
    start(filename, source, Charset.forName(charset), arguments);
  }

  public void
  start(String filename, InputStream source, String[] arguments) throws IOException {
    source.reset();

    ByteBuffer buffer = ByteBuffer.allocateDirect(Math.max(4096, source.available()));

    ReadableByteChannel channel = Channels.newChannel(source);

    while (channel.read(buffer) != -1) {
      if (buffer.hasRemaining()) continue;

      buffer.flip();

      ByteBuffer resized = ByteBuffer.allocateDirect(buffer.capacity() * 2);
      resized.put(buffer);

      buffer = resized;
    }

    buffer.flip();
    channel.close();

    start(filename, buffer, buffer.limit(), arguments);
  }

  public void
  suspend() {
    suspend(0);
  }

  public void
  suspend(int linger) {
    if (linger <= 0) linger = 30000;

    suspend(handle, linger);
  }

  public void
  resume() {
    resume(handle);
  }

  public void
  terminate() {
    terminate(handle);

    handle = null;
  }

  private void
  push(ByteBuffer payload, int len, Looper looper, PushCallback<ByteBuffer> callback) {
    Handler handler = Handler.createAsync(looper);

    push(handle, payload, len, (reply, error) -> {
      if (reply != null) {
        ByteBuffer buffer = ByteBuffer.allocateDirect(reply.limit());
        buffer.put(reply);
        buffer.flip();

        handler.post(() -> callback.apply(buffer, null));
      } else if (error != null) {
        Error exception = new Error(error);

        handler.post(() -> callback.apply(null, exception));
      } else {
        handler.post(() -> callback.apply(null, null));
      }
    });
  }

  private ByteBuffer
  push(ByteBuffer payload, int len, long timeout, TimeUnit unit) throws InterruptedException {
    final CountDownLatch latch = new CountDownLatch(1);

    final AtomicReference<ByteBuffer> result = new AtomicReference<>();
    final AtomicReference<Error> exception = new AtomicReference<>();

    push(handle, payload, len, (reply, error) -> {
      if (reply != null) {
        ByteBuffer buffer = ByteBuffer.allocateDirect(reply.limit());
        buffer.put(reply);
        buffer.flip();

        result.set(buffer);
      } else if (error != null) {
        exception.set(new Error(error));
      }

      latch.countDown();
    });

    latch.await(timeout, unit);

    if (exception.get() != null) throw exception.get();

    return result.get();
  }

  public void
  push(ByteBuffer payload, Looper looper, PushCallback<ByteBuffer> callback) {
    ByteBuffer buffer;

    if (payload.isDirect()) {
      buffer = payload;
    } else {
      buffer = ByteBuffer.allocateDirect(payload.limit());
      buffer.put(payload);
      buffer.flip();
    }

    push(buffer, buffer.limit(), looper, callback);
  }

  public void
  push(ByteBuffer payload, PushCallback<ByteBuffer> callback) {
    push(payload, Looper.myLooper(), callback);
  }

  public ByteBuffer
  push(ByteBuffer payload, long timeout, TimeUnit unit) throws InterruptedException {
    ByteBuffer buffer;

    if (payload.isDirect()) {
      buffer = payload;
    } else {
      buffer = ByteBuffer.allocateDirect(payload.limit());
      buffer.put(payload);
      buffer.flip();
    }

    return push(buffer, buffer.limit(), timeout, unit);
  }

  public void
  push(String payload, Charset charset, Looper looper, PushCallback<String> callback) {
    push(ByteBuffer.wrap(payload.getBytes(charset)), (reply, error) -> {
      callback.apply(reply == null ? null : charset.decode(reply).toString(), error);
    });
  }

  public void
  push(String payload, Charset charset, PushCallback<String> callback) {
    push(payload, charset, Looper.myLooper(), callback);
  }

  public String
  push(String payload, Charset charset, long timeout, TimeUnit unit) throws InterruptedException {
    ByteBuffer reply = push(ByteBuffer.wrap(payload.getBytes(charset)), timeout, unit);

    if (reply == null) return null;

    return charset.decode(reply).toString();
  }

  public void
  push(String payload, String charset, Looper looper, PushCallback<String> callback) {
    push(payload, Charset.forName(charset), callback);
  }

  public void
  push(String payload, String charset, PushCallback<String> callback) {
    push(payload, charset, Looper.myLooper(), callback);
  }

  public String
  push(String payload, String charset, long timeout, TimeUnit unit) throws InterruptedException {
    return push(payload, Charset.forName(charset), timeout, unit);
  }

  public void
  close() {
    terminate();
  }
}
