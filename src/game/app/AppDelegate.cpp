#include "AppDelegate.h"

#include "IAPController.h"
#include "PrivacyPolicyScene.h"
#include "Settings.h"
#include "qol/QoL.h"  // QOL (PC addition)
#include "input/Gamepad.h"  // PAD (PC addition)
#include "restored/Restored.h"  // RESTORED (PC addition)
#include "net/NetLevels.h"  // NET (PC addition)

#include "sdkbox/PluginReview.h"

USING_NS_CC;

// @0057ce04
AppDelegate::AppDelegate()
{
}

// @0057ce34 (D1), @0057ce38 (D0)
AppDelegate::~AppDelegate()
{
    net::stopLevelSharing();  // NET (PC addition): joins the network thread
}

// @0057ce5c
void AppDelegate::initGLContextAttrs()
{
    // red, green, blue, alpha, depth, stencil, multisamplesCount
    GLContextAttrs glContextAttrs = {8, 8, 8, 8, 24, 8, 0};
    GLView::setGLContextAttrs(glContextAttrs);
}

// @0057cebc
bool AppDelegate::applicationDidFinishLaunching()
{
    auto director = Director::getInstance();
    auto glview = director->getOpenGLView();
    if (!glview)
    {
        glview = GLViewImpl::create("hwcpp");
        director->setOpenGLView(glview);
    }

    UserDefault* userDefault = UserDefault::getInstance();
    bool useLowResGraphics = userDefault->getBoolForKey("use_low_res_graphics");

    director->setAnimationInterval(1.0f / 60);
    qol::installFrameRate(qol::frameRate());  // QOL (PC addition): 30 / 60 FPS (60 by default)
    openwheels::pad::install();  // PAD (PC addition): game controllers, haptics, tilt (src/input/)

    glview->setDesignResolutionSize(designResolutionSize.width, designResolutionSize.height,
                                    ResolutionPolicy::FIXED_HEIGHT);
    Size frameSize = glview->getFrameSize();
    // The result is not used.
    Rect visibleRect = glview->getVisibleRect();
    (void)visibleRect;

    FileUtils* fileUtils = FileUtils::getInstance();
    fileUtils->addSearchPath("shared");
    fileUtils->addSearchPath("sounds");

    std::string resolutionDirectory = "small";
    float resolutionHeight;
    if (frameSize.height > mediumResolutionSize.height)
    {
        if (!useLowResGraphics)
        {
            resolutionDirectory.assign("large");
            resolutionHeight = largeResolutionSize.height;
        }
        else
        {
            resolutionDirectory.assign("medium");
            resolutionHeight = mediumResolutionSize.height;
        }
    }
    else if (frameSize.height > smallResolutionSize.height && !useLowResGraphics)
    {
        resolutionDirectory.assign("medium");
        resolutionHeight = mediumResolutionSize.height;
    }
    else if (frameSize.height > smallResolutionSize.height
             || (frameSize.height > tinyResolutionSize.height && !useLowResGraphics))
    {
        resolutionDirectory.assign("small");
        resolutionHeight = smallResolutionSize.height;
    }
    else
    {
        resolutionDirectory.assign("tiny");
        resolutionHeight = tinyResolutionSize.height;
    }
    // QOL (PC addition): "textures" overrides the tier (auto by default = the choice above).
    qol::overrideAssetTier(&resolutionDirectory, &resolutionHeight);
    fileUtils->addSearchPath(resolutionDirectory);
    qol::setAssetTier(resolutionDirectory);  // QOL (PC addition): generated/<tier>/ sheets
    restored::addSearchPaths(resolutionDirectory);  // RESTORED (PC addition): browser characters
    director->setContentScaleFactor(resolutionHeight / designResolutionSize.height);

    Settings::getInstance()->getIAPController()->init();
    // PC: sdkbox stub (no store rating prompt).
    sdkbox::PluginReview::init();

    userDefault->setIntegerForKey("mascot_state", 0);

    director->runWithScene(PrivacyPolicyScene::createScene());
    net::startLevelSharing();  // NET (PC addition): receive levels from nearby players (src/net/)
    return true;
}

// @0057d1f0
void AppDelegate::applicationDidEnterBackground()
{
    IAPController* iapController = Settings::getInstance()->getIAPController();
    if (iapController)
    {
        iapController->applicationDidEnterBackground();
    }
    Director::getInstance()->stopAnimation();
}

// @0057d214
void AppDelegate::applicationWillEnterForeground()
{
    Director::getInstance()->startAnimation();
    IAPController* iapController = Settings::getInstance()->getIAPController();
    if (iapController)
    {
        iapController->applicationWillEnterForeground();
    }
}
