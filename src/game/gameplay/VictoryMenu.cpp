#include "VictoryMenu.h"

#include "Gameplay.h"
#include "LevelB2D.h"
#include "Session.h"
#include "Settings.h"
#include "VictoryAnimation.h"

USING_NS_CC;

// @00640b44
VictoryMenu::VictoryMenu()
{
}

// @00640b90 (D1), @00640ba4 (D0)
VictoryMenu::~VictoryMenu()
{
}

// @00640bc8
bool VictoryMenu::init(float time, int placement, Size bannerSize)
{
    _placement = placement;
    _time = time;
    _bannerSize = bannerSize;
    addMenu();
    return true;
}

// @00640c04
void VictoryMenu::addBg()
{
}

// @00640c08
void VictoryMenu::addMenu()
{
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("menus/victory/menu_victory.plist");
    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();
    Settings* settings = Settings::getInstance();
    SpriteFrameCache* cache = SpriteFrameCache::getInstance();
    if (!cache->isSpriteFramesWithFileLoaded("menus/pause/menu_pause.plist"))
    {
        cache->addSpriteFramesWithFile("menus/pause/menu_pause.plist");
    }

    Sprite* panel = Sprite::createWithSpriteFrameName("popUp_scale9.png");
    panel->setAnchorPoint(Vec2::ANCHOR_MIDDLE);
    panel->setCenterRectNormalized(Rect(0.266666681f, 0.266666681f, 0.466666698f, 0.466666698f));
    addChild(panel);

    Vector<MenuItem*> items;
    MenuItemImage* exitBtn = btnWithIcon("menu_pause_icon_exit.png", 0, GameplayMenuActionExit);
    MenuItemImage* resetBtn = btnWithIcon("menu_pause_icon_reset.png", 1, GameplayMenuActionReset);
    float padding = exitBtn->getContentSize().width / 3.0f;
    float btnWidth = exitBtn->getContentSize().width;
    items.pushBack(exitBtn);
    items.pushBack(resetBtn);
    if (!settings->getCurrentSession()->getLevel()->getForcedChar())
    {
        items.pushBack(btnWithIcon("menu_pause_icon_character.png", 1,
                                   GameplayMenuActionChangeCharacter));
    }
    items.pushBack(btnWithIcon("menu_pause_icon_replay.png", 1, GameplayMenuActionViewReplay));
    int chapter = Settings::getInstance()->getSelectedChapter();
    if (chapter != 5000 && chapter != 5001)
    {
        items.pushBack(btnWithIcon("menu_pause_icon_next.png", 0, GameplayMenuActionNextLevel));
    }

    float panelHeight = padding * 2.0f + btnWidth + 36.0f;
    Vec2 menuPosition(origin.x + visibleSize.width * 0.5f,
                      panelHeight * 0.5f + (visibleSize.height - _bannerSize.height - 596.0f -
                                            panelHeight - 18.0f) * 0.5f);
    _menu = Menu::createWithArray(items);
    _menu->alignItemsHorizontallyWithPadding(padding);
    _menu->setPositionY(menuPosition.y);
    _menu->setVisible(false);
    // both fetched but not used
    items.front()->getPosition();
    items.back()->getPosition();
    ssize_t count = items.size();
    float panelWidth = padding * 2.0f + (count * btnWidth + (count - 1) * padding) + 36.0f;
    panel->setContentSize(Size(panelWidth, panelHeight));
    panel->setPosition(menuPosition.x, panelHeight * -0.5f);
    addChild(_menu);

    FiniteTimeAction* delay = DelayTime::create(0.15f);
    FiniteTimeAction* intro = CallFunc::create(CC_CALLBACK_0(VictoryMenu::introComplete, this));
    panel->runAction(Sequence::create(
        delay, EaseExponentialOut::create(MoveTo::create(0.5f, menuPosition)), intro, nullptr));

    std::string placementText = "";
    if (chapter != 5001)
    {
        switch (_placement)
        {
        case 1:
            placementText = "your best time!";
            break;
        case 2:
            placementText = "your 2nd best time!";
            break;
        case 3:
            placementText = "your 3rd best time!";
            break;
        case 4:
            placementText = "your worst time!";
            break;
        }
    }
    VictoryAnimation* animation = new VictoryAnimation();
    animation->autorelease();
    animation->init(_time, placementText, panelWidth);
    animation->setPosition(Vec2(menuPosition.x, panelHeight * 0.5f + menuPosition.y + -18.0f));
    addChild(animation, -1);
}

// @00641534
MenuItemImage* VictoryMenu::btnWithIcon(std::string iconFrameName, int color, int tag)
{
    std::string normalFrame = "";
    std::string selectedFrame = "";
    switch (color)
    {
    case 0:
        normalFrame = "menu_pause_btn_pink_normal.png";
        selectedFrame = "menu_pause_btn_pink_down.png";
        break;
    case 1:
        normalFrame = "menu_pause_btn_blue_normal.png";
        selectedFrame = "menu_pause_btn_blue_down.png";
        break;
    }
    MenuItemImage* item =
        MenuItemImage::create("", "", CC_CALLBACK_1(VictoryMenu::btnPressed, this));
    Sprite* normalSprite = Sprite::createWithSpriteFrameName(normalFrame);
    Sprite* selectedSprite = Sprite::createWithSpriteFrameName(selectedFrame);
    Sprite* icon = Sprite::createWithSpriteFrameName(iconFrameName);
    icon->setAnchorPoint(Vec2(0.0f, 0.0f));
    item->setNormalImage(normalSprite);
    item->setSelectedImage(selectedSprite);
    item->setTag(tag);
    item->addChild(icon);
    return item;
}

// @00641938
void VictoryMenu::introComplete()
{
    _menu->setVisible(true);
}

// @006419b4
void VictoryMenu::btnPressed(Ref* sender)
{
    int action = ((Node*)sender)->getTag();
    if (action != GameplayMenuActionRemoveAds)
    {
        getEventDispatcher()->dispatchCustomEvent("gameplayMenuAction", &action);
    }
}
