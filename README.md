# bare-kit

Bare for native application development. The kit provides a web worker-like API to start and manage an isolated Bare thread, called a worklet[^1], that exposes an RPC abstraction with bindings for various languages.

[^1]: This term was chosen to avoid ambiguity with worker threads as implemented by <https://github.com/holepunchto/bare-worker>.

## Usage

### iOS

```objc
#import <BareKit/BareKit.h>
```

```objc
BareWorklet *worklet = [[BareWorklet alloc] init];
```

```objc
[worklet start:@"/app.bundle" source:/* Source for `app.bundle` */];
```

```objc
BareIPC *ipc = [[BareIPC alloc] initWithWorklet:worklet];
```

```objc
BareRPCRequestHandler requestHandler = ^(BareRPCIncomingRequest *req, NSError *error) {
  if ([req.command isEqualToString:@"ping"]) {
    CFShow([req dataWithEncoding:NSUTF8StringEncoding]);

    [req reply:@"pong" encoding:NSUTF8StringEncoding];
  }
};
```

```objc
BareRPC *rpc = [[BareRPC alloc] initWithIPC:ipc requestHandler:requestHandler];
```

```objc
BareRPCOutgoingRequest *req = [rpc request:@"ping"];
```

```objc
[req send:@"ping" encoding:NSUTF8StringEncoding];
```

```objc
[req reply:NSUTF8StringEncoding completion:^(NSString *data, NSError *error) {
  CFShow(data);
}];
```

See <https://github.com/holepunchto/bare-ios> for a complete example of using the kit in an iOS application.

### Android

```java
import to.holepunch.bare.kit.IPC;
import to.holepunch.bare.kit.RPC;
import to.holepunch.bare.kit.Worklet;
```

```java
Worklet worklet = new Worklet();
```

```java
try {
  worklet.start("/app.bundle", getAssets().open("app.bundle"));
} catch (Exception e) {
  throw new RuntimeException(e);
}
```

```java
IPC ipc = new IPC(worklet);
```

```java
RPC rpc = new RPC(ipc, (req, error) -> {
  if (req.command.equals("ping")) {
    Log.i(TAG, req.data("UTF-8"));

    req.reply("pong", "UTF-8");
  }
});
```

```java
RPC.OutgoingRequest req = rpc.request("ping");
```

```java
req.send("ping", "UTF-8");
```

```java
req.reply("UTF-8", (data, error) -> {
  Log.i(TAG, data);
});
```

See <https://github.com/holepunchto/bare-android> for a complete example of using the kit in an Android application.

## License

Apache-2.0
