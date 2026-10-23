#ifndef BARE_KIT_H
#define BARE_KIT_H

#ifdef __OBJC__

#import <Foundation/Foundation.h>

@interface BareWorkletConfiguration : NSObject

@property NSUInteger memoryLimit;
@property(nullable, copy) NSString *assets;

+ (BareWorkletConfiguration *_Nullable)defaultWorkletConfiguration;

@end

@interface BareWorklet : NSObject

+ (void)optimizeForMemory:(BOOL)enabled;

- (_Nullable instancetype)initWithConfiguration:(BareWorkletConfiguration *_Nullable)options;

/**
 * Raises `NSInternalInconsistencyException` if the worklet has terminated or
 * could not be started.
 */
- (void)start:(NSString *_Nonnull)filename
    arguments:(NSArray<NSString *> *_Nullable)arguments;

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

/**
 * Returns `nil` if the worklet has terminated or already has an open ipc.
 */
- (_Nullable instancetype)initWithWorklet:(BareWorklet *_Nonnull)worklet;

/**
 * Reads whatever is available without blocking. Returns `nil` when nothing is
 * ready yet, and empty data at end-of-stream, once the worklet has closed its
 * end or this ipc is closed. Wait for `readable` before retrying a `nil`.
 */
- (NSData *_Nullable)read;

/**
 * Reads once something is available, waiting for `readable` as needed. Delivers
 * `nil` data at end-of-stream, once the worklet has closed its end or this ipc
 * is closed. A pending read keeps this ipc alive until it completes, which
 * closing does.
 */
- (void)read:(void (^_Nonnull)(NSData *_Nullable data, NSError *_Nullable error))completion;

/**
 * Writes whatever fits without blocking, returning the number of bytes taken,
 * or -1 once this ipc is closed or the worklet has exited.
 */
- (NSInteger)write:(NSData *_Nonnull)data;

/**
 * Writes all of the data, waiting for `writable` as needed. A pending write
 * keeps this ipc alive until it completes, which closing does.
 */
- (void)write:(NSData *_Nonnull)data
   completion:(void (^_Nonnull)(NSError *_Nullable error))completion;

/**
 * Closes this ipc, after which reads return end-of-stream and writes fail.
 * Closing twice is a no-op.
 */
- (void)close;

@end

#endif

#endif // BARE_KIT_H
