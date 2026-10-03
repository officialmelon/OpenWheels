#pragma once

#include "cocos2d.h"

// Resolution tiers. In the binary these five cocos2d::Size objects are dynamically initialised in
// every TU that includes this header (AppDelegate _INIT_4 @0057d240, MainMenu _INIT_13), so they are
// internal-linkage statics defined here, as in the cocos2d-x project template.
// The tier is chosen from the frame height (and the "use_low_res_graphics" option); its folder name is
// added as a search path and contentScaleFactor = tier.height / designResolutionSize.height.
static cocos2d::Size designResolutionSize = cocos2d::Size(3600.0f, 2000.0f);
static cocos2d::Size largeResolutionSize = cocos2d::Size(3600.0f, 2000.0f);   // "large"
static cocos2d::Size mediumResolutionSize = cocos2d::Size(1800.0f, 1000.0f);  // "medium"
static cocos2d::Size smallResolutionSize = cocos2d::Size(1350.0f, 750.0f);    // "small"
static cocos2d::Size tinyResolutionSize = cocos2d::Size(900.0f, 500.0f);      // "tiny"

// The cocos2d application. Private base, as in the cocos2d-x template (RTTI: __vmi_class_type_info
// with a non-public base). arm64 sizeof 8 (vptr only).
//
// applicationDidFinishLaunching: GLView "hwcpp", animation interval 1/60, design resolution
// 3600x2000 FIXED_HEIGHT, search paths "shared", "sounds" and the resolution tier
// (frame height > 1000: large, low-res option -> medium; > 750: medium / small; > 500: small / tiny;
// else tiny), IAPController::init(), sdkbox::PluginReview::init() (PC: dropped), UserDefault
// "mascot_state" = 0, then runWithScene(PrivacyPolicyScene::createScene()).
// The background/foreground hooks only stop/start the animation and notify the IAPController
// (audio is not paused here).
class AppDelegate : private cocos2d::Application
{
public:
    AppDelegate();                                                             // @0057ce04
    ~AppDelegate() override;                                                   // @0057ce34 (D1), @0057ce38 (D0)

    // vtable order (cocos2d::Application): applicationDidFinishLaunching (+0x10),
    // applicationDidEnterBackground (+0x18), applicationWillEnterForeground (+0x20),
    // initGLContextAttrs (+0x30).
    bool applicationDidFinishLaunching() override;                             // @0057cebc
    void applicationDidEnterBackground() override;                             // @0057d1f0
    void applicationWillEnterForeground() override;                            // @0057d214
    // GLContextAttrs {8, 8, 8, 8, 24, 8, 0}
    void initGLContextAttrs() override;                                        // @0057ce5c
};
