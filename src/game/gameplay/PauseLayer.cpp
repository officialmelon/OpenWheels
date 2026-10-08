#include "PauseLayer.h"

#include <new>

#include "AdController.h"
#include "GameText.h"
#include "Gameplay.h"
#include "HWWindow.h"
#include "IAPController.h"
#include "LevelB2D.h"
#include "OptionsMenu.h"
#include "Session.h"
#include "Settings.h"
#include "Tracker.h"
#include "qol/CharacterChoice.h"  // QOL (PC addition)

USING_NS_CC;

// _INIT_16: Tracker category of the pause menu.
static std::string s_trackerCategory = "pause_menu";  // @00ac6500

// @005fe3c8
PauseLayer::PauseLayer()
{
}

// @005fe400 (D1), @005fe404 (D0)
PauseLayer::~PauseLayer()
{
}

// @005fe428
bool PauseLayer::init()
{
    bool result = Layer::init();
    if (result)
    {
        _waitingForStoreResponse = false;
        addMenu();
        scheduleUpdate();
    }
    return result;
}

// @005fe468
void PauseLayer::addMenu()
{
    SpriteFrameCache* cache = SpriteFrameCache::getInstance();
    if (!cache->isSpriteFramesWithFileLoaded("menus/pause/menu_pause.plist"))
    {
        cache->addSpriteFramesWithFile("menus/pause/menu_pause.plist");
    }
    bool adsRemoved = Settings::getInstance()->getAdController()->getAdsRemoved();

    MenuItemImage* resumeBtn =
        btnWithIcon("menu_pause_icon_resume.png", PauseLayerColorPink, GameplayMenuActionResume);
    MenuItemImage* exitBtn =
        btnWithIcon("menu_pause_icon_exit.png", PauseLayerColorPink, GameplayMenuActionExit);
    MenuItemImage* resetBtn =
        btnWithIcon("menu_pause_icon_reset.png", PauseLayerColorBlue, GameplayMenuActionReset);
    MenuItemImage* characterBtn = btnWithIcon("menu_pause_icon_character.png", PauseLayerColorBlue,
                                              GameplayMenuActionChangeCharacter);
    float btnWidth = resumeBtn->getContentSize().width;
    float btnHeight = resumeBtn->getContentSize().height;  // unused
    // QOL (PC addition): "any character" lets the player change a user level's forced character.
    bool forcedChar = Settings::getInstance()->getCurrentSession()->getLevel()->getForcedChar() &&
                      !qol::canChangeForcedCharacter();
    if (forcedChar)
    {
        characterBtn->setOpacity(0x7d);
    }
    MenuItemImage* replayBtn = btnWithIcon("menu_pause_icon_replay.png", PauseLayerColorBlue,
                                           GameplayMenuActionViewReplay);
    MenuItemImage* adsBtn = nullptr;
    if (!adsRemoved)
    {
        adsBtn = btnWithIcon("menu_pause_icon_ads.png", PauseLayerColorPink,
                             GameplayMenuActionRemoveAds);
    }
    MenuItemImage* optionsBtn = btnWithIcon("menu_pause_icon_options.png", PauseLayerColorPink,
                                            GameplayMenuActionOptions);

    Menu* menu;
    if (adsRemoved)
    {
        menu = Menu::create(resumeBtn, exitBtn, resetBtn, characterBtn, replayBtn, optionsBtn,
                            nullptr);
    }
    else
    {
        menu = Menu::create(resumeBtn, exitBtn, resetBtn, characterBtn, replayBtn, adsBtn,
                            optionsBtn, nullptr);
    }
    menu->setContentSize(getContentSize());
    menu->alignItemsHorizontallyWithPadding(btnWidth * 0.5f);
    addChild(menu);

    Color4B blue(61, 134, 201, 255);
    Color4B pink(254, 110, 134, 255);
    createLabel(resumeBtn, "RESUME", pink)->Node::visit();
    createLabel(exitBtn, "QUIT", pink)->Node::visit();
    createLabel(resetBtn, "RESET\nLEVEL", blue)->Node::visit();
    Label* characterLabel = createLabel(characterBtn, "CHANGE\nCHARACTER", blue);
    characterLabel->Node::visit();
    if (forcedChar)
    {
        characterLabel->setOpacity(0x7d);
    }
    createLabel(replayBtn, "VIEW\nREPLAY", blue)->Node::visit();
    if (adsBtn)
    {
        createLabel(adsBtn, "REMOVE\nADS", pink)->Node::visit();
    }
    createLabel(optionsBtn, "OPTIONS", pink)->Node::visit();
}

