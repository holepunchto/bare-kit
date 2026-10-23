#import <Foundation/Foundation.h>
#import <objc/runtime.h>

#import <assert.h>
#import <string.h>
#import <utf.h>
#import <uv.h>

#import "BareKit.h"

#import "../../shared/ipc.h"
#import "../../shared/worklet.h"

typedef void (^BareWorkletPushHandler)(NSData *_Nullable reply, NSError *_Nullable error);

@interface BareWorkletPushContext : NSObject

@property(nonatomic, copy) BareWorkletPushHandler handler;
@property(nonatomic, strong) NSData *payload;
@property(nonatomic, strong) NSOperationQueue *queue;

- (_Nullable instancetype)initWithHandler:(BareWorkletPushHandler)handler
                                  payload:(NSData *)payload
                                    queue:(NSOperationQueue *)queue;

@end

@implementation BareWorkletPushContext {
@public
  bare_worklet_push_t _req;
}

- (_Nullable instancetype)initWithHandler:(BareWorkletPushHandler)handler
                                  payload:(NSData *)payload
                                    queue:(NSOperationQueue *)queue {
  self = [super init];

  if (self) {
    _handler = [handler copy];
    _payload = [payload retain];
    _queue = [queue retain];
    _req.data = (__bridge void *) self;
  }

  return self;
}

- (void)dealloc {
  [_handler release];
  [_payload release];
  [_queue release];

  [super dealloc];
}

@end

static NSError *
bare_kit__error(NSString *message) {
  return [NSError
    errorWithDomain:@"to.holepunch.bare.kit"
               code:-1
           userInfo:@{NSLocalizedDescriptionKey : message}];
}

static void
bare_worklet__on_push(bare_worklet_push_t *req, const char *err, const uv_buf_t *reply) {
  @autoreleasepool {
    // Owned by the request since the push and released here rather than by the
    // operation below, as a queue releases its finished operations at no set
    // time.
    BareWorkletPushContext *context = (__bridge BareWorkletPushContext *) req->data;

    NSError *error;

    if (err) {
      error = bare_kit__error([NSString stringWithUTF8String:err]);

    } else {
      error = nil;
    }

    NSData *data;

    if (reply) {
      data = [NSData dataWithBytes:reply->base length:reply->len];
    } else {
      data = nil;
    }

    BareWorkletPushHandler handler = context.handler;

    [context.queue addOperationWithBlock:^{
      handler(data, error);
    }];

    [context release];
  }
}

@implementation BareWorkletConfiguration

+ (BareWorkletConfiguration *_Nullable)defaultWorkletConfiguration {
  return [[BareWorkletConfiguration alloc] init];
}

- (_Nullable instancetype)init {
  self = [super init];

  if (self) {
    _memoryLimit = 0;
    _assets = nil;
  }

  return self;
}

@end

@implementation BareWorklet {
@public
  bare_worklet_t _worklet;

  bool _terminated;
}

+ (void)optimizeForMemory:(BOOL)enabled {
  int err;
  err = bare_worklet_optimize_for_memory(enabled);
  assert(err == 0);
}

- (_Nullable instancetype)initWithConfiguration:(BareWorkletConfiguration *_Nullable)options {
  self = [super init];

  if (self) {
    bare_worklet_options_t _options;

    if (options) {
      _options.memory_limit = options.memoryLimit;
      _options.assets = options.assets == nil ? nil : [options.assets UTF8String];
    } else {
      _options.memory_limit = 0;
      _options.assets = nil;
    }

    int err;
    err = bare_worklet_init(&_worklet, &_options);
    assert(err == 0);

    _worklet.data = (__bridge void *) self;
  }

  return self;
}

- (void)start:(NSString *_Nonnull)filename
    arguments:(NSArray<NSString *> *_Nullable)arguments {
  [self start:filename source:nil arguments:arguments];
}

- (void)start:(NSString *_Nonnull)filename
       source:(NSData *_Nullable)source
    arguments:(NSArray<NSString *> *_Nullable)arguments {
  const char *_filename = [filename cStringUsingEncoding:NSUTF8StringEncoding];

  int argc = arguments == nil ? 0 : [arguments count];

  const char **argv = calloc(argc, sizeof(char *));

  for (int i = 0; i < argc; i++) {
    argv[i] = [arguments[i] UTF8String];
  }

  uv_buf_t _source;

  if (source) _source = uv_buf_init((char *) source.bytes, source.length);

  int err;

  @synchronized(self) {
    err = _terminated ? -1 : bare_worklet_start(&_worklet, _filename, source ? &_source : NULL, argc, argv);
  }

  free(argv);

  if (err < 0) [NSException raise:NSInternalInconsistencyException format:@"Worklet could not be started"];
}

