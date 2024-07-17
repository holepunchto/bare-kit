#import <Foundation/Foundation.h>

#import <assert.h>
#import <compact.h>
#import <rpc.h>
#import <string.h>
#import <utf.h>

#import "BareKit.h"

#import "../../shared/worklet.h"

typedef void (^BareWorkletPushHandler)(NSData *_Nullable reply, NSError *_Nullable error);

@interface BareWorkletPushContext : NSObject

@property(nonatomic, copy) BareWorkletPushHandler handler;
@property(nonatomic, strong) NSData *payload;
@property(nonatomic, strong) NSOperationQueue *queue;

- (id)initWithHandler:(BareWorkletPushHandler)handler payload:(NSData *)payload queue:(NSOperationQueue *)queue;

@end

@implementation BareWorkletPushContext {
@public
  bare_worklet_push_t _req;
}

- (id)initWithHandler:(BareWorkletPushHandler)handler payload:(NSData *)payload queue:(NSOperationQueue *)queue {
  self = [super init];

  if (self) {
    _handler = [handler copy];
    _payload = [payload retain];
    _queue = [queue retain];
    _req.data = (__bridge void *) self;
  }

  return self;
}

@end

static void
bare_worklet__on_push (bare_worklet_push_t *req, const char *err, const uv_buf_t *reply) {
  @autoreleasepool {
    BareWorkletPushContext *context = (__bridge BareWorkletPushContext *) req->data;

    [context.payload release];

    NSError *error;

    if (err) {
      error = [NSError
        errorWithDomain:@"to.holepunch.bare.kit"
                   code:-1
               userInfo:@{NSLocalizedDescriptionKey : @"Push error"}];

    } else {
      error = nil;
    }

    NSData *data;

    if (reply) {
      data = [NSData dataWithBytes:reply->base length:reply->len];
    } else {
      data = nil;
    }

    [context.queue addOperationWithBlock:^{
      context.handler(data, error);
    }];

    [context.queue release];
  }
}

@implementation BareWorklet {
  bare_worklet_t _worklet;
}

- (id)init {
  self = [super init];

  if (self) {
    int err;
    err = bare_worklet_init(&_worklet, NULL);
    assert(err == 0);
  }

  return self;
}

- (void)start:(NSString *_Nonnull)filename source:(NSData *_Nonnull)source {
  int err;

  const char *_filename = [filename cStringUsingEncoding:NSUTF8StringEncoding];

  uv_buf_t _source = uv_buf_init((char *) source.bytes, source.length);

  err = bare_worklet_start(&_worklet, _filename, &_source);
  assert(err == 0);

  _incoming = _worklet.incoming;
  _outgoing = _worklet.outgoing;
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

  bare_worklet_destroy(&_worklet);
}

- (void)push:(NSData *_Nonnull)payload queue:(NSOperationQueue *_Nonnull)queue completion:(void (^_Nonnull)(NSData *_Nullable reply, NSError *_Nullable error))completion {
  BareWorkletPushContext *context = [[BareWorkletPushContext alloc]
    initWithHandler:completion
            payload:payload
              queue:queue];

  uv_buf_t buf = uv_buf_init((char *) payload.bytes, payload.length);

  int err;
  err = bare_worklet_push(&_worklet, &context->_req, &buf, bare_worklet__on_push);
  assert(err == 0);
}

- (void)push:(NSData *_Nonnull)payload completion:(void (^_Nonnull)(NSData *_Nullable reply, NSError *_Nullable error))completion {
  [self push:payload queue:[NSOperationQueue mainQueue] completion:completion];
}

- (void)push:(NSString *_Nonnull)payload encoding:(NSStringEncoding)encoding queue:(NSOperationQueue *_Nonnull)queue completion:(void (^_Nonnull)(NSData *_Nullable reply, NSError *_Nullable error))completion {
  [self push:[payload dataUsingEncoding:encoding] queue:queue completion:completion];
}

