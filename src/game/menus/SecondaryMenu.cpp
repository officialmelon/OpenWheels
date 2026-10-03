#include "SecondaryMenu.h"

#include "Globals.h"
#include "MainMenu.h"
#include "MenuHelper.h"

USING_NS_CC;

// @0060eabc
Scene* SecondaryMenu::createScene(bool popSceneOnExit)
{
    Scene* scene = Scene::create();
    SecondaryMenu* layer = SecondaryMenu::create();
    scene->addChild(layer);
    return scene;
}

// @0060eb64
SecondaryMenu::SecondaryMenu()
: _headerFontSize(120)
, _headerTopMargin(150.0f)
, _headerBottomMargin(150.0f)
{
    // _popSceneOnExit and _headerLabel are left uninitialised (as in the original).
    cocos2d::log("SecondaryMenu: constructor");
    _contentTop = 0.0f;
}

// @0060ebec (D1), @0060ec3c (D0)
SecondaryMenu::~SecondaryMenu()
{
    cocos2d::log("SecondaryMenu: destructor");
}

// @0060ec60
bool SecondaryMenu::init()
{
    // Layer::init() is not called.
    cocos2d::log("SecondaryMenu: init");

    MenuHelper::addBg(this, 0);
    MenuHelper::addOverlay(this, 1);
    MenuHelper::addBackBtn(this, 2, CC_CALLBACK_0(SecondaryMenu::backBtnPressed, this));

    addHeader();
    addContent();

    return true;
}

// @0060eda0
void SecondaryMenu::addHeader()
{
    if (_title.empty())
    {
        return;
    }

    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();

    _headerLabel = Label::createWithTTF(_title, "fonts/ClarendonLTStd-Bold.ttf", (float)_headerFontSize);
    _headerLabel->setAlignment(TextHAlignment::CENTER);
    _headerLabel->setColor(globals::colors::blue);
    _headerLabel->setAnchorPoint(Vec2(0.5f, 1.0f));
    _headerLabel->setPosition(Vec2(origin.x + visibleSize.width / 2, visibleSize.height - _headerTopMargin));
    addChild(_headerLabel, 3);

    // Underline bar below the header.
    Size labelSize = _headerLabel->getContentSize();
    Vec2 labelPosition = _headerLabel->getPosition();
    LayerColor* underline = LayerColor::create(Color4B(globals::colors::blue, 255), labelSize.width, 6.0f);
    underline->setPosition(Vec2(labelPosition.x - labelSize.width / 2, labelPosition.y - labelSize.height + 22));
    addChild(underline, 4);

    _contentTop = _headerLabel->getPosition().y - _headerLabel->getContentSize().height - _headerBottomMargin;
}

// @0060f030
void SecondaryMenu::setPopSceneOnExit(bool popSceneOnExit)
{
    _popSceneOnExit = popSceneOnExit;
}

// @0060f038
void SecondaryMenu::pushScene(Scene* scene)
{
    // `scene` is never used: the original pushes a freshly created SecondaryMenu scene.
    Director* director = Director::getInstance();
    director->pushScene(TransitionFade::create(globals::ui::menuFadeTime, SecondaryMenu::createScene(false),
                                               Color3B(0, 0, 0)));
}

// @0060f168
void SecondaryMenu::backBtnPressed()
{
    bool popSceneOnExit = _popSceneOnExit;
    Director* director = Director::getInstance();
    if (popSceneOnExit)
    {
        director->popScene();
    }
    else
    {
        director->pushScene(TransitionFade::create(globals::ui::menuFadeTime,
                                                   MainMenu::createScene(MenuModeMain, nullptr), Color3B(0, 0, 0)));
    }
}

// @0060f240
void SecondaryMenu::addContent()
{
}
