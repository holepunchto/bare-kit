# bare-kit

Bare for native application development. The kit provides a web worker-like API to start and manage isolated Bare threads, called worklets[^1], that expose an IPC abstraction with bindings for various languages.

[^1]: This term was chosen to avoid ambiguity with worker threads as implemented by <https://github.com/holepunchto/bare-worker>.

## Worklet

In this section, we will learn how to create a worklet both iOS and Android.

### iOS

This is a basic example of how to create a worklet in Objective-C:

```objc
#import <BareKit/BareKit.h>
#import <Foundation/Foundation.h>

int
main() {
  BareWorkletConfiguration *options = [BareWorkletConfiguration defaultWorkletConfiguration];
  options.memoryLimit = 1024 * 1024 * 24; // 24 MiB

  BareWorklet *worklet = [[BareWorklet alloc] initWithConfiguration:options];

  NSString *source = @"console.log('hello from the worklet')";

  [worklet start:@"/app.js" source:[source dataUsingEncoding:NSUTF8StringEncoding] arguments:@[]];

  [[NSRunLoop currentRunLoop] run];
}
```

You can create a `BareWorkletConfiguration` to optionally set the memory limit or attach assets. This step is optional.

The configuration is then passed to the `BareWorklet`, which represents the running worklet instance.

Available actions on a worklet include:

- `[worklet start]`: starts the worklet
- `[worklet suspend]`: suspends the execution of the worklet
- `[worklet suspendWithLinger]`: suspends the worklet for a specified duration; the linger integer (in milliseconds) defines how long to keep the process alive before it fully exits
- `[worklet resume]`: resumes a suspended worklet
- `[worklet terminate]`: terminates the worklet

> [!TIP]
> For a full API reference, see [`apple/BareKit/BareKit.h`](apple/BareKit/BareKit.h).

### Android

This is a basic example of how to create a worklet in Java:

```java
import to.holepunch.bare.kit.Worklet;
import java.nio.charset.StandardCharsets;

Worklet.Options options = new Worklet.Options()
  .memoryLimit(24 * 1024 * 1024); // 24 MiB

Worklet worklet = new Worklet(options);

String source = "console.log('hello from the worklet')";

worklet.start("/app.js", source, StandardCharsets.UTF_8, null);
```

You can create a `Worklet.Options` to optionally set the memory limit or attach assets. This step is optional.

The configuration is then passed to the `Worklet`, which represents the running worklet instance.

Available actions on a worklet include:

- `worklet.start()`: starts the worklet
- `worklet.suspend()`: suspends the execution of the worklet
- `worklet.suspend(linger)`: suspends the worklet for a specified duration; the linger integer (in milliseconds) defines how long to keep the process alive before it fully exits
- `worklet.resume()`: resumes a suspended worklet
- `worklet.terminate()`: terminates the worklet

> [!TIP]
> For a full API reference, see [`android/src/main/java/to/holepunch/bare/kit/Worklet.java`](android/src/main/java/to/holepunch/bare/kit/Worklet.java).

## IPC

Bare Kit provides an IPC (Inter-Process Communication) abstraction that allows communication between the main application and worklets. The IPC interface provides both synchronous and asynchronous read/write operations with callback-based event handling.

> [!IMPORTANT]
> IPC operations are **non-blocking** regardless of whether you use synchronous or asynchronous APIs. They may return partial results (e.g., writing fewer bytes than requested) and require polling to complete the operation.

### iOS

#### Polling IPC

The `BareIPC` class provides these methods for polling IPC:

- `ipc.readable`: callback property for when data is available to read
- `ipc.writable`: callback property for when data can be written
- `[ipc read]`: synchronously reads available data, returns `nil` if no data is available
- `[ipc write:data]`: synchronously writes data, returns the number of bytes written

