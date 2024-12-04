package to.holepunch.bare.kit;

import java.io.Closeable;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import to.holepunch.bare.kit.Worklet;

public class IPC implements Closeable {
  @FunctionalInterface
  public interface ReadCallback<T> {
    void
    apply(T result, Throwable exception);
  }

  @FunctionalInterface
  public interface WriteCallback {
    void
    apply(Throwable exception);
  }

  private ByteBuffer handle;

  public IPC(String endpoint) {
    handle = init(endpoint);
  }

  public IPC(Worklet worklet) {
    this(worklet.endpoint());
  }

  private native ByteBuffer
  init(String endpoint);

  private native void
  close(ByteBuffer handle);

  public void
  read(ReadCallback<ByteBuffer> callback) {
  }

  public void
  read(Charset charset, ReadCallback<String> callback) {
    read((data, error) -> {
      callback.apply(data == null ? null : charset.decode(data).toString(), error);
    });
  }

  public void
  read(String charset, ReadCallback<String> callback) {
    read(Charset.forName(charset), callback);
  }

  public void
  write(ByteBuffer data, WriteCallback callback) {
  }

  public void
  write(String data, Charset charset, WriteCallback callback) {
    write(ByteBuffer.wrap(data.getBytes(charset)), callback);
  }

  public void
  write(String data, String charset, WriteCallback callback) {
    write(data, Charset.forName(charset), callback);
  }

  public void
  close() {
    close(handle);
  }
}
