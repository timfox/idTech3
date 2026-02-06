#ifdef __APPLE__
#if defined(TARGET_OS_IPHONE)

#import <UIKit/UIKit.h>
#import "ios_appdelegate.mm"

int main(int argc, char* argv[]) {
	@autoreleasepool {
		return UIApplicationMain(argc, argv, nil, NSStringFromClass([IDTechAppDelegate class]));
	}
}

#endif
#endif