```objc
#import <BareKit/BareKit.h>
#import <Foundation/Foundation.h>

int
main() {
  BareWorkletConfiguration *options = [BareWorkletConfiguration defaultWorkletConfiguration];
  options.memoryLimit = 1024 * 1024 * 24; // 24 MiB

  BareWorklet *worklet = [[BareWorklet alloc] initWithConfiguration:options];
  BareIPC *ipc = [[BareIPC alloc] initWithWorklet:worklet];

  NSString *source = @"console.log('hello from the worklet')";
  [worklet start:@"/app.js" source:[source dataUsingEncoding:NSUTF8StringEncoding] arguments:@[]];

  // Set up readable callback
  ipc.readable = ^(BareIPC *ipc) {
    NSData *data = [ipc read];
    if (data) {
      NSString *message = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
      NSLog(@"Received: %@", message);
    }
  };

  // Set up writable callback and write data
  NSString *message = @"Hello from main thread";
  NSData *data = [message dataUsingEncoding:NSUTF8StringEncoding];

  ipc.writable = ^(BareIPC *ipc) {
    NSInteger written = [ipc write:data];
    NSLog(@"Written %ld bytes", (long)written);
    ipc.writable = nil; // Clear the callback after writing
  };

  [[NSRunLoop currentRunLoop] run];
}
```

> [!NOTE]
> On iOS, IPC callbacks run on a dedicated queue, so callers must be mindful of synchronization when accessing shared resources from these callbacks.

#### Async IPC

The `BareIPC` class provides these methods for async IPC:

- `[ipc read:completion]`: asynchronously reads data with a completion callback
- `[ipc write:data completion:]`: asynchronously writes data with a completion callback

```objc
#import <BareKit/BareKit.h>
#import <Foundation/Foundation.h>

int
main() {
  BareWorkletConfiguration *options = [BareWorkletConfiguration defaultWorkletConfiguration];
  options.memoryLimit = 1024 * 1024 * 24; // 24 MiB

  BareWorklet *worklet = [[BareWorklet alloc] initWithConfiguration:options];
  BareIPC *ipc = [[BareIPC alloc] initWithWorklet:worklet];

  NSString *source = @"console.log('hello from the worklet')";
  [worklet start:@"/app.js" source:[source dataUsingEncoding:NSUTF8StringEncoding] arguments:@[]];

  // Read data asynchronously
  [ipc read:^(NSData *data, NSError *error) {
    if (error) {
      NSLog(@"Read error: %@", error);
    } else if (data) {
      NSString *message = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
      NSLog(@"Received: %@", message);
    }
  }];

  // Write data asynchronously
  NSString *message = @"Hello from main thread";
  NSData *data = [message dataUsingEncoding:NSUTF8StringEncoding];
  [ipc write:data completion:^(NSError *error) {
    if (error) {
      NSLog(@"Write error: %@", error);
    } else {
      NSLog(@"Message sent successfully");
    }
  }];

  [[NSRunLoop currentRunLoop] run];
}
```

**Utility methods**

- `[ipc close]`: closes the IPC connection

> [!TIP]
> For a full API reference, see [`apple/BareKit/BareKit.h`](apple/BareKit/BareKit.h).

### Android

#### Polling IPC

The `IPC` class provides these methods for polling IPC:

- `ipc.readable(callback)`: sets a callback for when data is available to read
- `ipc.writable(callback)`: sets a callback for when data can be written
- `ipc.read()`: synchronously reads available data, returns `null` if no data is available
- `ipc.write(data)`: synchronously writes data, returns the number of bytes written

```java
import to.holepunch.bare.kit.Worklet;
import to.holepunch.bare.kit.IPC;
import java.nio.charset.StandardCharsets;
import java.nio.ByteBuffer;

Worklet.Options options = new Worklet.Options()
  .memoryLimit(24 * 1024 * 1024); // 24 MiB

Worklet worklet = new Worklet(options);
IPC ipc = new IPC(worklet);

String source = "console.log('hello from the worklet')";
worklet.start("/app.js", source, StandardCharsets.UTF_8, null);

  // Set up readable callback
  ipc.readable(() -> {
    ByteBuffer data = ipc.read();
    if (data != null) {
      byte[] bytes = new byte[data.remaining()];
      data.get(bytes);
      String message = new String(bytes, StandardCharsets.UTF_8);
      System.out.println("Received: " + message);
    }
  });

  // Set up writable callback and write data
  String message = "Hello from main thread";
  ByteBuffer data = ByteBuffer.wrap(message.getBytes(StandardCharsets.UTF_8));

  ipc.writable(() -> {
    int written = ipc.write(data);
    System.out.println("Written " + written + " bytes");
    ipc.writable(null); // Clear the callback after writing
  });
```

