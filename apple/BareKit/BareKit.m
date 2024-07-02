#import <Foundation/Foundation.h>

#import <assert.h>
#import <compact.h>
#import <rpc.h>

#import "BareKit.h"

#import "../../shared/worklet.h"

@implementation BareWorklet {
  bare_worklet_t _worklet;
}

- (int)incoming {
  return _worklet.incoming;
}

- (int)outgoing {
  return _worklet.outgoing;
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

- (void)start:(NSString *_Nonnull)filename source:(NSData *_Nonnull)source {
  int err;
  err = bare_worklet_start(
    &_worklet,
    [filename cStringUsingEncoding : NSUTF8StringEncoding],
    &(uv_buf_t) { (char *) source.bytes, source.length }
  );
  assert(err == 0);
}

- (void)suspend {
  int err;
  err = bare_worklet_suspend(&_worklet, 0);
  assert(err == 0);
}

- (void)suspendWithLinger:(int)linger {
  int err;
  err = bare_worklet_suspend(&_worklet, linger);
  assert(err == 0);
}

- (void)resume {
  int err;
  err = bare_worklet_resume(&_worklet);
  assert(err == 0);
}

- (void)terminate {
  int err;
  err = bare_worklet_terminate(&_worklet);
  assert(err == 0);
}

@end

@implementation BareIPC {
  NSFileHandle *_incoming;
  NSFileHandle *_outgoing;
}

- (_Nullable id)initWithWorklet:(BareWorklet *_Nonnull)worklet {
  self = [super init];

  if (self) {
    _incoming = [[NSFileHandle alloc]
      initWithFileDescriptor:worklet.incoming
              closeOnDealloc:YES];

    _outgoing = [[NSFileHandle alloc]
      initWithFileDescriptor:worklet.outgoing
              closeOnDealloc:YES];
  }

  return self;
}

- (void)read:(void (^_Nonnull)(NSData *_Nullable data))completion {
  _incoming.readabilityHandler = ^(NSFileHandle *handle) {
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
  _incoming.readabilityHandler = ^(NSFileHandle *handle) {
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
  _outgoing.writeabilityHandler = ^(NSFileHandle *handle) {
    handle.writeabilityHandler = nil;

    NSError *error = nil;

    [_outgoing writeData:data error:&error];

    completion(error);
  };
}

- (void)write:(NSData *_Nonnull)data {
  _outgoing.writeabilityHandler = ^(NSFileHandle *handle) {
    handle.writeabilityHandler = nil;

    [_outgoing writeData:data];
  };
}

- (void)write:(NSString *_Nonnull)data encoding:(NSStringEncoding)encoding completion:(void (^_Nonnull)(NSError *_Nullable error))completion {
  [self write:[data dataUsingEncoding:encoding] completion:completion];
}

- (void)write:(NSString *_Nonnull)data encoding:(NSStringEncoding)encoding {
  [self write:[data dataUsingEncoding:encoding]];
}

- (void)close {
  [_incoming closeFile];
  [_outgoing closeFile];
}

@end

@implementation BareRPCIncomingRequest

- (_Nullable id)initWithRequest:(rpc_message_t *)request {
  self = [super init];

  if (self) {
    _id = [NSNumber numberWithUnsignedLong:request->id];
    _command = [NSString stringWithFormat:@"%.*s", (int) request->command.len, request->command.data];
  }

  return self;
}

@end

@implementation BareRPC {
  BareIPC *_ipc;
  BareRPCRequestHandler _requestHandler;
  BareRPCErrorHandler _errorHandler;
  NSData *_buffer;
}

- (_Nullable id)initWithIPC:(BareIPC *_Nonnull)ipc requestHandler:(BareRPCRequestHandler _Nonnull)requestHandler errorHandler:(BareRPCErrorHandler _Nonnull)errorHandler {
  self = [super init];

  if (self) {
    _ipc = ipc;
    _requestHandler = requestHandler;
    _errorHandler = errorHandler;
    _buffer = nil;

    [self _read];
  }

  return self;
}

- (void)_read {
  [_ipc read:^(NSData *data) {
    int err;

    if (data == nil) return;

    if (_buffer == nil) _buffer = data;
    else {
      NSMutableData *copy = [_buffer mutableCopy];
      [copy appendData:data];
      _buffer = copy;
    }

    while (_buffer != nil) {
      compact_state_t state = {
        .start = 0,
        .end = _buffer.length,
        .buffer = (uint8_t *) _buffer.bytes,
      };

      rpc_message_t message;
      err = rpc_decode_message(&state, &message);

      if (err == rpc_partial) break;
      else if (err < 0) {
        _errorHandler([NSError
          errorWithDomain:@"to.holepunch.bare.kit"
                     code:err
                 userInfo:@{NSLocalizedDescriptionKey : NSLocalizedString(@"RPC error", @"")}]);

        return;
      }

      switch (message.type) {
      case rpc_request: {
        _requestHandler([[BareRPCIncomingRequest alloc] initWithRequest:&message]);

        break;
      }

      case rpc_response: {
        break;
      }
      }

      _buffer = state.start == state.end ? nil : [_buffer subdataWithRange:NSMakeRange(state.start, state.end - state.start)];
    }

    [self _read];
  }];
}

@end
