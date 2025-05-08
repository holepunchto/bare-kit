#import <BareKit/BareKit.h>
#import <Foundation/Foundation.h>

int
main() {
  BareWorklet *worklet = [[BareWorklet alloc] initWithConfiguration:nil];

  NSString *source = @"BareKit.IPC.on('data', (data) => BareKit.IPC.write(data)).write('Hello!')";

  [worklet start:@"/app.js" source:[source dataUsingEncoding:NSUTF8StringEncoding] arguments:@[]];

  BareIPC *ipc = [[BareIPC alloc] initWithWorklet:worklet];

  [ipc read:^(NSData *_Nullable data, NSError *_Nullable error) {
    NSLog(@"%@", data);

    [ipc write:[@"Hello back!" dataUsingEncoding:NSUTF8StringEncoding]
      completion:^(NSError *_Nullable error) {
        [ipc read:^(NSData *_Nullable data, NSError *_Nullable error) {
          NSLog(@"%@", data);

          exit(0);
        }];
      }];
  }];

  [[NSRunLoop currentRunLoop] run];
}
