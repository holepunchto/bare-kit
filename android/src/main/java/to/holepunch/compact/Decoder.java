package to.holepunch.compact;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;

public class Decoder {
  public ByteBuffer buffer;

  public Decoder(ByteBuffer buffer) {
    this.buffer = buffer;
    this.buffer.order(ByteOrder.LITTLE_ENDIAN);
  }

  public boolean
  getBool () {
    return buffer.get() != 0;
  }

  public short
  getUint8 () {
    return (short) (buffer.get() & 0xff);
  }

  public int
  getUint16 () {
    return buffer.getShort() & 0xffff;
  }

  public int
  getUint32 () {
    return buffer.getInt() & 0xffffffff;
  }

  public long
  getUint () {
    int len = getUint8();
    if (len <= 0xfc) return len;
    if (len == 0xfd) return getUint16();
    if (len == 0xfe) return getUint32();
    throw new RuntimeException("Unsigned integer overflow");
  }

  public byte
  getInt8 () {
    return zigZag(getUint8());
  }

  public short
  getInt16 () {
    return buffer.getShort();
  }

  public int
  getInt32 () {
    return buffer.getInt();
  }

  public long
  getInt () {
    return zigZag(getUint());
  }

  private byte
  zigZag (short n) {
    return (byte) ((n >> 1) ^ -(n & 1));
  }

  private short
  zigZag (int n) {
    return (short) ((n >> 1) ^ -(n & 1));
  }

  private int
  zigZag (long n) {
    return (int) ((n >> 1) ^ -(n & 1));
  }

  public byte[]
  getUint8Array () {
    long len = getUint();

    byte[] data = new byte[(int) len];
    buffer.get(data);

    return data;
  }

  public ByteBuffer
  getBuffer () {
    long len = getUint();

    ByteBuffer copy = buffer.slice();
    copy.limit((int) len);

    ByteBuffer data = ByteBuffer.allocateDirect((int) len);
    data.put(copy);
    data.flip();

    buffer.position(buffer.position() + (int) len);

    return data;
  }

  public String
  getString (Charset charset) {
    return charset.decode(getBuffer()).toString();
  }

  public String
  getString (String charset) {
    return getString(Charset.forName(charset));
  }

  public String
  getUTF8 () {
    return getString(StandardCharsets.UTF_8);
  }
}
