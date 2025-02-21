package to.holepunch.bare.kit;

import java.io.Closeable;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import to.holepunch.bare.kit.Worklet;

public class IPC implements Closeable {
  @FunctionalInterface
  public interface PollCallback {
    void
    apply();
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

  private native boolean
  write(ByteBuffer handle, ByteBuffer data, int len);

  private native void
  readable(ByteBuffer handle, boolean reset);

  private native void
  writable(ByteBuffer handle, boolean reset);

  public boolean
  readable() {
    if (readable != null) readable.apply();

    return writable != null;
  }

  public void
  readable(PollCallback callback) {
    readable = callback;

    readable(handle, readable == null);
  }

  public boolean
  writable() {
    if (writable != null) writable.apply();

    return writable != null;
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

  public String
  read(Charset charset) {
    ByteBuffer buffer = read(handle);

    if (buffer == null) {
      return null;
    }

    String result = charset.decode(buffer).toString();

    return result;
  }

  public String
  read(String charset) {
    return read(Charset.forName(charset));
  }

  public boolean
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
