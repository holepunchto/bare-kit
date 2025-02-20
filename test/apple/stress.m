#import <BareKit/BareKit.h>
#import <Foundation/Foundation.h>

int
main() {
  BareWorklet *worklet = [[BareWorklet alloc] initWithConfiguration:nil];

  NSString *source = @"BareKit.IPC.on('data', (data) => BareKit.IPC.write(data))";

  [worklet start:@"/app.js" source:[source dataUsingEncoding:NSUTF8StringEncoding] arguments:@[]];

  BareIPC *ipc = [[BareIPC alloc] initWithWorklet:worklet];

  __block int i = 0;
  NSMutableArray<NSString *> *messages = [NSMutableArray new];

  ipc.readable = ^(BareIPC *ipc) {
    while (true) {
      NSString *data = [ipc read:NSUTF8StringEncoding];

      if (data == nil) break;

      NSArray *parts = [data componentsSeparatedByString:@"\n"];
      for (NSUInteger i = 0; i < parts.count - 1; i++) {
        [messages addObject:parts[i]];
      }
    }

    NSLog(@"Messages written %d", i);
    NSLog(@"Messages read %lu", [messages count]);
    if (([messages count]) == i) {
      NSLog(@"All messages read!");
      [ipc close];
      exit(0);
    } else {
      NSLog(@"Messages missing %d", i - (int) [messages count]);
      [ipc close];
      exit(1);
    }
  };

  ipc.writable = ^(BareIPC *ipc) {
    while (i < 100000) {
      BOOL success = [ipc write:[NSString stringWithFormat:@"Message #%d\n", i] encoding:NSUTF8StringEncoding];

      if (success) i++;
      else break;
    }
  };

  [[NSRunLoop currentRunLoop] run];
}
