package to.holepunch.bare.kit;

import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import to.holepunch.bare.kit.IPC;

public class RPC {
  public class IncomingRequest {
    private RPC rpc;

    public int id;
    public String command;
    public ByteBuffer data;

    IncomingRequest(RPC rpc, int id, String command, ByteBuffer data) {
      this.rpc = rpc;
      this.id = id;
      this.command = command;
      this.data = data;
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

    public String command;

    OutgoingRequest(RPC rpc, String command) {
      this.rpc = rpc;
      this.command = command;
    }

    public void
    send (ByteBuffer data) {
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
    }

    public void
    reply (Charset charset, ResponseCallback<String> callback) {
    }

    public void
    reply (String charset, ResponseCallback<String> callback) {
      reply(Charset.forName(charset), callback);
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
  private ByteBuffer buffer;

  public RPC(IPC ipc, RequestHandler requestHandler) {
    this.ipc = ipc;
    this.requestHandler = requestHandler;
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
        ByteBuffer copy = ByteBuffer.allocateDirect(buffer.limit() + data.limit());
        copy.put(buffer);
        copy.put(data);
        buffer = copy;
      }
    });
  }

  public OutgoingRequest
  request (String command) {
    return new OutgoingRequest(this, command);
  }
}
