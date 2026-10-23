#import <BareKit/BareKit.h>
#import <Foundation/Foundation.h>
#import <assert.h>
#import <objc/runtime.h>

// A read waiting on `readable` holds the ipc. Closing completes the read with
// end-of-stream, which lets go, so the ipc is deallocated once released.

static int deallocs = 0;

static void (*dealloc)(id, SEL);

static void
on_dealloc(id self, SEL cmd) {
  deallocs++;

  dealloc(self, cmd);
}

int
main() {
  Method method = class_getInstanceMethod([BareIPC class], @selector(dealloc));

  dealloc = (void (*)(id, SEL)) method_setImplementation(method, (IMP) on_dealloc);

  BareWorklet *worklet = [[BareWorklet alloc] initWithConfiguration:nil];

  NSString *source = @"BareKit.IPC.on('data', () => {})";

  [worklet start:@"/app.js" source:[source dataUsingEncoding:NSUTF8StringEncoding] arguments:@[]];

  BareIPC *ipc = [[BareIPC alloc] initWithWorklet:worklet];

  __block bool completed = false;

  [ipc read:^(NSData *data, NSError *error) {
    assert(data == nil && error == nil);

    completed = true;
  }];

  [ipc close];
  [ipc release];

  [worklet terminate];
  [worklet release];

  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
    assert(completed);
    assert(deallocs == 1);

    exit(0);
  });

  [[NSRunLoop currentRunLoop] run];
}
