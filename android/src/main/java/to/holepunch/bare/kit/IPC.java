package to.holepunch.bare.kit;

import java.io.Closeable;
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

  private IPC(Integer incoming, Integer outgoing) {
    handle = init(incoming, outgoing);
  }

  public IPC(Worklet worklet) {
    this(worklet.incoming, worklet.outgoing);
  }

  private native ByteBuffer
  init(Integer incoming, Integer outgoing);

  private native void
  destroy(ByteBuffer handle);

  private native ByteBuffer
  message();

  private native ByteBuffer
  read(ByteBuffer handle);

  private native boolean
  write(ByteBuffer handle, ByteBuffer data, int len);

  private native void
  setReadableHandler(ByteBuffer handle);

  private native void
  resetReadableHandler(ByteBuffer handle);

  private native void
  setWritableHandler(ByteBuffer handle);

  private native void
  resetWritableHandler(ByteBuffer handle);

  public boolean
  callReadable() {
    if (readable != null) {
      readable.apply();
    }

    return readable != null;
  }

  public boolean
  callWritable() {
    if (writable != null) {
      writable.apply();
    }

    return writable != null;
  }

  public void
  readable(PollCallback callback) {
    readable = callback;

    if (readable != null) {
      setReadableHandler(handle);
    } else {
      resetReadableHandler(handle);
    }
  }

  public void
  writable(PollCallback callback) {
    writable = callback;

    if (writable != null) {
      setWritableHandler(handle);
    } else {
      resetWritableHandler(handle);
    }
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

    boolean sent = write(handle, buffer, buffer.limit());

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
