#import <BareKit/BareKit.h>
#import <Foundation/Foundation.h>

int
main() {
  BareWorklet *worklet = [[BareWorklet alloc] initWithConfiguration:nil];

  NSString *source = @"BareKit.on('push', (data, reply) => reply(null, 'Hello world!'))";

  [worklet start:@"/app.js" source:[source dataUsingEncoding:NSUTF8StringEncoding] arguments:@[]];

  [worklet push:@"Push payload"
       encoding:NSUTF8StringEncoding
     completion:^(NSString *_Nullable reply, NSError *_Nullable error) {
       NSLog(@"%@", reply);

       exit(0);
     }];

  [[NSRunLoop currentRunLoop] run];
}
