#import <BareKit/BareKit.h>
#import <Foundation/Foundation.h>
#import <assert.h>

// A readable handler that clears the writable handler must not have the cleared
// handler called after it for the same wake.

int
main() {
  BareWorklet *worklet = [[BareWorklet alloc] initWithConfiguration:nil];

  NSString *source = @"BareKit.IPC.on('data', () => {}).write('Hello!')";

  [worklet start:@"/app.js" source:[source dataUsingEncoding:NSUTF8StringEncoding] arguments:@[]];

  BareIPC *ipc = [[BareIPC alloc] initWithWorklet:worklet];

  ipc.writable = ^(BareIPC *ipc) {};

  ipc.readable = ^(BareIPC *ipc) {
    while ([ipc read] != nil) {}

    ipc.writable = nil;
  };

  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
    exit(0);
  });

  [[NSRunLoop currentRunLoop] run];
}
