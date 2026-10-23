#import <BareKit/BareKit.h>
#import <Foundation/Foundation.h>
#import <assert.h>

// Releasing the last reference to an ipc on the main thread, while its handler
// waits on the main thread, must not deadlock.

int
main() {
  BareWorklet *worklet = [[BareWorklet alloc] initWithConfiguration:nil];

  NSString *source = @"BareKit.IPC.on('data', () => {}).write('Hello!')";

  [worklet start:@"/app.js" source:[source dataUsingEncoding:NSUTF8StringEncoding] arguments:@[]];

  BareIPC *ipc = [[BareIPC alloc] initWithWorklet:worklet];

  [worklet release];

  dispatch_semaphore_t entered = dispatch_semaphore_create(0);

  __block bool synced = false;

  ipc.readable = ^(BareIPC *ipc) {
    ipc.readable = nil;

    dispatch_semaphore_signal(entered);

    dispatch_sync(dispatch_get_main_queue(), ^{
      synced = true;
    });
  };

  dispatch_semaphore_wait(entered, DISPATCH_TIME_FOREVER);

  [ipc release];

  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
    assert(synced);

    exit(0);
  });

  [[NSRunLoop currentRunLoop] run];
}