#### Async IPC

The `IPC` class provides these methods for async IPC:

- `ipc.read(callback)`: asynchronously reads data with a callback
- `ipc.write(data, callback)`: asynchronously writes data with a callback

```java
import to.holepunch.bare.kit.Worklet;
import to.holepunch.bare.kit.IPC;
import java.nio.charset.StandardCharsets;
import java.nio.ByteBuffer;

Worklet.Options options = new Worklet.Options()
  .memoryLimit(24 * 1024 * 1024); // 24 MiB

Worklet worklet = new Worklet(options);
IPC ipc = new IPC(worklet);

String source = "console.log('hello from the worklet')";
worklet.start("/app.js", source, StandardCharsets.UTF_8, null);

// Read data asynchronously
ipc.read((data, exception) -> {
  if (exception != null) {
    System.err.println("Read error: " + exception.getMessage());
  } else if (data != null) {
    byte[] bytes = new byte[data.remaining()];
    data.get(bytes);
    String message = new String(bytes, StandardCharsets.UTF_8);
    System.out.println("Received: " + message);
  }
});

// Write data asynchronously
String message = "Hello from main thread";
ByteBuffer data = ByteBuffer.wrap(message.getBytes(StandardCharsets.UTF_8));
ipc.write(data, (exception) -> {
  if (exception != null) {
    System.err.println("Write error: " + exception.getMessage());
  } else {
    System.out.println("Message sent successfully");
  }
});
```

**Utility methods**

- `ipc.close()`: closes the IPC connection

> [!TIP]
> For a full API reference, see [`android/src/main/java/to/holepunch/bare/kit/IPC.java`](android/src/main/java/to/holepunch/bare/kit/IPC.java).

## Notifications

Bare Kit provides two native classes for implementing push notification support on iOS and Android. In both cases, you must provide an entry point to the notification worklet responsible for handling incoming push notifications. To handle push notifications, add a listener for the `push` event emitted on the `BareKit` namespace:

```js
BareKit.on('push', (payload, reply) => {
  console.log('📩 Received a notification: %s', payload)

  let err = null
  let data

  reply(err, data)
})
```

- `payload`: Contains the notification content as a serializable object. The content varies depending on the platform.
- `reply`: A function that takes three arguments:
  - `error`: An `Error` object in case of an error, otherwise `null`.
  - `payload`: A `Buffer` or string containing the data to pass back to the native code.
  - `encoding`: The encoding of the payload string, defaults to `utf8`.

Since the notification content structure (the `payload` argument of the `reply()` callback) differs by platform, we’ll detail the differences below.

### iOS

#### Setup

