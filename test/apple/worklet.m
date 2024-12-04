#import <BareKit/BareKit.h>
#import <Foundation/Foundation.h>

#import <assert.h>

int
main() {
  BareWorklet *worklet = [[BareWorklet alloc] initWithConfiguration:nil];

  NSString *source = @"BareKit.IPC.on('data', (data) => BareKit.IPC.write(data))";

  [worklet start:@"/app.js" source:[source dataUsingEncoding:NSUTF8StringEncoding] arguments:@[]];

  BareIPC *ipc = [[BareIPC alloc] initWithWorklet:worklet];

  [ipc write:@"Ping"
      encoding:NSUTF8StringEncoding
    completion:^(NSError *error) {
      assert(error == nil);
    }];

  [ipc read:^(NSData *data, NSError *error) {
    assert(error == nil);

    assert([data isEqualToData:[@"Ping" dataUsingEncoding:NSUTF8StringEncoding]]);

    exit(0);
  }];

  [[NSRunLoop currentRunLoop] run];
}
