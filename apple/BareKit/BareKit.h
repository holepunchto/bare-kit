#ifndef BARE_KIT_H
#define BARE_KIT_H

#if defined(__OBJC__)

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

@interface BareWorklet : NSObject

@property(atomic, readonly) int incoming;
@property(atomic, readonly) int outgoing;

+ (void)optimizeForMemory:(BOOL)enabled;

- (void)start:(NSString *_Nonnull)filename;
- (void)start:(NSString *_Nonnull)filename
       source:(NSData *_Nonnull)source;
- (void)start:(NSString *_Nonnull)filename
       source:(NSString *_Nonnull)source
     encoding:(NSStringEncoding)encoding;
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

- (_Nullable instancetype)initWithWorklet:(BareWorklet *_Nonnull)worklet;
- (void)read:(void (^_Nonnull)(NSData *_Nullable data))completion;
- (void)read:(NSStringEncoding)encoding
  completion:(void (^_Nonnull)(NSString *_Nullable data))completion;
- (void)write:(NSData *_Nonnull)data;
- (void)write:(NSData *_Nonnull)data
   completion:(void (^_Nonnull)(NSError *_Nullable error))completion;
- (void)write:(NSString *_Nonnull)data
     encoding:(NSStringEncoding)encoding
   completion:(void (^_Nonnull)(NSError *_Nullable error))completion;
- (void)write:(NSString *_Nonnull)data
     encoding:(NSStringEncoding)encoding;
- (void)close;

@end

@interface BareRPCIncomingRequest : NSObject

@property(atomic, readonly, nonnull) NSNumber *id;
@property(atomic, readonly, nonnull) NSString *command;
@property(atomic, readonly, nonnull) NSData *data;

- (NSString *_Nonnull)dataWithEncoding:(NSStringEncoding)encoding;
- (void)reply:(NSData *_Nonnull)data;
- (void)reply:(NSString *_Nonnull)data
     encoding:(NSStringEncoding)encoding;

@end

@interface BareRPCOutgoingRequest : NSObject

@property(atomic, readonly, nonnull) NSString *command;

- (void)send:(NSData *_Nonnull)data;
- (void)send:(NSString *_Nonnull)data
    encoding:(NSStringEncoding)encoding;
- (void)reply:(void (^_Nonnull)(NSData *_Nullable data, NSError *_Nullable error))completion;
- (void)reply:(NSStringEncoding)encoding
   completion:(void (^_Nonnull)(NSString *_Nullable data, NSError *_Nullable error))completion;

@end

typedef void (^BareRPCRequestHandler)(BareRPCIncomingRequest *_Nullable request, NSError *_Nullable error);
typedef void (^BareRPCResponseHandler)(NSData *_Nullable data, NSError *_Nullable error);

@interface BareRPC : NSObject

- (_Nullable instancetype)initWithIPC:(BareIPC *_Nonnull)ipc
                       requestHandler:(BareRPCRequestHandler _Nonnull)requestHandler;
- (BareRPCOutgoingRequest *_Nonnull)request:(NSString *_Nonnull)command;

@end

@interface BareKitNotificationService : UNNotificationServiceExtension

- (_Nullable instancetype)initWithFilename:(NSString *_Nonnull)filename
                                    source:(NSData *_Nonnull)source;
- (_Nullable instancetype)initWithFilename:(NSString *_Nonnull)filename
                                    source:(NSString *_Nonnull)source
                                  encoding:(NSStringEncoding)encoding;
- (_Nullable instancetype)initWithResource:(NSString *_Nonnull)name
                                    ofType:(NSString *_Nonnull)type
                                  inBundle:(NSBundle *_Nonnull)bundle;
- (_Nullable instancetype)initWithResource:(NSString *_Nonnull)name
                                    ofType:(NSString *_Nonnull)type
                               inDirectory:(NSString *_Nonnull)subpath
                                  inBundle:(NSBundle *_Nonnull)bundle;

@end

#endif

#endif // BARE_KIT_H
