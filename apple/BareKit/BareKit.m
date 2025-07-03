#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

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

@end

static void
bare_worklet__on_push(bare_worklet_push_t *req, const char *err, const uv_buf_t *reply) {
  @autoreleasepool {
    BareWorkletPushContext *context = (__bridge BareWorkletPushContext *) req->data;

    [context.payload release];

    NSError *error;

    if (err) {
      error = [NSError
        errorWithDomain:@"to.holepunch.bare.kit"
                   code:-1
               userInfo:@{NSLocalizedDescriptionKey : [NSString stringWithUTF8String:err]}];

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

static void
bare_worklet__on_finalize(bare_worklet_t *handle, const uv_buf_t *source, void *finalize_hint) {
  CFTypeRef ref = (__bridge CFTypeRef) finalize_hint;

  CFRelease(ref);
}

@implementation BareWorklet {
@public
  bare_worklet_t _worklet;
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
       source:(NSData *_Nullable)source
    arguments:(NSArray<NSString *> *_Nullable)arguments {
  int err;

  const char *_filename = [filename cStringUsingEncoding:NSUTF8StringEncoding];

  int argc = arguments == nil ? 0 : [arguments count];

  const char **argv = calloc(argc, sizeof(char *));

  for (int i = 0; i < argc; i++) {
    argv[i] = [arguments[i] UTF8String];
  }

  if (source == nil) {
    err = bare_worklet_start(&_worklet, _filename, nil, nil, nil, argc, argv);
    assert(err == 0);
  } else {
    CFTypeRef ref = (__bridge CFTypeRef) source;

    CFRetain(ref);

    uv_buf_t _source = uv_buf_init((char *) source.bytes, source.length);

    err = bare_worklet_start(&_worklet, _filename, &_source, bare_worklet__on_finalize, (void *) ref, argc, argv);
    assert(err == 0);
  }

  free(argv);
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
  NSString *path = [bundle pathForResource:name ofType:type];

  [self start:path source:[NSData dataWithContentsOfFile:path] arguments:arguments];
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
  NSString *path = [bundle pathForResource:name ofType:type inDirectory:subpath];

  [self start:path source:[NSData dataWithContentsOfFile:path] arguments:arguments];
}

- (void)suspend {
  [self suspendWithLinger:-1];
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

- (void)push:(NSData *_Nonnull)payload
       queue:(NSOperationQueue *_Nonnull)queue
  completion:(void (^_Nonnull)(NSData *_Nullable reply, NSError *_Nullable error))completion {
  BareWorkletPushContext *context = [[BareWorkletPushContext alloc]
    initWithHandler:completion
            payload:payload
              queue:queue];

  uv_buf_t buf = uv_buf_init((char *) payload.bytes, payload.length);

  int err;
  err = bare_worklet_push(&_worklet, &context->_req, &buf, bare_worklet__on_push);
  assert(err == 0);
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
    BareIPC *ipc = (__bridge BareIPC *) poll->data;

    [ipc poll:events];
  }
}

@implementation BareIPC {
  bare_ipc_t _ipc;
  bare_ipc_poll_t _poll;
}

- (_Nullable instancetype)initWithWorklet:(BareWorklet *_Nonnull)worklet {
  self = [super init];

  if (self) {
    int err;

    err = bare_ipc_init(&_ipc, &worklet->_worklet);
    assert(err == 0);

    err = bare_ipc_poll_init(&_poll, &_ipc);
    assert(err == 0);

    _poll.data = (__bridge void *) self;
  }

  return self;
}

- (void)setReadable:(void (^)(BareIPC *_Nonnull))readable {
  if (readable == nil) {
    _readable = nil;
  } else {
    _readable = [readable copy];
  }

  [self update];
}

- (void)setWritable:(void (^)(BareIPC *_Nonnull))writable {
  if (writable == nil) {
    _writable = nil;

  } else {
    _writable = [writable copy];
  }

  [self update];
}

- (void)update {
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

- (void)poll:(int)events {
  if (events & bare_ipc_readable) _readable(self);
  if (events & bare_ipc_writable) _writable(self);
}

- (NSData *_Nullable)read {
  int err;

  void *data;
  size_t len;
  err = bare_ipc_read(&_ipc, &data, &len);
  assert(err == 0 || err == bare_ipc_would_block);

  if (err == bare_ipc_would_block) {
    return nil;
  }

  return [[NSData alloc] initWithBytes:data length:len];
}

- (void)read:(void (^_Nonnull)(NSData *_Nullable data, NSError *_Nullable error))completion {
  NSData *data = [self read];

  if (data) {
    completion(data, nil);
  } else {
    self.readable = ^(BareIPC *ipc) {
      NSData *data = [self read];

      if (data) {
        self.readable = nil;

        completion(data, nil);
      }
    };
  }
}

- (NSInteger)write:(NSData *_Nonnull)data {
  int err;
  err = bare_ipc_write(&_ipc, data.bytes, data.length);
  assert(err >= 0 || err == bare_ipc_would_block);

  if (err == bare_ipc_would_block) {
    return 0;
  }

  return err;
}

- (void)write:(NSData *_Nonnull)data
   completion:(void (^_Nonnull)(NSError *_Nullable error))completion {
  NSInteger written = [self write:data];

  if (written == data.length) {
    completion(nil);
  } else {
    data = [data subdataWithRange:NSMakeRange(written, data.length - written)];

    [data retain];

    __block NSData *remaining = data;

    self.writable = ^(BareIPC *ipc) {
      NSInteger written = [self write:remaining];

      if (written == remaining.length) {
        [remaining release];

        self.writable = nil;

        completion(nil);
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
  bare_ipc_poll_destroy(&_poll);
  bare_ipc_destroy(&_ipc);
}

@end

@implementation BareNotificationService {
  BareWorklet *_worklet;
  void (^_contentHandler)(UNNotificationContent *_Nonnull);
}

- (_Nullable instancetype)initWithConfiguration:(BareWorkletConfiguration *_Nullable)options {
  self = [super init];

  if (self) {
    _delegate = self;
    _contentHandler = nil;

    [BareWorklet optimizeForMemory:YES];

    _worklet = [[BareWorklet alloc] initWithConfiguration:options];
  }

  return self;
}

- (_Nullable instancetype)initWithFilename:(NSString *_Nonnull)filename
                                    source:(NSData *_Nullable)source
                                 arguments:(NSArray<NSString *> *_Nullable)arguments
                             configuration:(BareWorkletConfiguration *_Nullable)options {
  self = [self initWithConfiguration:options];

  if (self) {
    [self start:filename source:source arguments:arguments];
  }

  return self;
}

- (_Nullable instancetype)initWithFilename:(NSString *_Nonnull)filename
                                    source:(NSString *_Nonnull)source
                                  encoding:(NSStringEncoding)encoding
                                 arguments:(NSArray<NSString *> *_Nullable)arguments
                             configuration:(BareWorkletConfiguration *_Nullable)options {
  self = [self initWithConfiguration:options];

  if (self) {
    [self start:filename source:source encoding:encoding arguments:arguments];
  }

  return self;
}

- (_Nullable instancetype)initWithResource:(NSString *_Nonnull)name
                                    ofType:(NSString *_Nonnull)type
                                 arguments:(NSArray<NSString *> *_Nullable)arguments
                             configuration:(BareWorkletConfiguration *_Nullable)options {
  self = [self initWithConfiguration:options];

  if (self) {
    [self start:name ofType:type arguments:arguments];
  }

  return self;
}

- (_Nullable instancetype)initWithResource:(NSString *_Nonnull)name
                                    ofType:(NSString *_Nonnull)type
                                  inBundle:(NSBundle *_Nonnull)bundle
                                 arguments:(NSArray<NSString *> *_Nullable)arguments
                             configuration:(BareWorkletConfiguration *_Nullable)options {
  self = [self initWithConfiguration:options];

  if (self) {
    [self start:name ofType:type inBundle:bundle arguments:arguments];
  }

  return self;
}

- (_Nullable instancetype)initWithResource:(NSString *_Nonnull)name
                                    ofType:(NSString *_Nonnull)type
                               inDirectory:(NSString *_Nonnull)subpath
                                 arguments:(NSArray<NSString *> *_Nullable)arguments
                             configuration:(BareWorkletConfiguration *_Nullable)options {
  self = [self initWithConfiguration:options];

  if (self) {
    [self start:name ofType:type inDirectory:subpath arguments:arguments];
  }

  return self;
}

- (_Nullable instancetype)initWithResource:(NSString *_Nonnull)name
                                    ofType:(NSString *_Nonnull)type
                               inDirectory:(NSString *_Nonnull)subpath
                                  inBundle:(NSBundle *_Nonnull)bundle
                                 arguments:(NSArray<NSString *> *_Nullable)arguments
                             configuration:(BareWorkletConfiguration *_Nullable)options {
  self = [self initWithConfiguration:options];

  if (self) {
    [self start:name ofType:type inDirectory:subpath inBundle:bundle arguments:arguments];
  }

  return self;
}

- (void)start:(NSString *_Nonnull)filename
       source:(NSData *_Nullable)source
    arguments:(NSArray<NSString *> *_Nullable)arguments {
  [_worklet start:filename source:source arguments:arguments];
}

- (void)start:(NSString *_Nonnull)filename
       source:(NSString *_Nonnull)source
     encoding:(NSStringEncoding)encoding
    arguments:(NSArray<NSString *> *_Nullable)arguments {
  [_worklet start:filename source:source encoding:encoding arguments:arguments];
}

- (void)start:(NSString *_Nonnull)name
       ofType:(NSString *_Nonnull)type
    arguments:(NSArray<NSString *> *_Nullable)arguments {
  [_worklet start:name ofType:type arguments:arguments];
}

- (void)start:(NSString *_Nonnull)name
       ofType:(NSString *_Nonnull)type
     inBundle:(NSBundle *_Nonnull)bundle
    arguments:(NSArray<NSString *> *_Nullable)arguments {
  [_worklet start:name ofType:type inBundle:bundle arguments:arguments];
}

- (void)start:(NSString *_Nonnull)name
       ofType:(NSString *_Nonnull)type
  inDirectory:(NSString *_Nonnull)subpath
    arguments:(NSArray<NSString *> *_Nullable)arguments {
  [_worklet start:name ofType:type inDirectory:subpath arguments:arguments];
}

- (void)start:(NSString *_Nonnull)name
       ofType:(NSString *_Nonnull)type
  inDirectory:(NSString *_Nonnull)subpath
     inBundle:(NSBundle *_Nonnull)bundle
    arguments:(NSArray<NSString *> *_Nullable)arguments {
  [_worklet start:name ofType:type inDirectory:subpath inBundle:bundle arguments:arguments];
}

- (void)complete:(UNNotificationContent *_Nonnull)content {
  @synchronized(self) {
    if (_worklet == nil) return;

    [_worklet terminate];

    _worklet = nil;

    if (_contentHandler) {
      _contentHandler(content);

      [_contentHandler release];
    }
  }
}

- (void)didReceiveNotificationRequest:(UNNotificationRequest *)request
                   withContentHandler:(void (^)(UNNotificationContent *_Nonnull))contentHandler {
  _contentHandler = [contentHandler copy];

  NSError *error;

  NSData *json = [NSJSONSerialization dataWithJSONObject:request.content.userInfo options:0 error:&error];

  assert(error == nil); // `userInfo` has already been validated

  [_worklet push:json
      completion:^(NSData *_Nullable reply, NSError *_Nullable error) {
        UNNotificationContent *content = [[UNNotificationContent alloc] init];

        if (reply) {
          NSError *error;

          id data = [NSJSONSerialization JSONObjectWithData:reply options:0 error:&error];

          if (data) {
            content = [_delegate workletDidReply:data];
          }
        }

        [self complete:content];
      }];
}

- (void)serviceExtensionTimeWillExpire {
  [self complete:[[UNNotificationContent alloc] init]];
}

- (UNNotificationContent *_Nonnull)workletDidReply:(NSDictionary *_Nonnull)reply {
  UNMutableNotificationContent *content = [[UNMutableNotificationContent alloc] init];

  // Primary content
  content.title = reply[@"title"];
  content.subtitle = reply[@"subtitle"];
  content.body = reply[@"body"];

  // Supplementary content
  content.userInfo = reply[@"userInfo"];

  // App behavior
  content.badge = reply[@"badge"];
  content.targetContentIdentifier = reply[@"targetContentIdentifier"];

  // System integration
  id sound = reply[@"sound"];

  if (sound) {
    id type = sound[@"type"];
    id critical = sound[@"critical"];
    id volume = sound[@"volume"];

    if ([type isEqualToString:@"default"]) {
      if (critical) {
        if (volume) {
          content.sound = [UNNotificationSound defaultCriticalSoundWithAudioVolume:[volume floatValue]];
        } else {
          content.sound = [UNNotificationSound defaultCriticalSound];
        }
      } else {
        content.sound = [UNNotificationSound defaultSound];
      }
    } else if ([type isEqualToString:@"named"]) {
      if (critical) {
        if (volume) {
          content.sound = [UNNotificationSound criticalSoundNamed:sound[@"name"] withAudioVolume:[volume floatValue]];
        } else {
          content.sound = [UNNotificationSound criticalSoundNamed:sound[@"name"]];
        }
      } else {
        content.sound = [UNNotificationSound soundNamed:sound[@"name"]];
      }
    }
  }

  // Grouping
  content.threadIdentifier = reply[@"threadIdentifier"];
  content.categoryIdentifier = reply[@"categoryIdentifier"];

  return content;
}

@end
