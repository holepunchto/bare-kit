#ifndef BARE_KIT_H
#define BARE_KIT_H

#if defined(__OBJC__)

#import <Foundation/Foundation.h>

@interface BareWorklet : NSObject

@property(atomic, strong, readonly) NSFileHandle *incoming;
@property(atomic, strong, readonly) NSFileHandle *outgoing;

- (void)start:(NSString *)filename source:(NSData *)source;
- (void)suspend;
- (void)suspendWithLinger:(int)linger;
- (void)resume;
- (void)terminate;

@end

#endif

#endif // BARE_KIT_H
