#import <BareKit/BareKit.h>
#import <Foundation/Foundation.h>

int
main() {
  BareWorklet *worklet = [[BareWorklet alloc] initWithConfiguration:nil];

  NSString *source = @"BareKit.IPC.write('bye'); BareKit.IPC.end()";

  [worklet start:@"/app.js" source:[source dataUsingEncoding:NSUTF8StringEncoding] arguments:@[]];

  BareIPC *ipc = [[BareIPC alloc] initWithWorklet:worklet];

  [ipc read:^(NSData *_Nullable data, NSError *_Nullable error) {
    assert(data != nil); // The message arrives as its own read, before EOF

    [ipc read:^(NSData *_Nullable data, NSError *_Nullable error) {
      assert(data == nil); // EOF must be signalled as nil, not an empty buffer

      exit(0);
    }];
  }];

  [[NSRunLoop currentRunLoop] run];
}
