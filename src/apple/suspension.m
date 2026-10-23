#if defined(BARE_KIT_IOS)
#include <UIKit/UIKit.h>
#include <stdatomic.h>
#endif

#import "suspension.h"

#if defined(BARE_KIT_IOS)
@interface BareSuspensionTask : NSObject {
@public
  _Atomic UIBackgroundTaskIdentifier identifier;
}
@end

@implementation BareSuspensionTask
@end
#endif

int
bare_suspension_init(bare_suspension_t *suspension) {
#if defined(BARE_KIT_IOS)
  BareSuspensionTask *task = [[BareSuspensionTask alloc] init];

  atomic_init(&task->identifier, UIBackgroundTaskInvalid);

  suspension->task = task;
#endif

  return 0;
}

int
bare_suspension_start(bare_suspension_t *suspension, int timeout) {
#if defined(BARE_KIT_IOS)
  UIApplication *app = [UIApplication sharedApplication];

  if (app == nil) return timeout < 0 ? 30000 : timeout;

  BareSuspensionTask *task = suspension->task;

  __block UIBackgroundTaskIdentifier next = [app beginBackgroundTaskWithName:@"Suspending Bare"
                                                           expirationHandler:^{
                                                             UIBackgroundTaskIdentifier expected = next;

                                                             atomic_compare_exchange_strong(&task->identifier, &expected, UIBackgroundTaskInvalid);

                                                             [app endBackgroundTask:next];
                                                           }];

  UIBackgroundTaskIdentifier prev = atomic_exchange(&task->identifier, next);

  if (prev != UIBackgroundTaskInvalid) [app endBackgroundTask:prev];

  NSTimeInterval remaining = UIApplication.sharedApplication.backgroundTimeRemaining;

  if (remaining == DBL_MAX) remaining = 30.0;

  remaining *= 1000;

  if (timeout < 0 || timeout > remaining) return remaining;

  return timeout;
#else
  return timeout < 0 ? 30000 : timeout;
#endif
}

int
bare_suspension_end(bare_suspension_t *suspension) {
#if defined(BARE_KIT_IOS)
  UIApplication *app = [UIApplication sharedApplication];

  if (app == nil) return 0;

  BareSuspensionTask *task = suspension->task;

  UIBackgroundTaskIdentifier prev = atomic_exchange(&task->identifier, UIBackgroundTaskInvalid);

  if (prev != UIBackgroundTaskInvalid) [app endBackgroundTask:prev];

  return 0;
#else
  return 0;
#endif
}

void
bare_suspension_destroy(bare_suspension_t *suspension) {
#if defined(BARE_KIT_IOS)
  [(BareSuspensionTask *) suspension->task release];
#endif
}
