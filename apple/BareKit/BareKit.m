#import <Foundation/Foundation.h>

#import <assert.h>

#import "BareKit.h"

#import "../../shared/worklet.h"

@implementation BareWorklet {
  bare_worklet_t worklet;
}

- (int)incoming {
  return worklet.incoming;
}

- (int)outgoing {
  return worklet.outgoing;
}

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

- (void)start:(NSString *_Nonnull)filename source:(NSData *_Nonnull)source {
  int err;
  err = bare_worklet_start(
    &worklet,
    [filename cStringUsingEncoding : NSUTF8StringEncoding],
    &(uv_buf_t) { (char *) source.bytes, source.length }
  );
  assert(err == 0);
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

@implementation BareIPC {
  NSFileHandle *incoming;
  NSFileHandle *outgoing;
}

- (_Nullable id)initWithWorklet:(BareWorklet *_Nonnull)worklet {
  self = [super init];

  if (self) {
    incoming = [[NSFileHandle alloc]
      initWithFileDescriptor:worklet.incoming
              closeOnDealloc:YES];

    outgoing = [[NSFileHandle alloc]
      initWithFileDescriptor:worklet.outgoing
              closeOnDealloc:YES];
  }

  return self;
}

- (void)read:(void (^_Nonnull)(NSData *_Nullable data))completion {
  incoming.readabilityHandler = ^(NSFileHandle *handle) {
    NSData *data = [handle availableData];

    handle.readabilityHandler = nil;

    if (data.length == 0) {
      completion(nil);
    } else {
      completion(data);
    }
  };
}

- (void)read:(NSStringEncoding)encoding completion:(void (^_Nonnull)(NSString *_Nullable data))completion {
  incoming.readabilityHandler = ^(NSFileHandle *handle) {
    NSData *data = [handle availableData];

    handle.readabilityHandler = nil;

    if (data.length == 0) {
      completion(nil);
    } else {
      completion([[NSString alloc] initWithData:data encoding:encoding]);
    }
  };
}

- (void)write:(NSData *_Nonnull)data completion:(void (^_Nonnull)(NSError *_Nullable error))completion {
  outgoing.writeabilityHandler = ^(NSFileHandle *handle) {
    handle.writeabilityHandler = nil;

    NSError *error = nil;

    [outgoing writeData:data error:&error];

    completion(error);
  };
}

- (void)write:(NSData *_Nonnull)data {
  outgoing.writeabilityHandler = ^(NSFileHandle *handle) {
    handle.writeabilityHandler = nil;

    [outgoing writeData:data];
  };
}

- (void)write:(NSString *_Nonnull)data encoding:(NSStringEncoding)encoding completion:(void (^_Nonnull)(NSError *_Nullable error))completion {
  [self write:[data dataUsingEncoding:encoding] completion:completion];
}

- (void)write:(NSString *_Nonnull)data encoding:(NSStringEncoding)encoding {
  [self write:[data dataUsingEncoding:encoding]];
}

- (void)close {
  [incoming closeFile];
  [outgoing closeFile];
}

@end
