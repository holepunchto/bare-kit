#import <BareKit/BareKit.h>
#import <Foundation/Foundation.h>

static char buffer[4 * 1024 * 1024];

int
main() {
  BareWorklet *worklet = [[BareWorklet alloc] initWithConfiguration:nil];

  NSString *source = @"BareKit.IPC.on('data', (data) => BareKit.IPC.write(data))";

  [worklet start:@"/app.js" source:[source dataUsingEncoding:NSUTF8StringEncoding] arguments:@[]];

  BareIPC *ipc = [[BareIPC alloc] initWithWorklet:worklet];

  [ipc write:[[NSData alloc] initWithBytesNoCopy:buffer length:sizeof(buffer)]
    completion:^(NSError *_Nullable error) {
      __block NSInteger received = 0;

      ipc.readable = ^(BareIPC *ipc) {
        NSData *data = [ipc read];

        assert(data != nil);

        received += data.length;

        if (received == sizeof(buffer)) {
          exit(0);
        }
      };
    }];

  [[NSRunLoop currentRunLoop] run];
}
