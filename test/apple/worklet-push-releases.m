#import <BareKit/BareKit.h>
#import <Foundation/Foundation.h>
#import <assert.h>
#import <objc/runtime.h>

// A push releases its context once the reply has been delivered, both when the
// worklet replies and when the worklet is no longer running.

static int deallocs = 0;

static void (*dealloc)(id, SEL);

static void
on_dealloc(id self, SEL cmd) {
  deallocs++;

  dealloc(self, cmd);
}

int
main() {
  Method method = class_getInstanceMethod(NSClassFromString(@"BareWorkletPushContext"), @selector(dealloc));

  dealloc = (void (*)(id, SEL)) method_setImplementation(method, (IMP) on_dealloc);

  BareWorklet *worklet = [[BareWorklet alloc] initWithConfiguration:nil];

  NSString *source = @"BareKit.on('push', (data, reply) => reply(null, 'Hello world!'))";

  [worklet start:@"/app.js" source:[source dataUsingEncoding:NSUTF8StringEncoding] arguments:@[]];

  [worklet push:@"Push payload"
       encoding:NSUTF8StringEncoding
     completion:^(NSString *_Nullable reply, NSError *_Nullable error) {
       assert(error == nil);

       [worklet terminate];

       [worklet push:@"Push payload"
            encoding:NSUTF8StringEncoding
          completion:^(NSString *_Nullable reply, NSError *_Nullable error) {
            assert(error != nil);

            dispatch_async(dispatch_get_main_queue(), ^{
              assert(deallocs == 2);

              exit(0);
            });
          }];
     }];

  [[NSRunLoop currentRunLoop] run];
}
