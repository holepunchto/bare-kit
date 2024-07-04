package to.holepunch.compact;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;

public class Encoder {
  public ByteBuffer buffer;

  public Encoder() {
    buffer = ByteBuffer.allocateDirect(4096);
    buffer.order(ByteOrder.LITTLE_ENDIAN);
  }

  private void
  grow (int capacity) {
    if (buffer.remaining() >= capacity) return;

    buffer.flip();

    ByteBuffer copy = ByteBuffer.allocateDirect(buffer.capacity() * 2);
    copy.order(ByteOrder.LITTLE_ENDIAN);
    copy.put(buffer);

    buffer = copy;
  }

  public ByteBuffer
  finish () {
    buffer.flip();
    return buffer;
  }

  public void
  putBool (boolean value) {
    grow(1);
    buffer.put((byte) (value ? 1 : 0));
  }

  public void
  putUint8 (short n) {
    grow(1);
    buffer.put((byte) n);
  }

  public void
  putUint16 (int n) {
    grow(2);
    buffer.putShort((short) n);
  }

  public void
  putUint32 (long n) {
    grow(4);
    buffer.putInt((int) n);
  }

  public void
  putUint64 (long n) {
    grow(8);
    buffer.putLong(n);
  }

  public void
  putUint (long n) {
    if (n <= 0xfc) {
      grow(1);
      putUint8((short) (n & 0xff));
    } else if (n <= 0xffff) {
      grow(3);
      putUint8((short) 0xfd);
      putUint16((int) (n & 0xffff));
    } else if (n <= 0xffffffff) {
      grow(5);
      putUint8((short) 0xfe);
      putUint32(n & 0xffffffff);
    } else {
      grow(9);
      putUint8((short) 0xff);
      putUint64(n);
    }
  }

  public void
  putInt8 (byte n) {
    putUint8(zigZag(n));
  }

  public void
  putInt16 (short n) {
    putUint16(zigZag(n));
  }

  public void
  putInt32 (int n) {
    putUint32(zigZag(n));
  }

  public void
  putInt64 (long n) {
    putUint64(zigZag(n));
  }

  public void
  putInt (long n) {
    putUint(zigZag(n));
  }

  private short
  zigZag (byte n) {
    return (short) ((n << 1) ^ -n);
  }

  private int
  zigZag (short n) {
    return (int) ((n << 1) ^ -(n >> 1));
  }

  private long
  zigZag (int n) {
    return (n << 1) ^ -(n >> 3);
  }

  private long
  zigZag (long n) {
    return (n << 1) ^ -(n >> 7);
  }

  public void
  putUint8Array (byte[] data) {
    putUint(data.length);
    grow(data.length);
    buffer.put(data);
  }

  public void
  putBuffer (ByteBuffer data) {
    putUint(data.remaining());
    grow(data.remaining());
    buffer.put(data);
  }

  public void
  putString (String string, Charset charset) {
    putUint8Array(string.getBytes(charset));
  }

  public void
  putString (String string, String charset) {
    putString(string, Charset.forName(charset));
  }

  public void
  putUTF8 (String string) {
    putString(string, StandardCharsets.UTF_8);
  }
}
