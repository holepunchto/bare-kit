#ifndef BARE_KIT_H
#define BARE_KIT_H

#if defined(__OBJC__)

#import <Foundation/Foundation.h>

@interface BareWorklet : NSObject

@property(atomic, readonly) int incoming;
@property(atomic, readonly) int outgoing;

- (void)start:(NSString *_Nonnull)filename source:(NSData *_Nonnull)source;
- (void)suspend;
- (void)suspendWithLinger:(int)linger;
- (void)resume;
- (void)terminate;

@end

@interface BareIPC : NSObject

- (_Nullable id)initWithWorklet:(BareWorklet *_Nonnull)worklet;
- (void)read:(void (^_Nonnull)(NSData *_Nullable data))completion;
- (void)read:(NSStringEncoding)encoding completion:(void (^_Nonnull)(NSString *_Nullable data))completion;
- (void)write:(NSData *_Nonnull)data;
- (void)write:(NSData *_Nonnull)data completion:(void (^_Nonnull)(NSError *_Nullable error))completion;
- (void)write:(NSString *_Nonnull)data encoding:(NSStringEncoding)encoding completion:(void (^_Nonnull)(NSError *_Nullable error))completion;
- (void)write:(NSString *_Nonnull)data encoding:(NSStringEncoding)encoding;
- (void)close;

@end

#endif

#endif // BARE_KIT_H
