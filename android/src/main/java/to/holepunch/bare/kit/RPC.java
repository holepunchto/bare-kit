package to.holepunch.bare.kit;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.concurrent.CompletableFuture;
import to.holepunch.bare.kit.IPC;
import to.holepunch.compact.Decoder;
import to.holepunch.compact.Encoder;

public class RPC {
  public class IncomingRequest {
    private RPC rpc;

    public long id;
    public String command;
    public ByteBuffer data;

    IncomingRequest(RPC rpc, long id, String command, ByteBuffer data) {
      this.rpc = rpc;
      this.id = id;
      this.command = command;
      this.data = data;
    }

    IncomingRequest(RPC rpc, RequestMessage message) {
      this(rpc, message.id, message.command, message.data);
    }

    public String
    data (Charset charset) {
      return charset.decode(data).toString();
    }

    public String
    data (String charset) {
      return data(Charset.forName(charset));
    }

    public void
    reply (ByteBuffer data) {
      rpc.reply(this, data);
    }

    public void
    reply (String data, Charset charset) {
      reply(ByteBuffer.wrap(data.getBytes(charset)));
    }

    public void
    reply (String data, String charset) {
      reply(data, Charset.forName(charset));
    }
  }

  public class OutgoingRequest {
    private RPC rpc;
    private CompletableFuture<ByteBuffer> response;

    public String command;

    OutgoingRequest(RPC rpc, String command) {
      this.rpc = rpc;
      this.response = new CompletableFuture<ByteBuffer>();
      this.command = command;
    }

    public void
    send (ByteBuffer data) {
      rpc.send(this, data);
    }

    public void
    send (String data, Charset charset) {
      send(ByteBuffer.wrap(data.getBytes(charset)));
    }

    public void
    send (String data, String charset) {
      send(data, Charset.forName(charset));
    }

    public void
    reply (ResponseCallback<ByteBuffer> callback) {
      this.response.whenComplete((data, error) -> callback.apply(data, error));
    }

    public void
    reply (Charset charset, ResponseCallback<String> callback) {
      this.response.whenComplete((data, error) -> callback.apply(charset.decode(data).toString(), error));
    }

    public void
    reply (String charset, ResponseCallback<String> callback) {
      reply(Charset.forName(charset), callback);
    }
  }

  abstract static class Message {
    long type;
    long id;

    Message(long type, long id) {
      this.type = type;
      this.id = id;
    }

    void
    encode (Encoder encoder) {
      int frame = encoder.buffer.position();

      encoder.putUint32(0); // Frame

      int start = encoder.buffer.position();

      encoder.putUint(type);
      encoder.putUint(id);

      subencode(encoder);

      int end = encoder.buffer.position();

      encoder.buffer.position(frame);

      encoder.putUint32(end - start); // Frame

      encoder.buffer.position(end);
    }

    protected abstract void
    subencode (Encoder encoder);

    static Message
    decode (Decoder decoder) throws IOException {
      int start = decoder.buffer.position();

      int frame = decoder.getUint32();

      if (decoder.buffer.remaining() < frame) {
        decoder.buffer.position(start);

        return null;
      }

      long type = decoder.getUint();
      long id = decoder.getUint();

      if (type == 1) {
        return RequestMessage.decode(decoder, id);
      }

      if (type == 2) {
        return ResponseMessage.decode(decoder, id);
      }

      throw new IOException("Unknown message '" + type + "'");
    }
  }

  final static class RequestMessage extends Message {
    String command;
    ByteBuffer data;

    RequestMessage(long id, String command, ByteBuffer data) {
      super(1, id);

      this.command = command;
      this.data = data;
    }

    protected void
    subencode (Encoder encoder) {
      encoder.putUTF8(command);
      encoder.putBuffer(data);
      encoder.putUint(0); // Reserved
    }

    static RequestMessage
    decode (Decoder decoder, long id) {
      String command = decoder.getUTF8();
      ByteBuffer data = decoder.getBuffer();
      decoder.getUint(); // Reserved

      return new RequestMessage(id, command, data);
    }
  }

  final static class ResponseMessage extends Message {
    ByteBuffer data;
    Error error;

    ResponseMessage(long id, ByteBuffer data, Error error) {
      super(2, id);

      this.data = data;
      this.error = error;
    }

    protected void
    subencode (Encoder encoder) {
      boolean hasError = error != null;

      encoder.putBool(hasError);

      if (hasError) {
        encoder.putUTF8(error.getMessage());
        encoder.putUTF8(""); // Code
        encoder.putInt(0);   // Status
      } else {
        encoder.putBuffer(data);
      }

      encoder.putUint(0); // Reserved
    }

    static ResponseMessage
    decode (Decoder decoder, long id) {
      boolean hasError = decoder.getBool();

      ByteBuffer data = null;
      Error error = null;

      if (hasError) {
        String message = decoder.getUTF8();
        decoder.getUTF8(); // Code
        decoder.getInt();  // Status

        error = new Error(message);
      } else {
        data = decoder.getBuffer();
      }

      decoder.getUint(); // Reserved

      return new ResponseMessage(id, data, error);
    }
  }

  @FunctionalInterface
  public interface RequestHandler {
    void
    apply (IncomingRequest request, Throwable exception);
  }

  @FunctionalInterface
  public interface ResponseCallback<T> {
    void
    apply (T result, Throwable exception);
  }

  private IPC ipc;
  private RequestHandler requestHandler;
  private long id;
  private HashMap<Long, OutgoingRequest> requests;
  private ByteBuffer buffer;

  public RPC(IPC ipc, RequestHandler requestHandler) {
    this.ipc = ipc;
    this.requestHandler = requestHandler;
    this.id = 0;
    this.requests = new HashMap<Long, OutgoingRequest>();
    this.buffer = null;

    read();
  }

  private void
  read () {
    ipc.read((data, error) -> {
      if (error != null) {
        requestHandler.apply(null, error);
        return;
      }

      if (data == null) return;

      if (buffer == null) buffer = data;
      else {
        ByteBuffer copy = ByteBuffer.allocateDirect(buffer.remaining() + data.remaining());
        copy.put(buffer);
        copy.put(data);
        copy.flip();

        buffer = copy;
      }

      Decoder decoder = new Decoder(buffer);

      while (buffer != null) {
        Message message;

        try {
          message = Message.decode(decoder);
        } catch (IOException e) {
          throw new RuntimeException(e);
        }

        if (message == null) break;

        if (message instanceof RequestMessage) {
          requestHandler.apply(new IncomingRequest(this, (RequestMessage) message), null);
        } else if (message instanceof ResponseMessage) {
          OutgoingRequest request = requests.remove(message.id);

          if (request != null) {
            ResponseMessage response = (ResponseMessage) message;

            if (response.error != null) {
              request.response.completeExceptionally(response.error);
            } else {
              request.response.complete(response.data);
            }
          }
        }

        if (buffer.remaining() == 0) buffer = null;
      }

      read();
    });
  }

  public OutgoingRequest
  request (String command) {
    return new OutgoingRequest(this, command);
  }

  private void
  send (OutgoingRequest request, ByteBuffer data) {
    long id = ++this.id;

    requests.put(id, request);

    RequestMessage message = new RequestMessage(id, request.command, data);

    Encoder encoder = new Encoder();

    message.encode(encoder);

    ipc.write(encoder.finish());
  }

  private void
  reply (IncomingRequest request, ByteBuffer data) {
    ResponseMessage message = new ResponseMessage(request.id, data, null);

    Encoder encoder = new Encoder();

    message.encode(encoder);

    ipc.write(encoder.finish());
  }
}
