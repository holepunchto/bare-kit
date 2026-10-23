#import <BareKit/BareKit.h>
#import <Foundation/Foundation.h>
#import <assert.h>

// A terminated worklet refuses to open an ipc or to start, whether or not it
// was started before.

int
main() {
  BareWorklet *unstarted = [[BareWorklet alloc] initWithConfiguration:nil];

  [unstarted terminate];

  assert([[BareIPC alloc] initWithWorklet:unstarted] == nil);

  bool raised = false;

  @try {
    [unstarted start:@"/app.js" source:[@"" dataUsingEncoding:NSUTF8StringEncoding] arguments:@[]];
  } @catch (NSException *exception) {
    raised = true;
  }

  assert(raised);

  [unstarted release];

  BareWorklet *started = [[BareWorklet alloc] initWithConfiguration:nil];

  [started start:@"/app.js" source:[@"" dataUsingEncoding:NSUTF8StringEncoding] arguments:@[]];

  [started terminate];

  assert([[BareIPC alloc] initWithWorklet:started] == nil);

  [started release];

  return 0;
}
