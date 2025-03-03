console.log("In push-notif.js")
BareKit.on('push', (json, reply) => {
  console.log("Push notification received")
  console.log(json.toString())
  const fakeNotificationPayload = {
    title: "BareKit News",
    subtitle: "Important Update",
    body: "This is a test notification with all properties.",
  };

  reply(null, JSON.stringify(fakeNotificationPayload), "utf8");
})