// @005fecb0
void PauseLayer::update(float dt)
{
    Settings* settings = Settings::getInstance();
    if (!settings->_hasCachedAlertMessage)
    {
        return;
    }
    settings->_hasCachedAlertMessage = false;
    HWWindow* window =
        Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
    window->showAlertMessage(settings->_cachedAlertTitle, settings->_cachedAlertMessage,
                             settings->_cachedAlertConfirmLabel, "", true);
}

// @005fee64
Label* PauseLayer::createLabel(MenuItemImage* button, std::string text, Color4B color)
{
    float buttonHeight = button->getContentSize().height;
    Vec2 buttonPosition = button->getPosition();
    Label* label = Label::createWithTTF(text, "fonts/Arial Bold.ttf", 52.0f, Size::ZERO,
                                        TextHAlignment::LEFT, TextVAlignment::TOP);
    label->setLineSpacing(-22.0f);
    label->setAdditionalKerning(1.5f);
    label->setAlignment(TextHAlignment::CENTER);
    label->setColor(Color3B::WHITE);
    label->setAnchorPoint(Vec2(0.5f, 1.0f));
    label->enableOutline(color, 10);
    Vec2 position = button->getParent()->convertToWorldSpace(buttonPosition);
    label->setPosition(Vec2(position.x, position.y + buttonHeight * -0.5f + -25.0f));
    addChild(label);
    return label;
}

// @005ff050
MenuItemImage* PauseLayer::btnWithIcon(std::string iconFrameName, PauseLayerColor color, int tag)
{
    std::string normalFrame = "";
    std::string selectedFrame = "";
    switch (color)
    {
    case PauseLayerColorPink:
        normalFrame = "menu_pause_btn_pink_normal.png";
        selectedFrame = "menu_pause_btn_pink_down.png";
        break;
    case PauseLayerColorBlue:
        normalFrame = "menu_pause_btn_blue_normal.png";
        selectedFrame = "menu_pause_btn_blue_down.png";
        break;
    }
    MenuItemImage* item =
        MenuItemImage::create("", "", CC_CALLBACK_1(PauseLayer::btnPressed, this));
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

// @005ff2fc
void PauseLayer::btnPressed(Ref* sender)
{
    int action = ((Node*)sender)->getTag();
    if (action == GameplayMenuActionRemoveAds)
    {
        _waitingForStoreResponse = true;
        Settings::getInstance()->getTracker()->submitAction(s_trackerCategory, "remove_ads_pressed",
                                                            "", -1);
        IAPController* iap = Settings::getInstance()->getIAPController();
        iap->setDelegate(this);
        iap->removeAds();
    }
    else if (action == GameplayMenuActionChangeCharacter)
    {
        if (Settings::getInstance()->getCurrentSession()->getLevel()->getForcedChar() &&
            !qol::canChangeForcedCharacter())  // QOL (PC addition)
        {
            Settings::getInstance()->getTracker()->submitAction(
                s_trackerCategory, "change_character_denied", "", -1);
            HWWindow* window =
                Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
            window->showAlertMessage("Sorry", OW_GAMETEXT(pauseChangeCharacterDenied, 0x00412e9c),
                                     "ok", "", true);
        }
        else
        {
            getEventDispatcher()->dispatchCustomEvent("gameplayMenuAction", &action);
        }
    }
    else if (action == GameplayMenuActionOptions)
    {
        Settings::getInstance()->getTracker()->submitAction(s_trackerCategory, "options_pressed", "",
                                                            -1);
        OptionsMenu* optionsMenu = OptionsMenu::create();
        optionsMenu->setPopSceneOnExit(true);
        Scene* scene = Scene::create();
        scene->addChild(optionsMenu);
        Director::getInstance()->pushScene(scene);
    }
    else
    {
        getEventDispatcher()->dispatchCustomEvent("gameplayMenuAction", &action);
    }
}

// @005ff9b4
void PauseLayer::onExit()
{
    Settings::getInstance()->getIAPController()->setDelegate(nullptr);
    Node::onExit();
}

// @005ff9e4; thunk @005ff9ec
void PauseLayer::onStoreResponse(IAPStoreAction action, std::string productId)
{
    _waitingForStoreResponse = false;
}
