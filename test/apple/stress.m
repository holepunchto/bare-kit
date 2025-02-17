#import <BareKit/BareKit.h>
#import <Foundation/Foundation.h>

int
main() {
  BareWorklet *worklet = [[BareWorklet alloc] initWithConfiguration:nil];

  NSString *source = @"BareKit.IPC.on('data', (data) => BareKit.IPC.write(data))";

  [worklet start:@"/app.js" source:[source dataUsingEncoding:NSUTF8StringEncoding] arguments:@[]];

  BareIPC *ipc = [[BareIPC alloc] initWithWorklet:worklet];

  __block int i = 0;

  ipc.readable = ^(BareIPC *ipc) {
    while (true) {
      NSString *data = [ipc read:NSUTF8StringEncoding];

      if (data == nil) break;

      NSLog(@"%@", data);
    }
  };

  ipc.writable = ^(BareIPC *ipc) {
    while (i < 100000) {
      BOOL success = [ipc write:[NSString stringWithFormat:@"Message #%d", i] encoding:NSUTF8StringEncoding];

      if (success) i++;
      else break;
    }
  };

  [[NSRunLoop currentRunLoop] run];
}
