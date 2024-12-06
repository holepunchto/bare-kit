package to.holepunch.bare.kit;

import java.io.Closeable;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import to.holepunch.bare.kit.Worklet;

public class IPC implements Closeable {
  private static int READABLE = 1;
  private static int WRITABLE = 2;

  @FunctionalInterface
  public interface PollCallback {
    void
    apply();
  }

  private ByteBuffer handle;
  private PollCallback readable;
  private PollCallback writable;

  public IPC(String endpoint) {
    handle = init(endpoint);
  }

  public IPC(Worklet worklet) {
    this(worklet.endpoint());
  }

  private native ByteBuffer
  init(String endpoint);

  private native void
  destroy(ByteBuffer handle);

  private native ByteBuffer
  message();

  private native ByteBuffer
  read(ByteBuffer handle, ByteBuffer message);

  private native boolean
  write(ByteBuffer handle, ByteBuffer message, ByteBuffer data, int len);

  private native void
  release(ByteBuffer message);

  private native void
  poll(ByteBuffer handle, int events);

  private boolean
  poll(int events) {
    if ((events & IPC.READABLE) != 0) readable.apply();
    if ((events & IPC.WRITABLE) != 0) writable.apply();

    return readable != null || writable != null;
  }

  private void
  poll() {
    int events = 0;

    if (readable != null) events |= IPC.READABLE;
    if (writable != null) events |= IPC.WRITABLE;

    poll(handle, events);
  }

  public void
  readable(PollCallback callback) {
    readable = callback;

    poll();
  }

  public void
  writable(PollCallback callback) {
    writable = callback;

    poll();
  }

  public ByteBuffer
  read() {
    ByteBuffer message = message();
    ByteBuffer buffer = read(handle, message);

    if (buffer == null) {
      release(message);

      return null;
    }

    ByteBuffer copy = ByteBuffer.allocateDirect(buffer.limit());
    copy.put(buffer);
    copy.flip();

    release(message);

    return copy;
  }

  public String
  read(Charset charset) {
    ByteBuffer message = message();
    ByteBuffer buffer = read(handle, message);

    if (buffer == null) {
      release(message);

      return null;
    }

    String result = charset.decode(buffer).toString();

    release(message);

    return result;
  }

  public String
  read(String charset) {
    return read(Charset.forName(charset));
  }

  public boolean
  write(ByteBuffer data) {
    ByteBuffer message = message();
    ByteBuffer buffer;

    if (data.isDirect()) {
      buffer = data;
    } else {
      buffer = ByteBuffer.allocateDirect(data.limit());
      buffer.put(data);
      buffer.flip();
    }

    boolean sent = write(handle, message, buffer, buffer.limit());

    release(message);

    return sent;
  }

  public boolean
  write(String data, Charset charset) {
    return write(ByteBuffer.wrap(data.getBytes(charset)));
  }

  public boolean
  write(String data, String charset) {
    return write(data, Charset.forName(charset));
  }

  public void
  close() {
    destroy(handle);
  }
}
