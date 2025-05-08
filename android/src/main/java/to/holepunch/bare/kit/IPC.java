package to.holepunch.bare.kit;

import java.io.Closeable;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import to.holepunch.bare.kit.Worklet;

public class IPC implements Closeable {
  @FunctionalInterface
  public interface PollCallback {
    void
    apply();
  }

  @FunctionalInterface
  public interface ReadCallback {
    void
    apply(ByteBuffer data, Throwable exception);
  }

  @FunctionalInterface
  public interface WriteCallback {
    void
    apply(Throwable exception);
  }

  private ByteBuffer handle;
  private PollCallback readable;
  private PollCallback writable;

  private IPC(int incoming, int outgoing) {
    handle = init(incoming, outgoing);
  }

  public IPC(Worklet worklet) {
    this(worklet.incoming, worklet.outgoing);
  }

  private native ByteBuffer
  init(int incoming, int outgoing);

  private native void
  destroy(ByteBuffer handle);

  private native ByteBuffer
  message();

  private native ByteBuffer
  read(ByteBuffer handle);

  private native int
  write(ByteBuffer handle, ByteBuffer data, int len);

  private native void
  readable(ByteBuffer handle, boolean reset);

  private native void
  writable(ByteBuffer handle, boolean reset);

  private boolean
  readable() {
    if (readable != null) readable.apply();

    return readable != null;
  }

  private boolean
  writable() {
    if (writable != null) writable.apply();

    return writable != null;
  }

  public void
  readable(PollCallback callback) {
    readable = callback;

    readable(handle, readable == null);
  }

  public void
  writable(PollCallback callback) {
    writable = callback;

    writable(handle, writable == null);
  }

  public ByteBuffer
  read() {
    ByteBuffer buffer = read(handle);

    if (buffer == null) {
      return null;
    }

    ByteBuffer copy = ByteBuffer.allocateDirect(buffer.limit());
    copy.put(buffer);
    copy.flip();

    return copy;
  }

  public void
  read(ReadCallback callback) {
    final AtomicReference<ByteBuffer> data = new AtomicReference<>(read());

    if (data.get() != null) {
      callback.apply(data.get(), null);
    } else {
      readable(() -> {
        data.set(read());

        if (data.get() != null) {
          readable(null);

          callback.apply(data.get(), null);
        }
      });
    }
  }

  public int
  write(ByteBuffer data) {
    ByteBuffer buffer;

    if (data.isDirect()) {
      buffer = data;
    } else {
      buffer = ByteBuffer.allocateDirect(data.limit());
      buffer.put(data);
      buffer.flip();
    }

    return write(handle, buffer, buffer.limit());
  }

  public void
  write(ByteBuffer data, WriteCallback callback) {
    ByteBuffer buffer;

    if (data.isDirect()) {
      buffer = data.slice();
    } else {
      buffer = ByteBuffer.allocateDirect(data.limit());
      buffer.put(data);
      buffer.flip();
    }

    int written1 = write(buffer);

    if (written1 == buffer.limit()) {
      callback.apply(null);
    } else {
      buffer.position(written1);

      final AtomicReference<ByteBuffer> remaining = new AtomicReference<>(buffer.slice());

      writable(() -> {
        ByteBuffer slice = remaining.get();

        int written2 = write(slice);

        if (written2 == slice.limit()) {
          writable(null);

          callback.apply(null);
        } else {
          slice.position(written2);

          remaining.set(slice.slice());
        }
      });
    }
  }

  public void
  close() {
    destroy(handle);
  }
}
