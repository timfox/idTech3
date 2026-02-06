#ifdef __APPLE__
#if defined(TARGET_OS_IPHONE)

#import <UIKit/UIKit.h>
#import "ios_platform.h"
#import "../../common/qcommon.h"

@interface IDTechAppDelegate : UIResponder <UIApplicationDelegate>

@property (strong, nonatomic) UIWindow* window;

@end

@implementation IDTechAppDelegate

- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
	[self.window makeKeyAndVisible];
	Platform_SetupIOS(self.window.rootViewController.view);
	Sys_Init_iOS();
	return YES;
}

- (void)applicationWillTerminate:(UIApplication*)application {
	Sys_Shutdown_iOS();
}

- (void)applicationDidReceiveMemoryWarning:(UIApplication*)application {
	Com_Printf("iOS: memory warning\n");
}

@end

#endif
#endif
