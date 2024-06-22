#import <Foundation/Foundation.h>

#import <assert.h>

#import "BareKit.h"

#import "../../worklet.h"

@implementation BareWorklet {
  bare_worklet_t worklet;
}

@synthesize incoming;
@synthesize outgoing;

- (id)init {
  self = [super init];

  if (self) {
    int err;
    err = bare_worklet_init(&worklet);
    assert(err == 0);
  }

  return self;
}

- (void)dealloc {
  bare_worklet_destroy(&worklet);

#if !__has_feature(objc_arc)
  [super dealloc];
#endif
}

- (void)start:(nonnull NSString *)filename source:(nonnull NSData *)source {
  int err;
  err = bare_worklet_start(
    &worklet,
    [filename cStringUsingEncoding : NSUTF8StringEncoding],
    &(uv_buf_t) { (char *) source.bytes, source.length }
  );
  assert(err == 0);

  incoming = [[NSFileHandle alloc]
    initWithFileDescriptor:worklet.incoming
            closeOnDealloc:YES];

  outgoing = [[NSFileHandle alloc]
    initWithFileDescriptor:worklet.outgoing
            closeOnDealloc:YES];
}

- (void)suspend {
  int err;
  err = bare_worklet_suspend(&worklet, 0);
  assert(err == 0);
}

- (void)suspendWithLinger:(int)linger {
  int err;
  err = bare_worklet_suspend(&worklet, linger);
  assert(err == 0);
}

- (void)resume {
  int err;
  err = bare_worklet_resume(&worklet);
  assert(err == 0);
}

- (void)terminate {
  int err;
  err = bare_worklet_terminate(&worklet);
  assert(err == 0);
}

@end
