# bare-kit

Bare for native application development. The kit provides a web worker-like API to start and manage isolated Bare threads, called worklets[^1], that expose an IPC abstraction with bindings for various languages.

[^1]: This term was chosen to avoid ambiguity with worker threads as implemented by <https://github.com/holepunchto/bare-worker>.

## Push Notifications

`BareKit` provides two native classes to handle push notifications on **iOS** and **Android**. In both cases, you must provide a **JavaScript file** called a **worklet** (as defined below).

To handle push notifications, add a listener to the `push` event emitted by `BareKit`:

```js
BareKit.on('push', (payload, reply) => {
  console.log('📩 Received a notification:', payload.toString())


  let err = null
  const enrichedPayload = {}

  reply(err, JSON.stringify(enrichedPayload))
})
```

* `payload`: Contains the notification content as a serializable object. The content varies depending on the platform.
* `reply`: A function that takes three arguments:
	* `error`: A JavaScript Error object.
	*	`buffer`: A JavaScript Buffer or String containing the modified notification payload.
	*	`encoding`: A JavaScript String representing the encoding of the buffer (e.g., "utf8").

Since the notification content structure (`buffer`) differs by platform, we’ll detail the differences below.

### iOS

#### 📌 Setup

On **iOS**, you must create a **Notification Service Extension** containing a class that extends `BareKit.NotificationService`.

Using Xcode? [Modifying Content in Newly Delivered Notifications](https://developer.apple.com/documentation/usernotifications/modifying-content-in-newly-delivered-notifications)

Using XcodeGen? We maintain a XcodeGen template to generate the extension automatically: GitHub: [bare-ios](https://github.com/holepunchto/bare-ios)

#### 🛠 Creating the NotificationService Class

🔹 Basic Example (Swift)
```swift
import BareKit

class NotificationService: BareKit.NotificationService {
  override init() {
    super.init(resource: "push", ofType: "js")
  }
}
```

*	`push` is the name of the worklet file.
*	`js` is the file extension.

🔹 Passing Arguments & Configuration

```swift
import BareKit

class NotificationService: BareKit.NotificationService {
  override init() {
    let args = ["foo", "bar"]
    let conf = Worklet.Configuration()
    super.init(resource: "push", ofType: "js", arguments: args, configuration: conf)
  }
}
```

In the worklet, you can access `arguments` like this:

```js
const foo = Bare.argv[0] // foo
const bar = Bare.argv[1] // bar
```
The configuration object allows you to customize the behavior of the worklet by setting:
*	`memoryLimit`: Maximum memory allowed for the worklet (in bytes, e.g., 1024 * 1024 * 24 for 24 MiB).
*	`assets`: Path to asset files that the worklet can access.

> 📖 More about BareKit types on Github: [bare-kit-swift](https://github.com/holepunchto/bare-kit-swift/blob/main/Sources/BareKit/BareKit.swift)

#### 🔔 iOS Notification Payload

iOS **restricts** what can be modified inside a **Notification Service Extension**. BareKit provides an encapsulated handler for creating and displaying notifications, so you only need to call `reply` with the expected arguments.

```js
BareKit.on('push', (payload, reply) => {
  const notification = {
    title: "Hello",
    subtitle: "Hello from a worklet",
    body: "This is a test notification"
  }

  reply(err, JSON.stringify(notification))
})
```

This list details all the properties you can pass for iOS:

| Parameter                 | Type                    | Description |
|---------------------------|-------------------------|-------------|
| `title`                   | `string`                | The title of the notification |
| `subtitle`                | `string`                | Subtitle of the notification |
| `body`                    | `string`                | Main content text of the notification |
| `userInfo`                | `object`                | Additional data payload for the app |
| `badge`                   | `number`                | Badge count on the app icon |
| `targetContentIdentifier` | `string`                | Identifier for deep linking or targeting |
| `sound.type`              | `"default" \| "named"`  | Type of notification sound |
| `sound.name`              | `string`                | Sound file name (if `type` is `"named"`) |
| `sound.critical`          | `boolean`               | Whether the sound is critical |
| `sound.volume`            | `number`                | Volume level (0.0 - 1.0) |
| `threadIdentifier`        | `string`                | Groups notifications under a thread |
| `categoryIdentifier`      | `string`                | Defines notification actions |

This is more or less the list of properties you can pass to an iOS notification.

## Android

Todo

## License

Apache-2.0
