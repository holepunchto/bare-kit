#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
#endif

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

@interface BareWorklet ()

- (int)beginSuspensionTask:(int)linger;
- (void)endSuspensionTask;

@end

static void
bare_worklet__on_finalize(bare_worklet_t *handle, const uv_buf_t *source, void *finalize_hint) {
  CFTypeRef ref = (__bridge CFTypeRef) finalize_hint;

  CFRelease(ref);
}

static void
bare_worklet__on_idle(bare_worklet_t *handle) {
  @autoreleasepool {
    BareWorklet *worklet = (__bridge BareWorklet *) handle->data;

    [worklet endSuspensionTask];
  }
}

@implementation BareWorklet {
@public
  bare_worklet_t _worklet;

#if TARGET_OS_IOS
  UIBackgroundTaskIdentifier _suspending;
#endif
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

#if TARGET_OS_IOS
    _suspending = UIBackgroundTaskInvalid;
#endif
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
    err = bare_worklet_start(&_worklet, _filename, NULL, NULL, NULL, argc, argv);
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

#if TARGET_OS_IOS

- (int)beginSuspensionTask:(int)linger {
  @synchronized(self) {
    if (_suspending != UIBackgroundTaskInvalid) [self endSuspensionTask];

    UIApplication *app = [UIApplication sharedApplication];

    if (app == nil) return linger;

    _suspending = [app beginBackgroundTaskWithExpirationHandler:^{
      [self endSuspensionTask];
    }];

    NSTimeInterval remaining = UIApplication.sharedApplication.backgroundTimeRemaining;

    if (remaining == DBL_MAX) remaining = 30.0;

    remaining *= 1000;

    if (linger <= 0) return remaining;

    return MIN(linger, remaining);
  }
}

- (void)endSuspensionTask {
  @synchronized(self) {
    if (_suspending == UIBackgroundTaskInvalid) return;

    UIApplication *app = [UIApplication sharedApplication];

    if (app == nil) return;

    [app endBackgroundTask:_suspending];

    _suspending = UIBackgroundTaskInvalid;
  }
}

#else

- (int)beginSuspensionTask:(int)linger {
  return linger;
}

- (void)endSuspensionTask {
  ;
}

#endif

- (void)suspend {
  [self suspendWithLinger:0];
}

- (void)suspendWithLinger:(int)linger {
  linger = [self beginSuspensionTask:linger];

  int err;
  err = bare_worklet_suspend(&_worklet, linger, bare_worklet__on_idle);
  assert(err == 0);
}

- (void)resume {
  [self endSuspensionTask];

  int err;
  err = bare_worklet_resume(&_worklet);
  assert(err == 0);
}

- (void)terminate {
  [self endSuspensionTask];

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

@implementation BareIPC {
  bool _closed;
  bare_ipc_t _ipc;
  dispatch_queue_t _queue;
  dispatch_source_t _reader;
  dispatch_source_t _writer;
}

- (_Nullable instancetype)initWithWorklet:(BareWorklet *_Nonnull)worklet {
  self = [super init];

  if (self) {
    int err;

    _closed = false;

    err = bare_ipc_init(&_ipc, worklet->_worklet.incoming, worklet->_worklet.outgoing);
    assert(err == 0);

    _queue = dispatch_queue_create("to.holepunch.bare.kit.ipc", DISPATCH_QUEUE_SERIAL);

    _reader = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, _ipc.incoming, 0, _queue);

    _writer = dispatch_source_create(DISPATCH_SOURCE_TYPE_WRITE, _ipc.outgoing, 0, _queue);

    dispatch_source_set_event_handler(_reader, ^{
      if (_closed) return;

      @autoreleasepool {
        _readable(self);
      }
    });

    dispatch_source_set_event_handler(_writer, ^{
      if (_closed) return;

      @autoreleasepool {
        _writable(self);
      }
    });

    dispatch_source_set_cancel_handler(_reader, ^{
      dispatch_release(_reader);
    });

    dispatch_source_set_cancel_handler(_writer, ^{
      dispatch_release(_writer);
    });
  }

  return self;
}

- (void)setReadable:(void (^)(BareIPC *_Nonnull))readable {
  if (_closed) return;

  if (readable == nil) {
    if (_readable != nil) dispatch_suspend(_reader);

    _readable = nil;
  } else {
    if (_readable == nil) dispatch_resume(_reader);

    _readable = [readable copy];
  }
}

- (void)setWritable:(void (^)(BareIPC *_Nonnull))writable {
  if (_closed) return;

  if (writable == nil) {
    if (_writable != nil) dispatch_suspend(_writer);

    _writable = nil;

  } else {
    if (_writable == nil) dispatch_resume(_writer);

    _writable = [writable copy];
  }
}

- (NSData *_Nullable)read {
  void *data;
  size_t len;

  int err = bare_ipc_read(&_ipc, &data, &len);
  assert(err == 0 || err == bare_ipc_would_block);

  if (err == bare_ipc_would_block) {
    return nil;
  }

  return [[NSData alloc] initWithBytes:data length:len];
}

- (NSString *_Nullable)read:(NSStringEncoding)encoding {
  void *data;
  size_t len;

  int err = bare_ipc_read(&_ipc, &data, &len);
  assert(err == 0 || err == bare_ipc_would_block);

  if (err == bare_ipc_would_block) {
    return nil;
  }

  return [[NSString alloc] initWithBytes:data length:len encoding:encoding];
}

- (BOOL)write:(NSData *_Nonnull)data {
  int err;

  err = bare_ipc_write(&_ipc, data.bytes, data.length);
  assert(err == 0 || err == bare_ipc_would_block);

  return err == 0;
}

- (BOOL)write:(NSString *_Nonnull)data
     encoding:(NSStringEncoding)encoding {
  return [self write:[data dataUsingEncoding:encoding]];
}

- (void)close {
  _closed = true;

  if (_readable == nil) dispatch_resume(_reader);

  if (_writable == nil) dispatch_resume(_writer);

  dispatch_source_cancel(_reader);

  dispatch_source_cancel(_writer);

  dispatch_release(_queue);

  bare_ipc_destroy(&_ipc);
}

@end

@implementation BareNotificationService {
  BareWorklet *_worklet;
}

- (_Nullable instancetype)initWithConfiguration:(BareWorkletConfiguration *_Nullable)options {
  self = [super init];

  if (self) {
    _delegate = self;

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

- (void)didReceiveNotificationRequest:(UNNotificationRequest *)request
                   withContentHandler:(void (^)(UNNotificationContent *_Nonnull))contentHandler {
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

        contentHandler(content);
      }];
}

- (void)serviceExtensionTimeWillExpire {
  [_worklet suspend];
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
