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

  protected MessagingService(Worklet.Options options, ReplyCallback callback) {
    super();

    this.worklet = new Worklet(options);
    this.callback = callback;
  }

  protected MessagingService(String filename, ByteBuffer source, String[] arguments, Worklet.Options options, ReplyCallback callback) {
    this(options, callback);
    start(filename, source, arguments);
  }

  protected MessagingService(String filename, String source, Charset charset, String[] arguments, Worklet.Options options, ReplyCallback callback) {
    this(options, callback);
    start(filename, source, charset, arguments);
  }

  protected MessagingService(String filename, String source, String charset, String[] arguments, Worklet.Options options, ReplyCallback callback) {
    this(options, callback);
    start(filename, source, charset, arguments);
  }

  protected MessagingService(String filename, InputStream source, String[] arguments, Worklet.Options options, ReplyCallback callback) throws IOException {
    this(options, callback);
    start(filename, source, arguments);
  }

  protected MessagingService(Worklet.Options options) {
    this(options, null);
  }

  protected MessagingService(String filename, ByteBuffer source, String[] arguments, Worklet.Options options) {
    this(options);
    start(filename, source, arguments);
  }

  protected MessagingService(String filename, String source, Charset charset, String[] arguments, Worklet.Options options) {
    this(options);
    start(filename, source, charset, arguments);
  }

  protected MessagingService(String filename, String source, String charset, String[] arguments, Worklet.Options options) {
    this(options);
    start(filename, source, charset, arguments);
  }

  protected MessagingService(String filename, InputStream source, String[] arguments, Worklet.Options options) throws IOException {
    this(options);
    start(filename, source, arguments);
  }

  protected void
  start (String filename, ByteBuffer source, String[] arguments) {
    this.worklet.start(filename, source, arguments);
  }

  protected void
  start (String filename, String source, Charset charset, String[] arguments) {
    this.worklet.start(filename, source, charset, arguments);
  }

  protected void
  start (String filename, String source, String charset, String[] arguments) {
    this.worklet.start(filename, source, charset, arguments);
  }

  protected void
  start (String filename, InputStream source, String[] arguments) throws IOException {
    this.worklet.start(filename, source, arguments);
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
