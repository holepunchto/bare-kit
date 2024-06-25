package to.holepunch.bare.kit;

import java.io.Closeable;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;

public class Worklet implements Closeable {
  static {
    System.loadLibrary("bare_kit");
  }

  private ByteBuffer handle;

  public Worklet() {
    handle = init();
  }

  private native ByteBuffer init ();
  private native void start (ByteBuffer handle, String filename, ByteBuffer source);
  private native void suspend (ByteBuffer handle, int linger);
  private native void resume (ByteBuffer handle);
  private native void terminate (ByteBuffer handle);
  private native void close (ByteBuffer handle);

  public void start (String filename, ByteBuffer source) {
    start(handle, filename, source);
  }

  public void start (String filename, InputStream source) throws IOException {
    start(handle, filename, ByteBuffer.wrap(source.readAllBytes()));
  }

  public void suspend () {
    suspend(handle, 0);
  }

  public void suspend (int linger) {
    suspend(handle, linger);
  }

  public void resume () {
    resume(handle);
  }

  public void terminate () {
    terminate(handle);
  }

  public void close () {
    close(handle);

    handle = null;
  }
}
