#if defined(BARE_KIT_IOS)
#include <UIKit/UIKit.h>
#endif

#import "suspension.h"

int
bare_suspension_init(bare_suspension_t *suspension) {
#if defined(BARE_KIT_IOS)
  atomic_init(&suspension->task, UIBackgroundTaskInvalid);
#endif

  return 0;
}

int
bare_suspension_start(bare_suspension_t *suspension, int timeout) {
#if defined(BARE_KIT_IOS)
  UIApplication *app = [UIApplication sharedApplication];

  if (app == nil) return timeout < 0 ? 30000 : timeout;

  UIBackgroundTaskIdentifier next = [app beginBackgroundTaskWithName:@"Suspending Bare"
                                                   expirationHandler:^{
                                                     [app endBackgroundTask:next];
                                                   }];

  UIBackgroundTaskIdentifier prev = atomic_exchange(&suspension->task, next);

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

  UIBackgroundTaskIdentifier next = UIBackgroundTaskInvalid;

  UIBackgroundTaskIdentifier prev = atomic_exchange(&suspension->task, next);

  if (prev != UIBackgroundTaskInvalid) [app endBackgroundTask:prev];

  return 0;
#else
  return 0;
#endif
}