- (void)start:(NSString *_Nonnull)filename
       source:(NSString *_Nonnull)source
     encoding:(NSStringEncoding)encoding
    arguments:(NSArray<NSString *> *_Nullable)arguments {
  [self start:filename source:[source dataUsingEncoding:encoding] arguments:arguments];
}
- (void)start:(NSString *_Nonnull)name
       ofType:(NSString *_Nonnull)type
    arguments:(NSArray<NSString *> *_Nullable)arguments {
  [self start:name ofType:type inBundle:[NSBundle mainBundle] arguments:arguments];
}

- (void)start:(NSString *_Nonnull)name
       ofType:(NSString *_Nonnull)type
     inBundle:(NSBundle *_Nonnull)bundle
    arguments:(NSArray<NSString *> *_Nullable)arguments {
  [self start:[bundle pathForResource:name ofType:type] arguments:arguments];
}

- (void)start:(NSString *_Nonnull)name
       ofType:(NSString *_Nonnull)type
  inDirectory:(NSString *_Nonnull)subpath
    arguments:(NSArray<NSString *> *_Nullable)arguments {
  [self start:name ofType:type inDirectory:subpath inBundle:[NSBundle mainBundle] arguments:arguments];
}

- (void)start:(NSString *_Nonnull)name
       ofType:(NSString *_Nonnull)type
  inDirectory:(NSString *_Nonnull)subpath
     inBundle:(NSBundle *_Nonnull)bundle
    arguments:(NSArray<NSString *> *_Nullable)arguments {
  [self start:[bundle pathForResource:name ofType:type inDirectory:subpath] arguments:arguments];
}

- (void)suspend {
  [self suspendWithLinger:-1];
}

// The lifecycle methods are serialized, as terminating destroys the worklet the
// others use.
- (void)suspendWithLinger:(int)linger {
  @synchronized(self) {
    if (_terminated) return;

    int err;
    err = bare_worklet_suspend(&_worklet, linger);
    assert(err == 0);
  }
}

- (void)resume {
  @synchronized(self) {
    if (_terminated) return;

    int err;
    err = bare_worklet_resume(&_worklet);
    assert(err == 0);
  }
}

- (void)terminate {
  @synchronized(self) {
    if (_terminated) return;

    _terminated = true;

    int err;
    err = bare_worklet_terminate(&_worklet);
    assert(err == 0);

    bare_worklet_destroy(&_worklet);
  }
}

- (void)dealloc {
  [self terminate];

  [super dealloc];
}

- (void)push:(NSData *_Nonnull)payload
       queue:(NSOperationQueue *_Nonnull)queue
  completion:(void (^_Nonnull)(NSData *_Nullable reply, NSError *_Nullable error))completion {
  BareWorkletPushContext *context = [[BareWorkletPushContext alloc]
    initWithHandler:completion
            payload:payload
              queue:queue];

  uv_buf_t buf = uv_buf_init((char *) payload.bytes, payload.length);

  int err;

  @synchronized(self) {
    err = _terminated ? -1 : bare_worklet_push(&_worklet, &context->_req, &buf, bare_worklet__on_push);
  }

  if (err < 0) bare_worklet__on_push(&context->_req, "Worklet is not running", NULL);
}

- (void)push:(NSData *_Nonnull)payload
  completion:(void (^_Nonnull)(NSData *_Nullable reply, NSError *_Nullable error))completion {
  [self push:payload queue:[NSOperationQueue mainQueue] completion:completion];
}

- (void)push:(NSString *_Nonnull)payload
    encoding:(NSStringEncoding)encoding
       queue:(NSOperationQueue *_Nonnull)queue
  completion:(void (^_Nonnull)(NSString *_Nullable reply, NSError *_Nullable error))completion {
  [self push:[payload dataUsingEncoding:encoding]
         queue:queue
    completion:^(NSData *reply, NSError *error) {
      completion(reply == nil ? nil : [[NSString alloc] initWithData:reply encoding:encoding], error);
    }];
}

- (void)push:(NSString *_Nonnull)payload
    encoding:(NSStringEncoding)encoding
  completion:(void (^_Nonnull)(NSString *_Nullable reply, NSError *_Nullable error))completion {
  [self push:payload encoding:encoding queue:[NSOperationQueue mainQueue] completion:completion];
}

@end

@interface BareIPC ()

- (void)poll:(int)events;

@end

static void
bare_ipc__on_poll(bare_ipc_poll_t *poll, int events) {
  @autoreleasepool {
    // Nil once the ipc has begun deallocating. Otherwise held until the pool
    // drains, so dealloc never runs, or waits, while a handler does.
    BareIPC *ipc = objc_loadWeak((id *) poll->data);

    [ipc poll:events];
  }
}

@implementation BareIPC {
  // Retained, as an IPC is meaningless without its worklet. Nil if init failed,
  // in which case there is nothing to destroy.
  BareWorklet *_worklet;

  bare_ipc_t _ipc;
  bare_ipc_poll_t _poll;

  id _weak;
}

