#import <BareKit/BareKit.h>
#import <Foundation/Foundation.h>

int
main() {
  BareWorklet *worklet = [[BareWorklet alloc] initWithConfiguration:nil];

  NSString *source = @"BareKit.on('push', (data, reply) => reply(new Error('Abort!'), null))";

  [worklet start:@"/app.js" source:[source dataUsingEncoding:NSUTF8StringEncoding] arguments:@[]];

  [worklet push:@"Push payload"
       encoding:NSUTF8StringEncoding
     completion:^(NSString *_Nullable reply, NSError *_Nullable error) {
       NSLog(@"%@", error);

       exit(0);
     }];

  [[NSRunLoop currentRunLoop] run];
}
