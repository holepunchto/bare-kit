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
  getUint64 () {
    return buffer.getLong();
  }

  public long
  getUint () {
    int len = getUint8();
    if (len <= 0xfc) return len;
    if (len == 0xfd) return getUint16();
    if (len == 0xfe) return getUint32();
    return getUint64();
  }

  public byte
  getInt8 () {
    return (byte) zigZag(getUint8());
  }

  public short
  getInt16 () {
    return (short) zigZag(getUint16());
  }

  public int
  getInt32 () {
    return (int) zigZag(getUint32());
  }

  public long
  getInt64 () {
    return zigZag(getUint64());
  }

  public long
  getInt () {
    return zigZag(getUint());
  }

  private long
  zigZag (long n) {
    return (n >> 1) ^ -(n & 1);
  }

  public byte[]
  getUint8Array () {
    int len = (int) getUint();

    byte[] data = new byte[len];
    buffer.get(data);

    return data;
  }

  public ByteBuffer
  getBuffer () {
    int len = (int) getUint();

    ByteBuffer copy = buffer.slice();
    copy.limit(len);

    ByteBuffer data = ByteBuffer.allocateDirect(len);
    data.put(copy);
    data.flip();

    buffer.position(buffer.position() + len);

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
