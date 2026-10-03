#include "PrivacyPolicyScene.h"

#include "AdController.h"
#include "Globals.h"
#include "HWWindow.h"
#include "MainMenu.h"
#include "Settings.h"

USING_NS_CC;

// TU-local static (_INIT_21 @0064d0fc).
static const std::string kPrivacyPolicyURL = "http://totaljerkface.com/mobile_terms_android.tjf";  // @00ac6690

// @0064caa4
Scene* PrivacyPolicyScene::createScene()
{
    Scene* scene = Scene::create();
    scene->addChild(PrivacyPolicyScene::create());
    return scene;
}

// @0064cb4c
PrivacyPolicyScene::PrivacyPolicyScene()
{
}

// @0064ce8c (D2), @0064cea8 (D0)
PrivacyPolicyScene::~PrivacyPolicyScene()
{
}

// @0064cb84
bool PrivacyPolicyScene::init()
{
    // The result of Layer::init() is ignored.
    Layer::init();
    addChild(LayerColor::create(Color4B::BLACK));
    return true;
}

// @0064cbc8
void PrivacyPolicyScene::onEnterTransitionDidFinish()
{
    Node::onEnterTransitionDidFinish();

    bool termsAccepted = UserDefault::getInstance()->getBoolForKey("terms_of_use_accepted");
    Settings* settings = Settings::getInstance();
    if (!termsAccepted)
    {
        HWWindow* window = settings->createWindow(HWWindowAppearanceAlert, this, false, false);
        window->setDismissUponButtonPress(false);
        window->showPrivacyPolicyMessage();
    }
    else
    {
        settings->getAdController()->init();
        Director::getInstance()->replaceScene(TransitionFade::create(
            globals::ui::menuFadeTime, MainMenu::createScene(MenuModeMain, nullptr), Color3B(0, 0, 0)));
    }
}

// @0064cce4
void PrivacyPolicyScene::initAds()
{
    Settings::getInstance()->getAdController()->init();
}

// @0064ccfc
void PrivacyPolicyScene::goToNextScene()
{
    Director::getInstance()->replaceScene(TransitionFade::create(
        globals::ui::menuFadeTime, MainMenu::createScene(MenuModeMain, nullptr), Color3B(0, 0, 0)));
}

// @0064cda0
Sprite* PrivacyPolicyScene::addFillBGWithFile(std::string file)
{
    Size winSize = Director::getInstance()->getWinSize();

    Texture2D::setDefaultAlphaPixelFormat(Texture2D::PixelFormat::RGBA8888);
    Sprite* sprite = Sprite::create(file);
    sprite->setScaleX(winSize.width / sprite->getTextureRect().size.width);
    sprite->setScaleY(winSize.height / sprite->getTextureRect().size.height);
    sprite->setPosition(winSize.width * 0.5f, winSize.height * 0.5f);
    addChild(sprite);
    Texture2D::setDefaultAlphaPixelFormat(Texture2D::PixelFormat::RGBA4444);

    return sprite;
}

// @0064cecc (non-virtual thunk @0064cf40)
void PrivacyPolicyScene::hwWindowButtonPressed(int buttonTag, HWWindow* window)
{
    if (buttonTag == 1)
    {
        // "ACCEPT"
        Settings::getInstance()->getAdController()->init();
        UserDefault::getInstance()->setBoolForKey("terms_of_use_accepted", true);
        window->dismissWindow(true);
    }
    else
    {
        // "Privacy Policy"
        Application::getInstance()->openURL(kPrivacyPolicyURL);
    }
}

// @0064cfb4 (non-virtual thunk @0064d058)
void PrivacyPolicyScene::hwWindowWasDismissed(HWWindow* window)
{
    goToNextScene();
}
