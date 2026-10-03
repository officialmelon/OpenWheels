// Android entry point, as in cocos2d-x 3.17.2 templates/cpp-template-default/proj.android/app/jni/
// hellocpp/main.cpp: Cocos2dxActivity loads libOpenWheels.so, whose JNI_OnLoad
// (javaactivity-android.cpp, linked whole-archive) calls cocos_android_app_init() to create the
// application; Cocos2dxRenderer.nativeInit later creates the GL view with the surface size and runs
// the Application - the same order the original
// libMyGame.so had, so the game's AppDelegate finds the GL view already set (frame height picks the
// large/medium/small/tiny tier exactly as on a device).
//
// Game files: the APK's assets/ is the FileUtils root ("assets/"), staged at build time from the
// player's own copy of the original (android/app/build.gradle):
//   assets/{shared,sounds,large,medium,small,tiny}/   the original Android asset tree
//   assets/soundlist.tsv, assets/gametext.tsv          generated tables (tools/re/extract_*.py)
//   assets/ios/                                        optional iOS bundle files (level editor art,
//                                                      Localizable.strings) - platform/common/IOSBundle
//
// Platform policy (no ads, no store, no consent gate, no network requirement):
//   * the Java side never starts an ad, billing, analytics or consent SDK (AppActivity.java);
//   * the original's first-run "terms of use / privacy policy" alert (PrivacyPolicyScene) only
//     existed for the ad and analytics SDKs, which OpenWheels does not have: it is pre-accepted here
//     so the game starts straight into the main menu. The game code itself is unchanged.

#include <jni.h>

#include <memory>

#include <android/log.h>

#include "cocos2d.h"
#include "AppDelegate.h"
#include "platform/common/IOSBundle.h"
#include "platform/common/Localization.h"

USING_NS_CC;

#define OW_LOG(...) __android_log_print(ANDROID_LOG_INFO, "OpenWheels", __VA_ARGS__)

namespace {

// Directory (relative to assets/) the build stages the iOS bundle files into.
const char* const kIOSBundleDir = "ios/";

// Platform set-up that needs FileUtils / UserDefault. cocos_android_app_init() runs inside
// JNI_OnLoad (System.loadLibrary), before Cocos2dxHelper.init has handed the asset manager and
// class loader to the native side, so this waits for applicationDidFinishLaunching (GL thread,
// first nativeInit) - the point where the Win32 main.cpp has done the same work.
void setUpPlatform()
{
    FileUtils* fileUtils = FileUtils::getInstance();
    if (!fileUtils->isFileExist("shared/Characters.plist"))
    {
        OW_LOG("game assets missing from the APK (assets/shared): build with tools/build_android.ps1");
    }
    if (!fileUtils->isFileExist("soundlist.tsv") || !fileUtils->isFileExist("gametext.tsv"))
    {
        OW_LOG("soundlist.tsv / gametext.tsv missing from the APK: sounds and long texts will be missing");
    }

    if (fileUtils->isFileExist(std::string(kIOSBundleDir) + "Localizable.strings"))
    {
        openwheels::setIOSBundlePath(kIOSBundleDir);
        OW_LOG("iOS bundle = assets/%s (%d localized strings)", kIOSBundleDir, (int)Localization::size());
    }
    else
    {
        OW_LOG("no iOS bundle files in the APK: the level editor is unavailable");
    }

    // ANDROID (port): no consent gate - see the header comment.
    UserDefault* userDefault = UserDefault::getInstance();
    if (!userDefault->getBoolForKey("terms_of_use_accepted"))
    {
        userDefault->setBoolForKey("terms_of_use_accepted", true);
        userDefault->flush();
    }
}

// The game's AppDelegate, with the platform set-up run first.
class AndroidAppDelegate : public AppDelegate
{
public:
    bool applicationDidFinishLaunching() override
    {
        setUpPlatform();
        return AppDelegate::applicationDidFinishLaunching();
    }
};

std::unique_ptr<AndroidAppDelegate> g_appDelegate;

}  // namespace

void cocos_android_app_init(JNIEnv* env)
{
    (void)env;
    OW_LOG("cocos_android_app_init");
    g_appDelegate.reset(new AndroidAppDelegate());
}

// AppActivity.nativeSurfaceResized (GL thread): the GL surface changed size without the activity
// being recreated (rotation between the two landscapes, fold/unfold, split screen, freeform
// window). cocos2d-x 3.17 only reads the size once, in nativeInit; re-apply the game's design
// resolution policy (3600x2000 FIXED_HEIGHT, set by AppDelegate) to the new frame so the scene is
// neither stretched nor cropped. The asset tier chosen at start-up is kept.
extern "C" JNIEXPORT void JNICALL
Java_org_openwheels_game_AppActivity_nativeSurfaceResized(JNIEnv* env, jclass clazz, jint width, jint height)
{
    (void)env;
    (void)clazz;
    Director* director = Director::getInstance();
    GLView* glview = director->getOpenGLView();
    if (!glview || width <= 0 || height <= 0)
    {
        return;  // not running yet: nativeInit will use the current size
    }
    const Size frame = glview->getFrameSize();
    if ((int)frame.width == width && (int)frame.height == height)
    {
        return;
    }
    const Size design = glview->getDesignResolutionSize();
    const ResolutionPolicy policy = glview->getResolutionPolicy();
    OW_LOG("surface resized %dx%d -> %dx%d", (int)frame.width, (int)frame.height, (int)width, (int)height);
    glview->setFrameSize((float)width, (float)height);  // also resets the design size to the frame
    if (design.width > 0.0f && design.height > 0.0f && policy != ResolutionPolicy::UNKNOWN)
    {
        // FIXED_HEIGHT recomputes the design width from the new aspect ratio and updates the
        // viewport / projection through the director.
        glview->setDesignResolutionSize(design.width, design.height, policy);
    }
    else
    {
        director->setViewport();
    }
}
