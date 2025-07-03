#if defined(BARE_KIT_IOS)
#include <UIKit/UIKit.h>
#endif

#import "suspension.h"

int
bare_suspension_init(bare_suspension_t *suspension) {
  atomic_init(&suspension->task, UIBackgroundTaskInvalid);

  return 0;
}

int
bare_suspension_start(bare_suspension_t *suspension, int linger) {
#if defined(BARE_KIT_IOS)
  UIApplication *app = [UIApplication sharedApplication];

  if (app == nil) return MAX(linger, 0);

  UIBackgroundTaskIdentifier next = [app beginBackgroundTaskWithName:@"Suspending Bare"
                                                   expirationHandler:^{
                                                     [app endBackgroundTask:next];
                                                   }];

  UIBackgroundTaskIdentifier prev = atomic_exchange(&suspension->task, next);

  if (prev != UIBackgroundTaskInvalid) [app endBackgroundTask:prev];

  NSTimeInterval remaining = UIApplication.sharedApplication.backgroundTimeRemaining;

  if (remaining == DBL_MAX) remaining = 30.0;

  remaining *= 1000;

  if (linger < 0) return remaining;

  return MIN(linger, remaining);
#else
  return MAX(linger, 0);
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
