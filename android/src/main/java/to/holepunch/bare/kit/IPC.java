package to.holepunch.bare.kit;

import android.os.Handler;
import android.os.Looper;
import java.io.Closeable;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
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

  ByteBuffer handle;

  // Held to keep the worklet reachable, as an IPC is meaningless without it.
  private final Worklet worklet;

  private volatile PollCallback readable;
  private volatile PollCallback writable;

  private final ByteBuffer buffer = ByteBuffer.allocateDirect(64 * 1024);

  // The looper of the opening thread, on which the callbacks run and the poll is
  // changed.
  private final Handler handler;

  /**
   * Must be called on a thread with a looper, on which the callbacks then run.
   * Throws if the worklet has terminated or already has an open IPC.
   */
  public IPC(Worklet worklet) {
    Looper looper = Looper.myLooper();

    if (looper == null) throw new IllegalStateException("IPC must be opened on a looper thread");

    this.worklet = worklet;

    handler = new Handler(looper);

    synchronized (worklet) {
      if (worklet.handle == null) throw new IllegalStateException("Worklet terminated");

      handle = init(worklet.handle);
    }

    if (handle == null) throw new IllegalStateException("Could not open worklet IPC");
  }

  private native ByteBuffer
  init(ByteBuffer handle);

  private native void
  close(ByteBuffer handle);

  private native void
  destroy(ByteBuffer handle);

  private native ByteBuffer
  message();

  private native int
  read(ByteBuffer handle, ByteBuffer buffer);

  private native int
  write(ByteBuffer handle, ByteBuffer data, int len);

  private native void
  update(ByteBuffer handle, boolean readable, boolean writable);

  private void
  update() {
    if (handler.getLooper().isCurrentThread()) updateOnLooper();
    else handler.post(this::updateOnLooper);
  }

  private synchronized void
  updateOnLooper() {
    if (handle == null) return;

    update(handle, readable != null, writable != null);
  }

  // Called from native code. The callbacks are taken afresh for each event, as
  // the previous one may have changed them.
  private void
  poll(boolean readable, boolean writable) {
    if (readable) {
      PollCallback callback = this.readable;

      if (callback != null) callback.apply();
    }

    if (writable) {
      PollCallback callback = this.writable;

      if (callback != null) callback.apply();
    }
  }

  public void
  readable(PollCallback callback) {
    readable = callback;

    update();
  }

  public void
  writable(PollCallback callback) {
    writable = callback;

    update();
  }

  /**
   * Reads whatever is available without blocking. Returns null when nothing is
   * ready yet, and an empty buffer at end-of-stream, once the worklet has
   * closed its end or this IPC is closed. Wait for readable() before retrying
   * a null.
   */
  public synchronized ByteBuffer
  read() {
    if (handle == null) return ByteBuffer.allocateDirect(0);

    int len = read(handle, buffer);

    if (len < 0) return null;

    buffer.limit(len);

    ByteBuffer data = ByteBuffer.allocateDirect(len);
    data.put(buffer);
    data.flip();

    buffer.clear();

    return data;
  }

  /**
   * Reads once something is available, waiting for readable() as needed.
   * Delivers null data at end-of-stream, once the worklet has closed its end or
   * this IPC is closed.
   */
  public void
  read(ReadCallback callback) {
    ByteBuffer data1 = read();

    // A zero-length read is end-of-stream; signal it as null rather than an
    // empty buffer so consumers can detect a closed IPC.
    if (data1 != null) {
      callback.apply(data1.limit() == 0 ? null : data1, null);
    } else {
      readable(() -> {
        ByteBuffer data2 = read();

        if (data2 != null) {
          readable(null);

          callback.apply(data2.limit() == 0 ? null : data2, null);
        }
      });
    }
  }

  /**
   * Writes whatever fits without blocking, returning the number of bytes taken,
   * or -1 once this IPC is closed or the worklet has exited.
   */
  public synchronized int
  write(ByteBuffer data) {
    if (handle == null) return -1;

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

    if (written1 < 0) {
      callback.apply(new IOException("IPC closed"));
    } else if (written1 == buffer.limit()) {
      callback.apply(null);
    } else {
      buffer.position(written1);

      final AtomicReference<ByteBuffer> remaining = new AtomicReference<>(buffer.slice());

      writable(() -> {
        ByteBuffer slice = remaining.get();

        int written2 = write(slice);

        if (written2 < 0) {
          writable(null);

          callback.apply(new IOException("IPC closed"));
        } else if (written2 == slice.limit()) {
          writable(null);

          callback.apply(null);
        } else {
          slice.position(written2);

          remaining.set(slice.slice());
        }
      });
    }
  }

  /**
   * Closing twice is a no-op.
   */
  public void
  close() {
    ByteBuffer handle;

    synchronized (this) {
      handle = this.handle;

      if (handle == null) return;

      this.handle = null;

      close(handle);
    }

    // The poll belongs to the looper, so it is torn down there.
    if (handler.getLooper().isCurrentThread()) destroy(handle);
    else handler.post(() -> destroy(handle));
  }
}