- (_Nullable instancetype)initWithWorklet:(BareWorklet *_Nonnull)worklet {
  self = [super init];

  if (self) {
    int err;

    @synchronized(worklet) {
      err = worklet->_terminated ? -1 : bare_ipc_init(&_ipc, &worklet->_worklet);
    }

    if (err < 0) {
      [self release];

      return nil;
    }

    _worklet = [worklet retain];

    err = bare_ipc_poll_init(&_poll, &_ipc);
    assert(err == 0);

    objc_storeWeak(&_weak, self);

    _poll.data = &_weak;
  }

  return self;
}

// The handlers run on the poll queue while the setters may run anywhere, so
// they are swapped and taken under a lock, and only released outside it.
- (void)setReadable:(void (^)(BareIPC *_Nonnull))readable {
  readable = [readable copy];

  @synchronized(self) {
    void (^previous)(BareIPC *) = _readable;
    _readable = readable;
    readable = previous;
  }

  [readable release];

  [self update];
}

- (void)setWritable:(void (^)(BareIPC *_Nonnull))writable {
  writable = [writable copy];

  @synchronized(self) {
    void (^previous)(BareIPC *) = _writable;
    _writable = writable;
    writable = previous;
  }

  [writable release];

  [self update];
}

// Under the lock throughout, so that concurrent updates apply in order.
- (void)update {
  @synchronized(self) {
    int events = 0;

    if (_readable) events |= bare_ipc_readable;
    if (_writable) events |= bare_ipc_writable;

    int err;

    if (events) {
      err = bare_ipc_poll_start(&_poll, events, bare_ipc__on_poll);
      assert(err == 0);
    } else {
      err = bare_ipc_poll_stop(&_poll);
      assert(err == 0);
    }
  }
}

- (void)poll:(int)events {
  if (events & bare_ipc_readable) [self invoke:bare_ipc_readable];
  if (events & bare_ipc_writable) [self invoke:bare_ipc_writable];
}

// Taken afresh for each event, as the previous handler may have changed it.
- (void)invoke:(int)event {
  void (^handler)(BareIPC *);

  @synchronized(self) {
    handler = [(event == bare_ipc_readable ? _readable : _writable) retain];
  }

  if (handler) handler(self);

  [handler release];
}

- (NSData *_Nullable)read {
  NSMutableData *data = [NSMutableData dataWithLength:BARE_IPC_READ_BUFFER_SIZE];

  int len = bare_ipc_read(&_ipc, data.mutableBytes, data.length);
  assert(len >= 0 || len == bare_ipc_would_block);

  if (len == bare_ipc_would_block) {
    return nil;
  }

  data.length = len;

  return data;
}

- (void)read:(void (^_Nonnull)(NSData *_Nullable data, NSError *_Nullable error))completion {
  NSData *data = [self read];

  if (data == nil) {
    self.readable = ^(BareIPC *ipc) {
      self.readable = nil;

      [self read:completion];
    };

    return;
  }

  // A successful read of zero bytes is end-of-stream; signal it as nil rather
  // than an empty buffer so consumers can detect a closed IPC.
  completion(data.length == 0 ? nil : data, nil);
}

- (NSInteger)write:(NSData *_Nonnull)data {
  int err;
  err = bare_ipc_write(&_ipc, data.bytes, data.length);

  if (err == bare_ipc_would_block) return 0;
  if (err < 0) return -1;

  return err;
}

- (void)write:(NSData *_Nonnull)data
   completion:(void (^_Nonnull)(NSError *_Nullable error))completion {
  NSInteger written = [self write:data];

  if (written < 0) {
    completion(bare_kit__error(@"IPC closed"));
  } else if (written == data.length) {
    completion(nil);
  } else {
    data = [data subdataWithRange:NSMakeRange(written, data.length - written)];

    [data retain];

    __block NSData *remaining = data;

    self.writable = ^(BareIPC *ipc) {
      NSInteger written = [self write:remaining];

      if (written < 0 || written == remaining.length) {
        [remaining release];

        self.writable = nil;

        completion(written < 0 ? bare_kit__error(@"IPC closed") : nil);
      } else {
        NSData *data = [remaining subdataWithRange:NSMakeRange(written, remaining.length - written)];

        [data retain];
        [remaining release];

        remaining = data;
      }
    };
  }
}

- (void)close {
  bare_ipc_close(&_ipc);
}

- (void)dealloc {
  if (_worklet) {
    bare_ipc_poll_destroy(&_poll);
    bare_ipc_destroy(&_ipc);

    [_worklet release];
  }

  [_readable release];
  [_writable release];

  objc_storeWeak(&_weak, nil);

  [super dealloc];
}

@end
