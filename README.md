# bare-kit

Bare for native application development. The kit provides a web worker-like API to start and manage an isolated Bare thread, called a worklet[^1], that exposes an RPC abstraction with bindings for various languages.

[^1]: This term was chosen to avoid ambiguity with worker threads as implemented by <https://github.com/holepunchto/bare-worker>.

## Usage

### JavaScript

In this example we create a simple RPC instance using Bare Kit that sends a `ping` request, handles the request and outputs `pong` to the console.

Create a new RPC instance, providing a request handler

```js
const rpc = new BareKit.RPC((req) => {
  if (req.command === 'ping') {
    console.log(req.data.toString())

    req.reply('pong')
  }
})
```

Create an outgoing request with the command `ping`

```js
const req = rpc.request('ping')
```

Send the request with the data `ping`

```js
req.send('ping')
```

Store the received data and log it to console

```js
const data = await req.reply()
console.log(data.toString())
```

### iOS

In this example we create a simple RPC instance for iOS using Bare Kit and Objective-C that sends a `ping` request, handles the request and outputs `pong`.

Import the BareKit framework

```objc
#import <BareKit/BareKit.h>
```

Create a new Bare worklet instance

```objc
BareWorklet *worklet = [[BareWorklet alloc] init];
```

Start the worklet with the specified bundle source

```objc
[worklet start:@"/app.bundle" source:/* Source for `app.bundle` */];
```

Create an IPC instance associated with the worklet

```objc
BareIPC *ipc = [[BareIPC alloc] initWithWorklet:worklet];
```

Define a request handler block

```objc
BareRPCRequestHandler requestHandler = ^(BareRPCIncomingRequest *req, NSError *error) {
  if ([req.command isEqualToString:@"ping"]) {
    CFShow([req dataWithEncoding:NSUTF8StringEncoding]);

    [req reply:@"pong" encoding:NSUTF8StringEncoding];
  }
};
```

Create an RPC instance with the IPC and request handler

```objc
BareRPC *rpc = [[BareRPC alloc] initWithIPC:ipc requestHandler:requestHandler];
```

Create an outgoing request with the command `ping`

```objc
BareRPCOutgoingRequest *req = [rpc request:@"ping"];
```

Send the request with the data `ping`

```objc
[req send:@"ping" encoding:NSUTF8StringEncoding];
```

Get the reply data asynchronously and log the received data

```objc
[req reply:NSUTF8StringEncoding completion:^(NSString *data, NSError *error) {
  CFShow(data);
}];
```

See <https://github.com/holepunchto/bare-ios> for a complete example of using the kit in an iOS application.

### Android

In this example we create a simple RPC instance for Android using Bare Kit and Java that sends a `ping` request, handles the request and outputs `pong`.

Import necessary classes from BareKit.

```java
import to.holepunch.bare.kit.IPC;
import to.holepunch.bare.kit.RPC;
import to.holepunch.bare.kit.Worklet;
```

Create a new worklet instance

```java
Worklet worklet = new Worklet();
```

Start the worklet, loading the bundle from assets

```java
try {
  worklet.start("/app.bundle", getAssets().open("app.bundle"));
} catch (Exception e) {
  throw new RuntimeException(e);
}
```

Create an IPC instance using the worklet

```java
IPC ipc = new IPC(worklet);
```

Create an RPC instance with the IPC and a request handler

```java
RPC rpc = new RPC(ipc, (req, error) -> {
  if (req.command.equals("ping")) {
    Log.i(TAG, req.data("UTF-8"));

    req.reply("pong", "UTF-8");
  }
});
```

Create an outgoing request with command "ping"

```java
RPC.OutgoingRequest req = rpc.request("ping");
```

Send the request with data "ping"

```java
req.send("ping", "UTF-8");
```

Get the reply data asynchronously and log the received data

```java
req.reply("UTF-8", (data, error) -> {
  Log.i(TAG, data);
});
```

See <https://github.com/holepunchto/bare-android> for a complete example of using the kit in an Android application.

## License

Apache-2.0
