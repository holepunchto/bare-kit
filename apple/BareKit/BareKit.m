#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#import <assert.h>
#import <string.h>
#import <utf.h>

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

  if (source == NULL) {
    err = bare_worklet_start(&_worklet, _filename, NULL, argc, argv);
    assert(err == 0);
  } else {
    uv_buf_t _source = uv_buf_init((char *) source.bytes, source.length);

    err = bare_worklet_start(&_worklet, _filename, &_source, argc, argv);
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
  bare_ipc_t _ipc;
  NSFileHandle *_file;
}

- (_Nullable instancetype)initWithWorklet:(BareWorklet *_Nonnull)worklet {
  self = [super init];

  if (self) {
    int err;

    err = bare_ipc_init(&_ipc, (const char *) worklet->_worklet.endpoint);
    assert(err == 0);

    _file = [[NSFileHandle alloc] initWithFileDescriptor:bare_ipc_fd(&_ipc)];
  }

  return self;
}

- (void)read:(void (^_Nonnull)(NSData *_Nullable data, NSError *_Nullable error))completion {
  int err;

  void *data;
  size_t len;

  bare_ipc_msg_t msg;
  err = bare_ipc_receive(&_ipc, &msg, &data, &len);

  if (err == 0) {
    NSData *data = [[NSData alloc] initWithBytes:data length:len];

    bare_ipc_release(&msg);

    return completion(data, nil);
  }

  bare_ipc_release(&msg);

  if (err != bare_ipc_would_block) {
    NSError *error = [NSError
      errorWithDomain:@"to.holepunch.bare.kit"
                 code:err
             userInfo:@{NSLocalizedDescriptionKey : @"IPC error"}];

    return completion(nil, error);
  }

  _file.readabilityHandler = ^(NSFileHandle *handle) {
    int err;

    handle.readabilityHandler = nil;

    void *data;
    size_t len;

    bare_ipc_msg_t msg;
    err = bare_ipc_receive(&_ipc, &msg, &data, &len);

    if (err == 0) {
      NSData *data = [[NSData alloc] initWithBytes:data length:len];

      bare_ipc_release(&msg);

      return completion(data, nil);
    }

    bare_ipc_release(&msg);

    NSError *error = [NSError
      errorWithDomain:@"to.holepunch.bare.kit"
                 code:err
             userInfo:@{NSLocalizedDescriptionKey : @"IPC error"}];

    return completion(nil, error);
  };
}

- (void)read:(NSStringEncoding)encoding
  completion:(void (^_Nonnull)(NSString *_Nullable data, NSError *_Nullable error))completion {
  [self read:^(NSData *_Nullable data, NSError *_Nullable error) {
    if (data == nil) {
      completion(nil, error);
    } else {
      completion([[NSString alloc] initWithData:data encoding:encoding], nil);
    }
  }];
}

- (void)write:(NSData *_Nonnull)data
   completion:(void (^_Nonnull)(NSError *_Nullable error))completion {
  int err;

  bare_ipc_msg_t msg;
  err = bare_ipc_send(&_ipc, &msg, data.bytes, data.length);

  bare_ipc_release(&msg);

  if (err == 0) return completion(nil);

  if (err != bare_ipc_would_block) {
    NSError *error = [NSError
      errorWithDomain:@"to.holepunch.bare.kit"
                 code:err
             userInfo:@{NSLocalizedDescriptionKey : @"IPC error"}];

    return completion(error);
  }

  _file.writeabilityHandler = ^(NSFileHandle *handle) {
    int err;

    handle.writeabilityHandler = nil;

    bare_ipc_msg_t msg;
    err = bare_ipc_send(&_ipc, &msg, data.bytes, data.length);

    bare_ipc_release(&msg);

    if (err == 0) return completion(nil);

    NSError *error = [NSError
      errorWithDomain:@"to.holepunch.bare.kit"
                 code:err
             userInfo:@{NSLocalizedDescriptionKey : @"IPC error"}];

    return completion(error);
  };
}

- (void)write:(NSString *_Nonnull)data
     encoding:(NSStringEncoding)encoding
   completion:(void (^_Nonnull)(NSError *_Nullable error))completion {
  [self write:[data dataUsingEncoding:encoding] completion:completion];
}

- (void)close {
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
