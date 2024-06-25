package to.holepunch.bare.kit;

import java.nio.ByteBuffer;

public class Worklet {
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

  public void start (String filename, ByteBuffer source) {
    start(handle, filename, source);
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
}
