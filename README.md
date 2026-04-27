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

  NSString *source = @"console.log('hello from the worklet')";
  [worklet start:@"/app.js" source:[source dataUsingEncoding:NSUTF8StringEncoding] arguments:@[]];

  BareIPC *ipc = [[BareIPC alloc] initWithWorklet:worklet];

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

## License

Apache-2.0
