#ifndef BARE_KIT_H
#define BARE_KIT_H

#ifdef __OBJC__

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

@interface BareWorkletConfiguration : NSObject

@property NSUInteger memoryLimit;
@property(nullable, copy) NSString *assets;

+ (BareWorkletConfiguration *_Nullable)defaultWorkletConfiguration;

@end

@interface BareWorklet : NSObject

+ (void)optimizeForMemory:(BOOL)enabled;

- (_Nullable instancetype)initWithConfiguration:(BareWorkletConfiguration *_Nullable)options;

- (void)start:(NSString *_Nonnull)filename
       source:(NSData *_Nullable)source
    arguments:(NSArray<NSString *> *_Nullable)arguments;

- (void)start:(NSString *_Nonnull)filename
       source:(NSString *_Nonnull)source
     encoding:(NSStringEncoding)encoding
    arguments:(NSArray<NSString *> *_Nullable)arguments;

- (void)start:(NSString *_Nonnull)name
       ofType:(NSString *_Nonnull)type
    arguments:(NSArray<NSString *> *_Nullable)arguments;

- (void)start:(NSString *_Nonnull)name
       ofType:(NSString *_Nonnull)type
     inBundle:(NSBundle *_Nonnull)bundle
    arguments:(NSArray<NSString *> *_Nullable)arguments;

- (void)start:(NSString *_Nonnull)name
       ofType:(NSString *_Nonnull)type
  inDirectory:(NSString *_Nonnull)subpath
    arguments:(NSArray<NSString *> *_Nullable)arguments;

- (void)start:(NSString *_Nonnull)name
       ofType:(NSString *_Nonnull)type
  inDirectory:(NSString *_Nonnull)subpath
     inBundle:(NSBundle *_Nonnull)bundle
    arguments:(NSArray<NSString *> *_Nullable)arguments;

- (void)suspend;
- (void)suspendWithLinger:(int)linger;
- (void)resume;
- (void)terminate;

- (void)push:(NSData *_Nonnull)payload
       queue:(NSOperationQueue *_Nonnull)queue
  completion:(void (^_Nonnull)(NSData *_Nullable reply, NSError *_Nullable error))completion;

- (void)push:(NSData *_Nonnull)payload
  completion:(void (^_Nonnull)(NSData *_Nullable reply, NSError *_Nullable error))completion;

- (void)push:(NSString *_Nonnull)payload
    encoding:(NSStringEncoding)encoding
       queue:(NSOperationQueue *_Nonnull)queue
  completion:(void (^_Nonnull)(NSString *_Nullable reply, NSError *_Nullable error))completion;

- (void)push:(NSString *_Nonnull)payload
    encoding:(NSStringEncoding)encoding
  completion:(void (^_Nonnull)(NSString *_Nullable reply, NSError *_Nullable error))completion;

@end

@interface BareIPC : NSObject

@property(nonatomic, copy, nullable) void (^readable)(BareIPC *_Nonnull);
@property(nonatomic, copy, nullable) void (^writable)(BareIPC *_Nonnull);

- (_Nullable instancetype)initWithWorklet:(BareWorklet *_Nonnull)worklet;

- (NSData *_Nullable)read;

- (void)read:(void (^_Nonnull)(NSData *_Nullable data, NSError *_Nullable error))completion;

- (NSInteger)write:(NSData *_Nonnull)data;

- (void)write:(NSData *_Nonnull)data
   completion:(void (^_Nonnull)(NSError *_Nullable error))completion;

- (void)close;

@end

@protocol BareNotificationServiceDelegate

- (UNNotificationContent *_Nonnull)workletDidReply:(NSDictionary *_Nonnull)reply;

@end

@interface BareNotificationService : UNNotificationServiceExtension <BareNotificationServiceDelegate>

@property(nonatomic, assign, nonnull) id<BareNotificationServiceDelegate> delegate;

- (_Nullable instancetype)initWithConfiguration:(BareWorkletConfiguration *_Nullable)options;

- (_Nullable instancetype)initWithFilename:(NSString *_Nonnull)filename
                                    source:(NSData *_Nullable)source
                                 arguments:(NSArray<NSString *> *_Nullable)arguments
                             configuration:(BareWorkletConfiguration *_Nullable)options;

- (_Nullable instancetype)initWithFilename:(NSString *_Nonnull)filename
                                    source:(NSString *_Nonnull)source
                                  encoding:(NSStringEncoding)encoding
                                 arguments:(NSArray<NSString *> *_Nullable)arguments
                             configuration:(BareWorkletConfiguration *_Nullable)options;

- (_Nullable instancetype)initWithResource:(NSString *_Nonnull)name
                                    ofType:(NSString *_Nonnull)type
                                 arguments:(NSArray<NSString *> *_Nullable)arguments
                             configuration:(BareWorkletConfiguration *_Nullable)options;

- (_Nullable instancetype)initWithResource:(NSString *_Nonnull)name
                                    ofType:(NSString *_Nonnull)type
                                  inBundle:(NSBundle *_Nonnull)bundle
                                 arguments:(NSArray<NSString *> *_Nullable)arguments
                             configuration:(BareWorkletConfiguration *_Nullable)options;

- (_Nullable instancetype)initWithResource:(NSString *_Nonnull)name
                                    ofType:(NSString *_Nonnull)type
                               inDirectory:(NSString *_Nonnull)subpath
                                 arguments:(NSArray<NSString *> *_Nullable)arguments
                             configuration:(BareWorkletConfiguration *_Nullable)options;

- (_Nullable instancetype)initWithResource:(NSString *_Nonnull)name
                                    ofType:(NSString *_Nonnull)type
                               inDirectory:(NSString *_Nonnull)subpath
                                  inBundle:(NSBundle *_Nonnull)bundle
                                 arguments:(NSArray<NSString *> *_Nullable)arguments
                             configuration:(BareWorkletConfiguration *_Nullable)options;

- (void)start:(NSString *_Nonnull)filename
       source:(NSData *_Nullable)source
    arguments:(NSArray<NSString *> *_Nullable)arguments;

- (void)start:(NSString *_Nonnull)filename
       source:(NSString *_Nonnull)source
     encoding:(NSStringEncoding)encoding
    arguments:(NSArray<NSString *> *_Nullable)arguments;

- (void)start:(NSString *_Nonnull)name
       ofType:(NSString *_Nonnull)type
    arguments:(NSArray<NSString *> *_Nullable)arguments;

- (void)start:(NSString *_Nonnull)name
       ofType:(NSString *_Nonnull)type
     inBundle:(NSBundle *_Nonnull)bundle
    arguments:(NSArray<NSString *> *_Nullable)arguments;

- (void)start:(NSString *_Nonnull)name
       ofType:(NSString *_Nonnull)type
  inDirectory:(NSString *_Nonnull)subpath
    arguments:(NSArray<NSString *> *_Nullable)arguments;

- (void)start:(NSString *_Nonnull)name
       ofType:(NSString *_Nonnull)type
  inDirectory:(NSString *_Nonnull)subpath
     inBundle:(NSBundle *_Nonnull)bundle
    arguments:(NSArray<NSString *> *_Nullable)arguments;

@end

#endif

#endif // BARE_KIT_H