- (void)push:(NSString *_Nonnull)payload encoding:(NSStringEncoding)encoding completion:(void (^_Nonnull)(NSData *_Nullable reply, NSError *_Nullable error))completion {
  [self push:[payload dataUsingEncoding:encoding] completion:completion];
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

@interface
BareRPC ()

- (void)_send:(BareRPCOutgoingRequest *_Nonnull)request data:(NSData *_Nonnull)data;
- (void)_reply:(BareRPCIncomingRequest *_Nonnull)request data:(NSData *_Nonnull)data;

@end

@implementation BareRPCIncomingRequest {
  BareRPC *_rpc;
}

- (_Nullable id)initWithRPC:(BareRPC *_Nonnull)rpc request:(rpc_message_t *)request {
  self = [super init];

  if (self) {
    _rpc = rpc;
    _id = [[NSNumber alloc] initWithUnsignedLong:request->id];
    _command = [[NSString alloc] initWithFormat:@"%.*s", (int) request->command.len, request->command.data];
    _data = [[NSData alloc] initWithBytes:request->data length:request->len];
  }

  return self;
}

- (NSString *_Nonnull)dataWithEncoding:(NSStringEncoding)encoding {
  return [[NSString alloc] initWithData:_data encoding:encoding];
}

- (void)reply:(NSData *_Nonnull)data {
  [_rpc _reply:self data:data];
}

- (void)reply:(NSString *_Nonnull)data encoding:(NSStringEncoding)encoding {
  [_rpc _reply:self data:[data dataUsingEncoding:encoding]];
}

@end

@implementation BareRPCOutgoingRequest {
  BareRPC *_rpc;
  BareRPCResponseHandler _responseHandler;
}

- (_Nullable id)initWithRPC:(BareRPC *_Nonnull)rpc command:(NSString *_Nonnull)command {
  self = [super init];

  if (self) {
    _rpc = rpc;
    _command = command;
    _responseHandler = nil;
  }

  return self;
}

- (void)send:(NSData *_Nonnull)data {
  [_rpc _send:self data:data];
}

- (void)send:(NSString *_Nonnull)data encoding:(NSStringEncoding)encoding {
  [_rpc _send:self data:[data dataUsingEncoding:encoding]];
}

- (void)reply:(void (^_Nonnull)(NSData *_Nullable data, NSError *_Nullable error))completion {
  _responseHandler = [completion copy];
}

- (void)reply:(NSStringEncoding)encoding completion:(void (^_Nonnull)(NSString *_Nonnull data, NSError *_Nullable error))completion {
  _responseHandler = [^(NSData *_Nullable data, NSError *_Nullable error) {
    completion(data == nil ? nil : [[NSString alloc] initWithData:data encoding:encoding], error);
  } copy];
}

- (void)_responseHandler:(NSData *_Nullable)data error:(NSError *_Nullable)error {
  if (_responseHandler) _responseHandler(data, error);
}

@end

@implementation BareRPC {
  BareIPC *_ipc;
  BareRPCRequestHandler _requestHandler;
  NSUInteger _id;
  NSMutableDictionary<NSNumber *, BareRPCOutgoingRequest *> *_requests;
  NSData *_buffer;
}

- (_Nullable id)initWithIPC:(BareIPC *_Nonnull)ipc requestHandler:(BareRPCRequestHandler _Nonnull)requestHandler {
  self = [super init];

  if (self) {
    _ipc = ipc;
    _requestHandler = [requestHandler copy];
    _id = 0;
    _requests = [[NSMutableDictionary alloc] initWithCapacity:16];
    _buffer = nil;

    [self _read];
  }

  return self;
}

- (BareRPCOutgoingRequest *_Nonnull)request:(NSString *_Nonnull)command {
  return [[BareRPCOutgoingRequest alloc] initWithRPC:self command:command];
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
        NSError *error = [NSError
          errorWithDomain:@"to.holepunch.bare.kit"
                     code:err
                 userInfo:@{NSLocalizedDescriptionKey : @"RPC error"}];

        _requestHandler(nil, error);

        return;
      }

      switch (message.type) {
      case rpc_request:
        [self _requestHandler:&message];
        break;
      case rpc_response:
        [self _responseHandler:&message];
        break;
      }

      _buffer = state.start == state.end ? nil : [_buffer subdataWithRange:NSMakeRange(state.start, state.end - state.start)];
    }

    [self _read];
  }];
}

- (void)_requestHandler:(rpc_message_t *)message {
  _requestHandler([[BareRPCIncomingRequest alloc] initWithRPC:self request:message], nil);
}

- (void)_responseHandler:(rpc_message_t *)message {
  if (message->id == 0) return;

  NSNumber *id = [NSNumber numberWithUnsignedInteger:message->id];

  BareRPCOutgoingRequest *request = _requests[id];

  if (request == nil) return;

  [_requests removeObjectForKey:id];

  if (message->error) {
    NSError *error = [NSError
      errorWithDomain:@"to.holepunch.bare.kit"
                 code:message->status
             userInfo:@{NSLocalizedDescriptionKey : [[NSString alloc] initWithFormat:@"%.*s", (int) message->message.len, message->message.data]}];

    [request _responseHandler:nil error:error];

  } else {
    [request _responseHandler:[[NSData alloc] initWithBytes:message->data length:message->len] error:nil];
  }
}

- (void)_send:(BareRPCOutgoingRequest *_Nonnull)request data:(NSData *_Nonnull)data {
  int err;

  NSNumber *id = [NSNumber numberWithUnsignedInteger:++_id];

  _requests[id] = request;

  const char *command = [request.command cStringUsingEncoding:NSUTF8StringEncoding];

  rpc_message_t message = {
    .type = rpc_request,
    .id = [id unsignedIntegerValue],
    .command = utf8_string_view_init((const utf8_t *) command, strlen(command)),
    .data = (uint8_t *) data.bytes,
    .len = data.length,
  };

  compact_state_t state = {0, 0, nil};

  err = rpc_preencode_message(&state, &message);
  assert(err == 0);

  NSMutableData *buffer = [NSMutableData dataWithLength:state.end];

  state.buffer = (uint8_t *) buffer.mutableBytes;

  err = rpc_encode_message(&state, &message);
  assert(err == 0);

  [_ipc write:buffer];
}

- (void)_reply:(BareRPCIncomingRequest *_Nonnull)request data:(NSData *_Nonnull)data {
  int err;

  rpc_message_t message = {
    .type = rpc_response,
    .id = [request.id unsignedIntegerValue],
    .error = false,
    .data = (uint8_t *) data.bytes,
    .len = data.length,
  };

  compact_state_t state = {0, 0, nil};

  err = rpc_preencode_message(&state, &message);
  assert(err == 0);

  NSMutableData *buffer = [NSMutableData dataWithLength:state.end];

  state.buffer = (uint8_t *) buffer.mutableBytes;

  err = rpc_encode_message(&state, &message);
  assert(err == 0);

  [_ipc write:buffer];
}

@end
