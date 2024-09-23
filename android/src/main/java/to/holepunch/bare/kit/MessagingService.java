package to.holepunch.bare.kit;

import com.google.firebase.messaging.FirebaseMessagingService;
import com.google.firebase.messaging.RemoteMessage;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import org.json.JSONException;
import org.json.JSONObject;

public abstract class MessagingService extends FirebaseMessagingService {
  @FunctionalInterface
  public interface ReplyCallback {
    void
    apply (JSONObject reply);
  }

  protected Worklet worklet;
  protected ReplyCallback callback;

  private MessagingService(ReplyCallback callback) {
    super();

    this.worklet = new Worklet();
    this.callback = callback;
  }

  protected MessagingService(String filename, ByteBuffer source) {
    this(null);
    this.worklet.start(filename, source);
  }

  protected MessagingService(String filename, ByteBuffer source, ReplyCallback callback) {
    this(callback);
    this.worklet.start(filename, source);
  }

  protected MessagingService(String filename, String source, Charset charset) {
    this(null);
    this.worklet.start(filename, source, charset);
  }

  protected MessagingService(String filename, String source, Charset charset, ReplyCallback callback) {
    this(callback);
    this.worklet.start(filename, source, charset);
  }

  protected MessagingService(String filename, String source, String charset) {
    this(null);
    this.worklet.start(filename, source, charset);
  }

  protected MessagingService(String filename, String source, String charset, ReplyCallback callback) {
    this(callback);
    this.worklet.start(filename, source, charset);
  }

  protected MessagingService(String filename, InputStream source) throws IOException {
    this(null);
    this.worklet.start(filename, source);
  }

  protected MessagingService(String filename, InputStream source, ReplyCallback callback) throws IOException {
    this(callback);
    this.worklet.start(filename, source);
  }

  @Override
  public void
  onDestroy () {
    super.onDestroy();

    worklet.terminate();
  }

  @Override
  public void
  onMessageReceived (RemoteMessage message) {
    JSONObject json = new JSONObject();

    try {
      json.put("data", new JSONObject(message.getData()));
    } catch (JSONException err) {
      return;
    }

    worklet.push(json.toString(), StandardCharsets.UTF_8, (reply, exception) -> {
      if (reply == null) return;

      JSONObject data;
      try {
        data = new JSONObject(reply);
      } catch (JSONException err) {
        return;
      }

      onWorkletReply(data);
    });
  }

  @Override
  public void
  onNewToken (String token) {}

  public void
  onWorkletReply (JSONObject reply) {
    if (callback != null) callback.apply(reply);
  }
}
