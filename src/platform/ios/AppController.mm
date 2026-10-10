// iOS entry (OpenWheels port; see docs/IOS.md). Like the Android platform layer
// (src/platform/android/main.cpp) the platform creates the GL view with the screen's pixel size
// before the app runs, so the game's AppDelegate picks the original's asset tier from it.
//
// Game files: the player's own Android assets (never shipped with OpenWheels). Two places work:
//   * OpenWheels.app/assets/  - staged into the bundle at build time (tools/build.sh --ios copies
//     binary/HappyWheels_Android/HW_Android/assets and the generated tables there);
//   * Documents/assets/       - copied onto the device later with Finder / iTunes file sharing
//     (UIFileSharingEnabled), so a sideloaded build can be filled without rebuilding.
// Optional iOS editor art: the bundle's (or Documents') ios/ folder, like assets/ios/ on Android.

#import "AppController.h"
#import "RootViewController.h"

#include "cocos2d.h"
#include "AppDelegate.h"
#include "platform/common/IOSBundle.h"
#include "platform/common/Localization.h"

USING_NS_CC;

namespace {

bool looksLikeAssets(const std::string& dir)
{
    FileUtils* fu = FileUtils::getInstance();
    return fu->isDirectoryExist(dir + "shared/levels") && fu->isDirectoryExist(dir + "sounds");
}

std::string documentsDirectory()
{
    NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString* dir = [paths firstObject];
    return dir ? std::string([dir UTF8String]) + "/" : std::string();
}

// The game's AppDelegate, with the platform set-up run first (FileUtils exists by then).
class IOSAppDelegate : public AppDelegate
{
public:
    bool applicationDidFinishLaunching() override
    {
        setUpPlatform();
        return AppDelegate::applicationDidFinishLaunching();
    }

private:
    static void setUpPlatform()
    {
        FileUtils* fileUtils = FileUtils::getInstance();
        const std::string bundle = std::string([[[NSBundle mainBundle] resourcePath] UTF8String]) + "/";
        const std::string documents = documentsDirectory();
        std::string root;
        for (const std::string& candidate : {bundle + "assets/", bundle + "Resources/assets/", documents + "assets/"})
        {
            if (looksLikeAssets(candidate))
            {
                root = candidate;
                break;
            }
        }
        if (root.empty())
        {
            MessageBox("OpenWheels needs the game files from your own copy of Happy Wheels (Android).\n\n"
                       "Copy the APK's assets folder into OpenWheels' Documents with Finder (Files tab), "
                       "or build with tools/build.sh --ios so it is bundled.",
                       "Game files not found");
        }
        else
        {
            fileUtils->setDefaultResourceRootPath(root);
        }
        // Generated tables (soundlist.tsv, gametext.tsv) and generated/ art: bundle root first,
        // then next to the assets.
        fileUtils->addSearchPath(bundle, false);
        if (!documents.empty()) fileUtils->addSearchPath(documents, false);
        for (const std::string& dir : {root + "ios/", bundle + "ios/", documents + "ios/"})
        {
            if (fileUtils->isFileExist(dir + "Localizable.strings"))
            {
                openwheels::setIOSBundlePath(dir);
                cocos2d::log("OpenWheels: iOS bundle = %s (%d localized strings)", dir.c_str(),
                             (int)Localization::size());
                break;
            }
        }
        // No consent gate (no ad / analytics SDKs), as on Android and PC.
        UserDefault* userDefault = UserDefault::getInstance();
        if (!userDefault->getBoolForKey("terms_of_use_accepted"))
        {
            userDefault->setBoolForKey("terms_of_use_accepted", true);
            userDefault->flush();
        }
    }
};

IOSAppDelegate* g_appDelegate = nullptr;

}  // namespace

@implementation AppController

@synthesize viewController = _viewController;
// UIApplicationDelegate declares the window property; protocol properties are not synthesized.
@synthesize window = _window;

- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
    if (!g_appDelegate) g_appDelegate = new IOSAppDelegate();
    cocos2d::Application* app = cocos2d::Application::getInstance();
    app->initGLContextAttrs();
    cocos2d::GLViewImpl::convertAttrs();

    _window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    _viewController = [[RootViewController alloc] init];
    [_window setRootViewController:_viewController];
    [_window makeKeyAndVisible];
    [[UIApplication sharedApplication] setStatusBarHidden:YES];
    [[UIApplication sharedApplication] setIdleTimerDisabled:YES];

    cocos2d::GLView* glview = cocos2d::GLViewImpl::createWithEAGLView((__bridge void*)_viewController.view);
    cocos2d::Director::getInstance()->setOpenGLView(glview);
    app->run();
    return YES;
}

// "Open in OpenWheels" for .happywheels / level files (AirDrop, Files, Mail), like the original
// iOS game's document handler.
- (BOOL)application:(UIApplication*)app openURL:(NSURL*)url options:(NSDictionary*)options
{
    if (![url isFileURL]) return NO;
    const std::string path = [[url path] UTF8String];
    cocos2d::Director::getInstance()->getScheduler()->performFunctionInCocosThread([path]() {
        extern void openwheelsOpenFile(const std::string& path);
        openwheelsOpenFile(path);
    });
    return YES;
}

- (void)applicationWillResignActive:(UIApplication*)application
{
}

- (void)applicationDidBecomeActive:(UIApplication*)application
{
}

- (void)applicationDidEnterBackground:(UIApplication*)application
{
    cocos2d::Application::getInstance()->applicationDidEnterBackground();
}

- (void)applicationWillEnterForeground:(UIApplication*)application
{
    cocos2d::Application::getInstance()->applicationWillEnterForeground();
}

@end
