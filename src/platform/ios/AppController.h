// iOS application delegate (PC addition: OpenWheels' own iOS port of the Android reconstruction),
// as in cocos2d-x 3.17.2 templates/cpp-template-default/proj.ios_mac/ios/AppController.h.

#import <UIKit/UIKit.h>

@class RootViewController;

@interface AppController : NSObject <UIApplicationDelegate> {
}

@property(nonatomic, readonly) RootViewController* viewController;

@end
