#import <Foundation/Foundation.h>

#import <assert.h>

#import "BareKit.h"

#import "../../worklet.h"

@implementation BareWorklet {
  NSFileHandle *_incoming;
  NSFileHandle *_outgoing;

  bare_worklet_t _worklet;
}

- (id)init {
  self = [super init];

  if (self) {
    int err;
    err = bare_worklet_init(&_worklet);
    assert(err == 0);
  }

  return self;
}

- (void)dealloc {
  bare_worklet_destroy(&_worklet);

#if !__has_feature(objc_arc)
  [super dealloc];
#endif
}

- (void)start:(NSString *)filename source:(NSData *)source {
  const char *_filename = [filename cStringUsingEncoding:NSUTF8StringEncoding];

  uv_buf_t _source = uv_buf_init((char *) source.bytes, source.length);

  int err;
  err = bare_worklet_start(&_worklet, _filename, &_source);
  assert(err == 0);

  _incoming = [[NSFileHandle alloc]
    initWithFileDescriptor:_worklet.incoming
            closeOnDealloc:YES];

  _outgoing = [[NSFileHandle alloc]
    initWithFileDescriptor:_worklet.outgoing
            closeOnDealloc:YES];
}

- (NSFileHandle *)incoming {
  return _incoming;
}

- (NSFileHandle *)outgoing {
  return _outgoing;
}

- (void)suspend {
  int err;
  err = bare_worklet_suspend(&_worklet);
  assert(err == 0);
}

- (void)resume {
  int err;
  err = bare_worklet_resume(&_worklet);
  assert(err == 0);
}

@end
