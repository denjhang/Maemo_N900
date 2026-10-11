#import <UIKit/UIKit.h>
#import "ScanViewController.h"

@interface AppDelegate : NSObject <UIApplicationDelegate>
@property (nonatomic, retain) UIWindow *window;
@property (nonatomic, retain) UINavigationController *navigationController;
@end

@implementation AppDelegate

@synthesize window;
@synthesize navigationController;

- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {

    window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];

    ScanViewController *scanVC = [[ScanViewController alloc] init];
    navigationController = [[UINavigationController alloc]
        initWithRootViewController:scanVC];
    [scanVC release];

    window.rootViewController = navigationController;
    [window makeKeyAndVisible];
    return YES;
}

- (void)dealloc {
    [navigationController release];
    [window release];
    [super dealloc];
}

@end

int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    int retVal = UIApplicationMain(argc, argv, nil, @"AppDelegate");
    [pool release];
    return retVal;
}
