#import <UIKit/UIKit.h>
#import <stdatomic.h>

#import "../suspension.h"

@interface BareSuspensionTask : NSObject {
@public
  _Atomic UIBackgroundTaskIdentifier identifier;
}
@end

@implementation BareSuspensionTask
@end

int
bare_suspension_init(bare_suspension_t *suspension) {
  BareSuspensionTask *task = [[BareSuspensionTask alloc] init];

  atomic_init(&task->identifier, UIBackgroundTaskInvalid);

  suspension->task = task;

  return 0;
}

int
bare_suspension_start(bare_suspension_t *suspension, int timeout) {
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

  NSTimeInterval remaining = app.backgroundTimeRemaining;

  if (remaining == DBL_MAX) remaining = 30.0;

  remaining *= 1000;

  if (timeout < 0 || timeout > remaining) return remaining;

  return timeout;
}

int
bare_suspension_end(bare_suspension_t *suspension) {
  UIApplication *app = [UIApplication sharedApplication];

  if (app == nil) return 0;

  BareSuspensionTask *task = suspension->task;

  UIBackgroundTaskIdentifier prev = atomic_exchange(&task->identifier, UIBackgroundTaskInvalid);

  if (prev != UIBackgroundTaskInvalid) [app endBackgroundTask:prev];

  return 0;
}

void
bare_suspension_destroy(bare_suspension_t *suspension) {
  [(BareSuspensionTask *) suspension->task release];
}