On iOS you must create an [app extension](https://developer.apple.com/app-extensions/) with a class that extends the `BareKit.NotificationService` base implementation. If using Xcode, follow the guide [Modifying Content in Newly Delivered Notifications](https://developer.apple.com/documentation/usernotifications/modifying-content-in-newly-delivered-notifications). We also maintain a template at <https://github.com/holepunchto/bare-ios> to generate the extension automatically.

#### Creating the notification service

**Basic example**

```swift
import BareKit

class NotificationService: BareKit.NotificationService {
  override init() {
    super.init(resource: "push", ofType: "js")
  }
}
```

- `push` is the name of the worklet entry point.
- `js` is the file extension.

**Passing arguments and configuration**

```swift
import BareKit

class NotificationService: BareKit.NotificationService {
  override init() {
    let arguments = ["foo", "bar"]
    let configuration = Worklet.Configuration()

    super.init(resource: "push", ofType: "js", arguments: arguments, configuration: configuration)
  }
}
```

In the worklet, you can access `arguments` like this:

```js
const foo = Bare.argv[0]
const bar = Bare.argv[1]
```

The configuration object allows you to customize the behavior of the worklet by setting:

- `memoryLimit`: The heap memory limit of the worklet in bytes, e.g. `10 * 1024 * 1024` for 10 MiB.
- `assets`: Path to where bundled assets of the worklet entry point may be written. Must be provided if the worklet entry point uses assets.

> [!TIP]  
> For more information on the Swift interface, see [`BareKit.swift`](https://github.com/holepunchto/bare-kit-swift/blob/main/Sources/BareKit/BareKit.swift).

#### iOS notification payload

iOS limits what can be modified inside a notification service extension. Bare Kit provides an encapsulated handler for creating and displaying notifications, so you only need to call `reply` with the notification description encoded as a JSON object.

```js
BareKit.on('push', (payload, reply) => {
  const notification = {
    title: 'Hello',
    subtitle: 'Hello from a worklet',
    body: 'This is a test notification'
  }

  reply(null, JSON.stringify(notification))
})
```

This table describes the properties you can pass for iOS:

| Property                  | Type                   | Description                              |
| ------------------------- | ---------------------- | ---------------------------------------- |
| `title`                   | `string`               | The title of the notification            |
| `subtitle`                | `string`               | Subtitle of the notification             |
| `body`                    | `string`               | Main content text of the notification    |
| `userInfo`                | `object`               | Additional data payload for the app      |
| `badge`                   | `number`               | Badge count on the app icon              |
| `targetContentIdentifier` | `string`               | Identifier for deep linking or targeting |
| `sound.type`              | `"default" \| "named"` | Type of notification sound               |
| `sound.name`              | `string`               | Sound file name (if `type` is `"named"`) |
| `sound.critical`          | `boolean`              | Whether the sound is critical            |
| `sound.volume`            | `number`               | Volume level (0.0 - 1.0)                 |
| `threadIdentifier`        | `string`               | Groups notifications under a thread      |
| `categoryIdentifier`      | `string`               | Defines notification actions             |

### Android

#### Setup

On Android you must create a [service](https://developer.android.com/develop/background-work/services) that extends the `MessagingService` base implementation. We maintain a template at <https://github.com/holepunchto/bare-android> to generate the service automatically.

#### Creating the notification service

**Start the worklet**

```kt
import to.holepunch.bare.kit.Worklet
import to.holepunch.bare.kit.MessagingService as BaseMessagingService

class MessagingService : BaseMessagingService(Worklet.Options()) {
  override fun onCreate() {
    super.onCreate()

    try {
      this.start("push.js", assets.open("push.js"), null)
    } catch (e: Exception) {
      throw RuntimeException(e)
    }
  }
}
```

**Passing arguments and configuration**

```kt
import to.holepunch.bare.kit.Worklet
import to.holepunch.bare.kit.MessagingService as BaseMessagingService

class MessagingService : BaseMessagingService(Worklet.Options()) {
  override fun onCreate() {
    super.onCreate()

    try {
      val arguments = ["foo", "bar"]

      this.start("push.js", assets.open("push.js"), arguments)
    } catch (e: Exception) {
      throw RuntimeException(e)
    }
  }
}
```

> [!TIP]  
> For more information on the Java/Kotlin interface, see [`MessagingService.java`](https://github.com/holepunchto/bare-kit/blob/main/android/src/main/java/to/holepunch/bare/kit/MessagingService.java>).

#### Android notification payload

Unlike iOS, Android is much less restrictive in handling notifications. This is why, on Android, the `MessagingService` class provides the method `onWorkletReply(reply: JSONObject)` to handle the reply from the `push` listener:

```kt
override fun onWorkletReply(reply: JSONObject) {
  try {
    notificationManager!!.notify(
      1,
      Notification.Builder(this, CHANNEL_ID)
        .setSmallIcon(drawable.ic_dialog_info)
        .setContentTitle(reply.optString("myTitle", "Default title"))
        .setContentText(reply.optString("mybody", "Default description"))
        .setAutoCancel(true)
        .build()
    )
  } catch (e: Exception) {
    throw RuntimeException(e)
  }
}
```

The properties provided in the `reply` object are entirely determined by what you implement in the `push` listener:

```js
BareKit.on('push', (payload, reply) => {
  const notification = {
    myTitle: 'Hello',
    myBody: 'This is a test notification'
  }

  reply(null, JSON.stringify(notification))
})
```

## License

Apache-2.0
