#import <BareKit/BareKit.h>
#import <Foundation/Foundation.h>

int
main() {
  BareWorklet *worklet = [[BareWorklet alloc] initWithConfiguration:nil];

  NSString *source = @"BareKit.IPC.on('data', (data) => BareKit.IPC.write(data)).write('Hello!')";

  [worklet start:@"/app.js" source:[source dataUsingEncoding:NSUTF8StringEncoding] arguments:@[]];

  BareIPC *ipc = [[BareIPC alloc] initWithWorklet:worklet];

  ipc.readable = ^(BareIPC *ipc) {
    NSString *data = [ipc read:NSUTF8StringEncoding];

    if (data == nil) return;

    ipc.readable = nil;

    NSLog(@"%@", data);

    ipc.writable = ^(BareIPC *ipc) {
      BOOL sent = [ipc write:@"Hello back!" encoding:NSUTF8StringEncoding];

      if (sent == NO) return;

      ipc.writable = nil;

      ipc.readable = ^(BareIPC *ipc) {
        NSString *data = [ipc read:NSUTF8StringEncoding];

        if (data == nil) return;

        ipc.readable = nil;

        NSLog(@"%@", data);

        ipc.writable = ^(BareIPC *ipc) {
          BOOL sent = [ipc write:@"Hello back again!" encoding:NSUTF8StringEncoding];

          if (sent == NO) return;

          ipc.writable = nil;

          ipc.readable = ^(BareIPC *ipc) {
            NSString *data = [ipc read:NSUTF8StringEncoding];

            if (data == nil) return;

            ipc.readable = nil;

            NSLog(@"%@", data);

            [ipc close];

            [worklet terminate];

            exit(0);
          };
        };
      };
    };
  };

  [[NSRunLoop currentRunLoop] run];
}
