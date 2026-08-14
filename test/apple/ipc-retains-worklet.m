#import <BareKit/BareKit.h>
#import <Foundation/Foundation.h>
#import <assert.h>

// An IPC is opened on a worklet and is meaningless without it, so it must hold
// it: after the caller releases its own reference the worklet has to still be
// there for the IPC to tear down against.
//
// This file is manual reference counting, so the retain the IPC takes is
// observable directly.

int
main() {
  BareWorklet *worklet = [[BareWorklet alloc] initWithConfiguration:nil];

  NSString *source = @"BareKit.IPC.write('Hello!')";

  [worklet start:@"/app.js" source:[source dataUsingEncoding:NSUTF8StringEncoding] arguments:@[]];

  NSUInteger before = [worklet retainCount];

  BareIPC *ipc = [[BareIPC alloc] initWithWorklet:worklet];

  assert([worklet retainCount] == before + 1);

  // The caller is done with it; the IPC's reference is what keeps it alive.
  [worklet release];

  [ipc close];
  [ipc release];

  return 0;
}
