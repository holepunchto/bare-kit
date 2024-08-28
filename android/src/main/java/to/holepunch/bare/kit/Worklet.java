package to.holepunch.bare.kit;

import android.os.Handler;
import android.os.Looper;
import java.io.Closeable;
import java.io.FileDescriptor;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.channels.Channels;
import java.nio.channels.ReadableByteChannel;
import java.nio.charset.Charset;

public class Worklet implements Closeable {
  static {
    System.loadLibrary("bare-kit");
  }

  @FunctionalInterface
  public interface PushCallback<T> {
    void
    apply (T reply, Throwable exception);
  }

  @FunctionalInterface
  private interface NativePushCallback {
    void
    apply (ByteBuffer reply, String exception);
  }

  private ByteBuffer handle;
  private FileDescriptor incoming;
  private FileDescriptor outgoing;
  private Handler handler;

  public Worklet() {
    handle = init();

    handler = Handler.createAsync(Looper.getMainLooper());
  }

  private native ByteBuffer
  init ();

  private native void
  start (ByteBuffer handle, String filename, ByteBuffer source, int len);

  private native void
  suspend (ByteBuffer handle, int linger);

  private native void
  resume (ByteBuffer handle);

  private native void
  terminate (ByteBuffer handle);

  private native FileDescriptor
  incoming (ByteBuffer handle);

  private native FileDescriptor
  outgoing (ByteBuffer handle);

  private native void
  push (ByteBuffer handle, ByteBuffer payload, int len, NativePushCallback callback);

  private void
  start (String filename, ByteBuffer source, int len) {
    start(handle, filename, source, len);

    incoming = incoming(handle);
    outgoing = outgoing(handle);
  }

  public void
  start (String filename, ByteBuffer source) {
    ByteBuffer buffer;

    if (source.isDirect()) {
      buffer = source;
    } else {
      buffer = ByteBuffer.allocateDirect(source.limit());
      buffer.put(source);
      buffer.flip();
    }

    start(filename, buffer, buffer.limit());
  }

  public void
  start (String filename, InputStream source) throws IOException {
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

    start(filename, buffer, buffer.limit());
  }

  public void
  suspend () {
    suspend(handle, 0);
  }

  public void
  suspend (int linger) {
    suspend(handle, linger);
  }

  public void
  resume () {
    resume(handle);
  }

  public void
  terminate () {
    terminate(handle);

    handle = null;
  }

  public FileDescriptor
  incoming () {
    return incoming;
  }

  public FileDescriptor
  outgoing () {
    return outgoing;
  }

  private void
  push (ByteBuffer payload, int len, PushCallback<ByteBuffer> callback) {
    push(handle, payload, len, (reply, error) -> {
      ByteBuffer buffer = reply == null ? null : ByteBuffer.allocateDirect(reply.limit());

      if (buffer != null) {
        buffer.put(reply);
        buffer.flip();
      }

      Throwable exception = error == null ? null : new Error(error);

      handler.post(() -> callback.apply(buffer, exception));
    });
  }

  public void
  push (ByteBuffer payload, PushCallback<ByteBuffer> callback) {
    ByteBuffer buffer;

    if (payload.isDirect()) {
      buffer = payload;
    } else {
      buffer = ByteBuffer.allocateDirect(payload.limit());
      buffer.put(payload);
      buffer.flip();
    }

    push(buffer, buffer.limit(), callback);
  }

  public void
  push (String payload, Charset charset, PushCallback<String> callback) {
    push(ByteBuffer.wrap(payload.getBytes(charset)), (reply, error) -> {
      callback.apply(reply == null ? null : charset.decode(reply).toString(), error);
    });
  }

  public void
  push (String payload, String charset, PushCallback<String> callback) {
    push(payload, Charset.forName(charset), callback);
  }

  public void
  close () {
    terminate();
  }
}
