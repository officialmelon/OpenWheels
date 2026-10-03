#include "CreditsLayer.h"

#include "Credits.h"
#include "Globals.h"
#include "MainMenu.h"
#include "MenuHelper.h"

USING_NS_CC;

// @005a82e0
CreditsLayer::CreditsLayer()
{
    // _showEnding is left uninitialised (set in init()).
}

// @005a8310 (D1), @005a8324 (D0)
CreditsLayer::~CreditsLayer()
{
}

// @005a8348
Scene* CreditsLayer::createScene(bool showEnding)
{
    Scene* scene = Scene::create();
    scene->addChild(CreditsLayer::create(showEnding));
    return scene;
}

// @005a83f8
CreditsLayer* CreditsLayer::create(bool showEnding)
{
    CreditsLayer* layer = new (std::nothrow) CreditsLayer();
    if (layer)
    {
        if (layer->init(showEnding))
        {
            layer->autorelease();
        }
        else
        {
            delete layer;
            layer = nullptr;
        }
    }
    return layer;
}

// @005a8484
bool CreditsLayer::init(bool showEnding)
{
    if (!Layer::init())
    {
        return false;
    }

    _showEnding = showEnding;

    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();  // unused

    MenuHelper::addBg(this, 0);
    MenuHelper::addOverlay(this, 1);
    MenuHelper::addBackBtn(this, 2, CC_CALLBACK_0(CreditsLayer::backBtnPressed, this));

    addChild(LayerColor::create(Color4B(0, 0, 0, 100), visibleSize.width, visibleSize.height));

    return true;
}

// @005a8604
void CreditsLayer::backBtnPressed()
{
    Director::getInstance()->replaceScene(TransitionFade::create(
        globals::ui::menuFadeTime, MainMenu::createScene(MenuModeMain, nullptr), Color3B(0, 0, 0)));
}

// @005a86a8
void CreditsLayer::onEnterTransitionDidFinish()
{
    Node::onEnterTransitionDidFinish();
    addChild(Credits::create(_showEnding), 2);
}
