# bare-kit

Bare for native application development. The kit provides a web worker-like API to start and manage isolated Bare threads, called worklets[^1], that expose an IPC abstraction with bindings for various languages.

[^1]: This term was chosen to avoid ambiguity with worker threads as implemented by <https://github.com/holepunchto/bare-worker>.

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

On iOS you must create an [app extension](https://developer.apple.com/app-extensions/) with a class that extends the `BareKit.NotificationService` base implementation. If using Xcode, follow the guide [Modifying Content in Newly Delivered Notifications](https://developer.apple.com/documentation/usernotifications/modifying-content-in-newly-delivered-notifications). We also maintain a template to generate the extension automatically in <https://github.com/holepunchto/bare-ios>.

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
> For more information on the available Swift types, see <https://github.com/holepunchto/bare-kit-swift>.

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

## Android

Todo

## License

Apache-2.0
