#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE

#import <UIKit/UIKit.h>
#import <Foundation/Foundation.h>
#import <GameController/GameController.h>
#import "../qcommon/qcommon.h"
#import "macos_platform.h"

@interface Quake3AppDelegate : UIResponder <UIApplicationDelegate>

@property (strong, nonatomic) UIWindow *window;
@property (strong, nonatomic) UIViewController *gameViewController;

@end

@implementation Quake3AppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
	// Create window
	self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
	self.window.backgroundColor = [UIColor blackColor];
	
	// Create game view controller
	self.gameViewController = [[UIViewController alloc] init];
	self.window.rootViewController = self.gameViewController;
	
	// Create Metal view for rendering
	UIView* gameView = (__bridge UIView*)Platform_CreateView(
		(int)[[UIScreen mainScreen] bounds].size.width,
		(int)[[UIScreen mainScreen] bounds].size.height
	);
	gameView.frame = self.window.bounds;
	gameView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
	[self.gameViewController.view addSubview:(__bridge UIView*)gameView];
	
	// Make window key and visible
	[self.window makeKeyAndVisible];
	
	// Delay engine initialization to allow UI to fully initialize
	// This pattern is used in q3ios to prevent UI blocking
	dispatch_async(dispatch_get_main_queue(), ^{
		dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
			// Initialize engine on background thread
			dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
				iOS_InitializeEngine((__bridge void*)gameView);
			});
		});
	});
	
	return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application {
	// Pause game when app goes to background
	Com_Printf("iOS: Application will resign active\n");
}

- (void)applicationDidEnterBackground:(UIApplication *)application {
	// Save state when app enters background
	Com_Printf("iOS: Application did enter background\n");
}

- (void)applicationWillEnterForeground:(UIApplication *)application {
	// Resume game when app comes to foreground
	Com_Printf("iOS: Application will enter foreground\n");
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
	// Resume game when app becomes active
	Com_Printf("iOS: Application did become active\n");
}

- (void)applicationWillTerminate:(UIApplication *)application {
	// Cleanup when app terminates
	extern void iOS_ShutdownEngine(void);
	iOS_ShutdownEngine();
}

- (void)applicationDidReceiveMemoryWarning:(UIApplication *)application {
	// Handle memory warnings
	extern void iOS_HandleMemoryWarning(void);
	iOS_HandleMemoryWarning();
}

@end

// Main entry point for iOS
int main(int argc, char *argv[]) {
	@autoreleasepool {
		return UIApplicationMain(argc, argv, nil, NSStringFromClass([Quake3AppDelegate class]));
	}
}

#endif // TARGET_OS_IPHONE

