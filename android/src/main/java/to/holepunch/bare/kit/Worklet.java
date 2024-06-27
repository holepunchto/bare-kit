package to.holepunch.bare.kit;

import java.io.Closeable;
import java.io.FileDescriptor;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.channels.Channels;
import java.nio.channels.ReadableByteChannel;

public class Worklet implements Closeable {
  private ByteBuffer handle;
  private FileDescriptor incoming;
  private FileDescriptor outgoing;

  public Worklet() {
    handle = init();
  }

  private native ByteBuffer init ();
  private native void start (ByteBuffer handle, String filename, ByteBuffer source, int len);
  private native void suspend (ByteBuffer handle, int linger);
  private native void resume (ByteBuffer handle);
  private native void terminate (ByteBuffer handle);
  private native FileDescriptor incoming (ByteBuffer handle);
  private native FileDescriptor outgoing (ByteBuffer handle);

  public void start (String filename, ByteBuffer source) {
    ByteBuffer buffer = ByteBuffer.allocateDirect(source.limit());

    buffer.put(source);
    buffer.flip();

    start(handle, filename, buffer, buffer.limit());

    incoming = incoming(handle);
    outgoing = outgoing(handle);
  }

  public void start (String filename, InputStream source) throws IOException {
    source.reset();

    ByteBuffer buffer = ByteBuffer.allocateDirect(Math.max(4096, source.available()));

    ReadableByteChannel channel = Channels.newChannel(source);

    while (channel.read(buffer) != -1) {
      if (buffer.hasRemaining()) continue;

      ByteBuffer resized = ByteBuffer.allocateDirect(buffer.capacity() * 2);
      buffer.flip();
      resized.put(buffer);
      buffer = resized;
    }

    buffer.flip();
    channel.close();

    start(handle, filename, buffer, buffer.limit());
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

    handle = null;
  }

  public FileDescriptor incoming () {
    return incoming;
  }

  public FileDescriptor outgoing () {
    return outgoing;
  }

  public void close () {
    this.terminate();
  }
}
