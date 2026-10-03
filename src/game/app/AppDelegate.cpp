#include "AppDelegate.h"

#include "IAPController.h"
#include "PrivacyPolicyScene.h"
#include "Settings.h"

#include "sdkbox/PluginReview.h"

USING_NS_CC;

// @0057ce04
AppDelegate::AppDelegate()
{
}

// @0057ce34 (D1), @0057ce38 (D0)
AppDelegate::~AppDelegate()
{
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
    fileUtils->addSearchPath(resolutionDirectory);
    director->setContentScaleFactor(resolutionHeight / designResolutionSize.height);

    Settings::getInstance()->getIAPController()->init();
    // PC: sdkbox stub (no store rating prompt).
    sdkbox::PluginReview::init();

    userDefault->setIntegerForKey("mascot_state", 0);

    director->runWithScene(PrivacyPolicyScene::createScene());
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
