#import <BareKit/BareKit.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow *window;
@end

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    UIViewController *rootViewController = [[UIViewController alloc] init];
    self.window.rootViewController = rootViewController;
    [self.window makeKeyAndVisible];

    UNUserNotificationCenter *center = [UNUserNotificationCenter currentNotificationCenter];
    [center requestAuthorizationWithOptions:(UNAuthorizationOptionAlert | UNAuthorizationOptionSound | UNAuthorizationOptionBadge)
                          completionHandler:^(BOOL granted, NSError * _Nullable error) {
        if (granted) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [[UIApplication sharedApplication] registerForRemoteNotifications];
            });
        }
    }];

    // Run the worklet test
    BareWorklet *worklet = [[BareWorklet alloc] initWithConfiguration:nil];
    NSString *source = @"BareKit.IPC.on('data', (data) => BareKit.IPC.write(data)).write('Hello!')";

    [worklet start:@"/app.js" source:[source dataUsingEncoding:NSUTF8StringEncoding] arguments:@[]];

    BareIPC *ipc = [[BareIPC alloc] initWithWorklet:worklet];

    // Rest of the IPC test code remains the same
    ipc.readable = ^(BareIPC *ipc) {
        NSString *data = [ipc read:NSUTF8StringEncoding];
        if (data == nil) return;

        ipc.readable = nil;
        NSLog(@"%@", data);
    };

    return YES;
}

- (void)application:(UIApplication *)application didRegisterForRemoteNotificationsWithDeviceToken:(NSData *)deviceToken {
    NSString *tokenString = [[deviceToken description] stringByReplacingOccurrencesOfString:@"<" withString:@""];
    tokenString = [tokenString stringByReplacingOccurrencesOfString:@">" withString:@""];
    tokenString = [tokenString stringByReplacingOccurrencesOfString:@" " withString:@""];
    NSLog(@"✅ APNs Device Token: %@", tokenString);
}

- (void)application:(UIApplication *)application didFailToRegisterForRemoteNotificationsWithError:(NSError *)error {
    NSLog(@"❌ Failed to register for remote notifications: %@", error.localizedDescription);
}


@end

int main(int argc, char * argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    }
} 

