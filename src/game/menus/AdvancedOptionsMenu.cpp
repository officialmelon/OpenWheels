#include "AdvancedOptionsMenu.h"

#include "GameText.h"
#include "Globals.h"
#include "HWWindow.h"
#include "OptionsMenuItem.h"
#include "Session.h"
#include "Settings.h"
#include "Tracker.h"
#include "UserProgress.h"

USING_NS_CC;

// TU-local statics, in the order of this TU's static initialiser (@0057cc40).
// UserDefault keys:
static const std::string kGoreDisabledKey = "gore_disabled";                        // @00ac5e90
static const std::string kSendFeedbackDisabledKey = "send_feedback_disabled";       // @00ac5ea8
// Tracker category:
static const std::string kTrackerCategory = "advanced options";                     // @00ac5ec0
// UserDefault keys:
static const std::string kOverrideSpecialPositionKey = "override_special_position"; // @00ac5ee0
static const std::string kAdjustControlsForNotchKey = "adjust_controls_for_notch";  // @00ac5f00
static const std::string kControlsUserScaleKey = "controls_user_scale";             // @00ac5f18

// @0057a038
Scene* AdvancedOptionsMenu::createScene()
{
    Scene* scene = Scene::create();
    scene->addChild(AdvancedOptionsMenu::create());
    return scene;
}

// @0057a108
bool AdvancedOptionsMenu::init()
{
    _title = "Advanced Options";

    UserDefault* userDefault = UserDefault::getInstance();
    _initialSendFeedbackDisabled = userDefault->getBoolForKey(kSendFeedbackDisabledKey.c_str());
    _initialGoreDisabled = userDefault->getBoolForKey(kGoreDisabledKey.c_str());
    int notchSize = userDefault->getIntegerForKey(kAdjustControlsForNotchKey.c_str());
    _initialNotchSize = notchSize;
    _notchSize = notchSize;
    _initialUserScale = (float)userDefault->getIntegerForKey(kControlsUserScaleKey.c_str());
    int specialSideOverride = userDefault->getIntegerForKey(kOverrideSpecialPositionKey.c_str());
    _specialSideOverride = specialSideOverride;
    _initialSpecialSideOverride = specialSideOverride;

    _goreWindow = nullptr;
    _resetGameItem = nullptr;
    _menu = nullptr;
    _goreItem = nullptr;
    _resetLevelProgressItem = nullptr;
    _adjustForNotchItem = nullptr;
    _overrideSpecialPositionItem = nullptr;
    _resetGameWindow = nullptr;
    _resetLevelProgressWindow = nullptr;

    return SecondaryMenu::init();
}

// @0057a220
void AdvancedOptionsMenu::addContent()
{
    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();  // unused

    _overrideSpecialPositionItem =
        OptionsMenuItem::create(getLabelString(OptionActionOverrideSpecialPosition), OptionActionOverrideSpecialPosition,
                                CC_CALLBACK_1(AdvancedOptionsMenu::menuItemPressed, this),
                                OptionsMenuItemAppearanceDefault);
    _adjustForNotchItem =
        OptionsMenuItem::create(getLabelString(OptionActionAdjustForNotch), OptionActionAdjustForNotch,
                                CC_CALLBACK_1(AdvancedOptionsMenu::menuItemPressed, this),
                                OptionsMenuItemAppearanceDefault);
    _userScaleItem =
        OptionsMenuItem::create(getLabelString(OptionActionAdjustUserScale), OptionActionAdjustUserScale,
                                CC_CALLBACK_1(AdvancedOptionsMenu::menuItemPressed, this),
                                OptionsMenuItemAppearanceDefault);
    _resetLevelProgressItem =
        OptionsMenuItem::create(getLabelString(OptionActionResetLevelProgress), OptionActionResetLevelProgress,
                                CC_CALLBACK_1(AdvancedOptionsMenu::menuItemPressed, this),
                                OptionsMenuItemAppearanceRed);
    _sendFeedbackItem = createMenuItemLabel(getLabelString(OptionActionToggleSendFeedback),
                                            OptionActionToggleSendFeedback);
    _goreItem = createMenuItemLabel(getLabelString(OptionActionToggleGore), OptionActionToggleGore);

    _menu = Menu::create(_overrideSpecialPositionItem, _adjustForNotchItem, _userScaleItem, _goreItem,
                         _sendFeedbackItem, _resetLevelProgressItem, nullptr);
    _menu->alignItemsVerticallyWithPadding(_menuItemPadding);
    _menu->setPosition(Vec2(visibleSize.width * 0.5f, visibleSize.height * 0.5f + 37.5f));

    // "reset level progress" sits a little further down than the other rows.
    const Vec2& position = _resetLevelProgressItem->getPosition();
    _resetLevelProgressItem->setPosition(Vec2(position.x, position.y + -75.0f));

    addChild(_menu, 100);
}

// @0057a81c
std::string AdvancedOptionsMenu::getLabelString(OptionAction action)
{
    std::string label;
    UserDefault* userDefault = UserDefault::getInstance();
    switch (action)
    {
    case OptionActionToggleSendFeedback:
        label = userDefault->getBoolForKey(kSendFeedbackDisabledKey.c_str()) ? "send feedback: off"
                                                                              : "send feedback: on";
        break;
    case OptionActionToggleGore:
        label = userDefault->getBoolForKey(kGoreDisabledKey.c_str()) ? "gore: off" : "gore: on";
        break;
    case OptionActionResetLevelProgress:
        label = "reset level progress";
        break;
    case OptionActionResetGame:
        label = "reset game";
        break;
    case OptionActionOverrideSpecialPosition:
        label = "override special side: off";
        if (_specialSideOverride == 1)
        {
            label = "override special side: right";
        }
        else if (_specialSideOverride == 2)
        {
            label = "override special side: left";
        }
        break;
    case OptionActionAdjustForNotch:
    {
        int notchSize = userDefault->getIntegerForKey(kAdjustControlsForNotchKey.c_str());
        label = "notch size: none";
        if (notchSize == 2)
        {
            label = "notch size: large";
        }
        else if (notchSize == 1)
        {
            label = "notch size: small";
        }
        break;
    }
    case OptionActionAdjustUserScale:
    {
        int userScale = userDefault->getIntegerForKey(kControlsUserScaleKey.c_str());
        std::string sizes[4] = {"small", "medium", "large", "huge"};
        label = "controls size: " + sizes[userScale];
        break;
    }
    default:
        break;
    }
    return label;
}

// @0057ab98
void AdvancedOptionsMenu::menuItemPressed(Ref* sender)
{
    switch (static_cast<Node*>(sender)->getTag())
    {
    case OptionActionToggleSendFeedback:
        toggleSendFeedback();
        break;
    case OptionActionToggleGore:
        toggleGore();
        break;
    case OptionActionResetLevelProgress:
        resetLevelProgess();
        break;
    case OptionActionResetGame:
        resetGame();
        break;
    case OptionActionOverrideSpecialPosition:
        toggleOverrideSpecialPosition();
        break;
    case OptionActionAdjustForNotch:
        toggleAdjustForNotch();
        break;
    case OptionActionAdjustUserScale:
        adjustUserScale();
        break;
    default:
        break;
    }
}

// @0057ac58
OptionsMenuItem* AdvancedOptionsMenu::createMenuItemLabel(std::string text, OptionAction action)
{
    return OptionsMenuItem::create(text, action, CC_CALLBACK_1(AdvancedOptionsMenu::menuItemPressed, this),
                                   OptionsMenuItemAppearanceDefault);
}

// @0057ad90
void AdvancedOptionsMenu::backBtnPressed()
{
    bool popSceneOnExit = _popSceneOnExit;
    Director* director = Director::getInstance();
    if (!popSceneOnExit)
    {
        director->pushScene(
            TransitionFade::create(globals::ui::menuFadeTime, OptionsMenu::createScene(), Color3B(0, 0, 0)));
    }
    else
    {
        director->popScene();
    }
}

// @0057ae60
void AdvancedOptionsMenu::toggleGore()
{
    UserDefault* userDefault = UserDefault::getInstance();
    if (!userDefault->getBoolForKey(kGoreDisabledKey.c_str()))
    {
        // Disabling asks first; hwWindowButtonPressed stores the setting.
        _goreWindow = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
        _goreWindow->addDelegate(this);
        _goreWindow->showAlertMessage("Disable blood/gore?", OW_GAMETEXT(advancedOptionsDisableGoreMessage, 0x3f91c0),
                                      "Disable", "Cancel", true);
    }
    else
    {
        // The integer setter on the bool key (as in the original).
        userDefault->setIntegerForKey(kGoreDisabledKey.c_str(), 0);
        userDefault->flush();
        _goreItem->setLabelText(getLabelString(OptionActionToggleGore));
    }
}

// @0057b174
void AdvancedOptionsMenu::resetLevelProgess()
{
    _resetLevelProgressWindow = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
    _resetLevelProgressWindow->addDelegate(this);
    _resetLevelProgressWindow->showAlertMessage("Reset level progress?",
                                                OW_GAMETEXT(advancedOptionsResetLevelProgressMessage, 0x40170b),
                                                "Reset", "Don't do it!", true);
}

// @0057b388
void AdvancedOptionsMenu::resetGame()
{
    _resetGameWindow = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
    _resetGameWindow->addDelegate(this);
    _resetGameWindow->showAlertMessage("Reset everything?", OW_GAMETEXT(advancedOptionsResetGameMessage, 0x40f29d),
                                       "Reset", "Don't do it!", true);
}

// @0057b5c0
void AdvancedOptionsMenu::toggleSendFeedback()
{
    UserDefault* userDefault = UserDefault::getInstance();
    if (!userDefault->getBoolForKey(kSendFeedbackDisabledKey.c_str()))
    {
        // Disabling asks first; hwWindowButtonPressed stores the setting.
        _sendFeedbackWindow = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
        _sendFeedbackWindow->addDelegate(this);
        _sendFeedbackWindow->showAlertMessage("Don't send feedback?",
                                              OW_GAMETEXT(advancedOptionsSendFeedbackMessage, 0x412c86),
                                              "Don't send", "Cancel", true);
    }
    else
    {
        userDefault->setBoolForKey(kSendFeedbackDisabledKey.c_str(), false);
        userDefault->flush();
        _sendFeedbackItem->setLabelText(getLabelString(OptionActionToggleSendFeedback));
        Settings::getInstance()->getTracker()->setSendFeedback(true);
    }
}

// @0057b8d4
void AdvancedOptionsMenu::toggleOverrideSpecialPosition()
{
    UserDefault* userDefault = UserDefault::getInstance();

    // off -> right -> left -> off
    int specialSideOverride = _specialSideOverride;
    if (specialSideOverride == 0)
    {
        specialSideOverride = 1;
    }
    else if (specialSideOverride == 1)
    {
        specialSideOverride = 2;
    }
    else if (specialSideOverride == 2)
    {
        specialSideOverride = 0;
    }
    _specialSideOverride = specialSideOverride;

    userDefault->setIntegerForKey(kOverrideSpecialPositionKey.c_str(), specialSideOverride);
    userDefault->flush();
    _overrideSpecialPositionItem->setLabelText(getLabelString(OptionActionOverrideSpecialPosition));
}

// @0057ba24
void AdvancedOptionsMenu::toggleAdjustForNotch()
{
    UserDefault* userDefault = UserDefault::getInstance();
    int notchSize = userDefault->getIntegerForKey(kAdjustControlsForNotchKey.c_str());

    // none -> small -> large -> none; the first step explains the setting.
    if (notchSize == 1)
    {
        notchSize = 2;
    }
    else if (notchSize == 0)
    {
        HWWindow* window = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
        window->showAlertMessage("adjust controls for notch", OW_GAMETEXT(advancedOptionsAdjustForNotchMessage, 0x409156),
                                 "ok", "", true);
        notchSize = 1;
    }
    else if (notchSize == 2)
    {
        notchSize = 0;
    }
    _notchSize = notchSize;

    userDefault->setIntegerForKey(kAdjustControlsForNotchKey.c_str(), notchSize);
    userDefault->flush();
    _adjustForNotchItem->setLabelText(getLabelString(OptionActionAdjustForNotch));
}

// @0057bcf4
void AdvancedOptionsMenu::adjustUserScale()
{
    UserDefault* userDefault = UserDefault::getInstance();
    int userScale = userDefault->getIntegerForKey(kControlsUserScaleKey.c_str());
    // small -> medium -> large -> small ("huge" is never reached).
    userScale = (userScale < 2) ? userScale + 1 : 0;
    userDefault->setIntegerForKey(kControlsUserScaleKey.c_str(), userScale);
    userDefault->flush();
    _userScaleItem->setLabelText(getLabelString(OptionActionAdjustUserScale));
}

// @0057bdf8
void AdvancedOptionsMenu::onExit()
{
    Session* session = Settings::getInstance()->getCurrentSession();
    if (session)
    {
        if (UserDefault::getInstance()->getBoolForKey(kGoreDisabledKey.c_str()))
        {
            session->userHasDisabledGoreDuringSession();
        }
    }

    // Report what changed while the menu was open.
    UserDefault* userDefault = UserDefault::getInstance();
    bool sendFeedbackDisabled = userDefault->getBoolForKey(kSendFeedbackDisabledKey.c_str());
    bool goreDisabled = userDefault->getBoolForKey(kGoreDisabledKey.c_str());
    int notchSize = userDefault->getIntegerForKey(kAdjustControlsForNotchKey.c_str());
    int specialSideOverride = userDefault->getIntegerForKey(kOverrideSpecialPositionKey.c_str());
    int userScale = userDefault->getIntegerForKey(kControlsUserScaleKey.c_str());

    if (specialSideOverride != _initialSpecialSideOverride)
    {
        if (specialSideOverride == 2)
        {
            Settings::getInstance()->getTracker()->submitAction(kTrackerCategory, "override_special_left", "", -1);
        }
        else if (specialSideOverride == 1)
        {
            Settings::getInstance()->getTracker()->submitAction(kTrackerCategory, "override_special_right", "", -1);
        }
    }

    if (notchSize != _initialNotchSize)
    {
        if (notchSize == 2)
        {
            Settings::getInstance()->getTracker()->submitAction(kTrackerCategory, "notch_large", "", -1);
        }
        else if (notchSize == 1)
        {
            Settings::getInstance()->getTracker()->submitAction(kTrackerCategory, "notch_small", "", -1);
        }
    }

    if (_initialGoreDisabled != (int)goreDisabled)
    {
        Tracker* tracker = Settings::getInstance()->getTracker();
        if (!goreDisabled)
        {
            tracker->submitAction(kTrackerCategory, "gore_enabled", "", -1);
        }
        else
        {
            tracker->submitAction(kTrackerCategory, "gore_disabled", "", -1);
        }
    }

    if (_initialSendFeedbackDisabled != sendFeedbackDisabled)
    {
        Tracker* tracker = Settings::getInstance()->getTracker();
        if (!sendFeedbackDisabled)
        {
            tracker->submitAction(kTrackerCategory, "feedback_enabled", "", -1);
        }
        else
        {
            tracker->submitAction(kTrackerCategory, "feedback_disabled", "", -1);
        }
    }

    if (_initialUserScale != (float)userScale)
    {
        Settings::getInstance()->getTracker()->submitAction(kTrackerCategory, "controls_scale", "size", userScale);
    }

    Node::onExit();
}

// @0057c77c (non-virtual thunk @0057cab4)
void AdvancedOptionsMenu::hwWindowButtonPressed(int buttonTag, HWWindow* window)
{
    if (_resetLevelProgressWindow == window)
    {
        if (buttonTag == 1)
        {
            Settings::getInstance()->getUserProgress()->resetLevelProgress();
        }
        _resetLevelProgressWindow = nullptr;
        // Tracked whichever button was pressed (as in the original).
        Settings::getInstance()->getTracker()->submitAction(kTrackerCategory, "reset_level_progress", "", -1);
    }
    else if (_goreWindow == window)
    {
        if (buttonTag == 1)
        {
            UserDefault* userDefault = UserDefault::getInstance();
            userDefault->setBoolForKey(kGoreDisabledKey.c_str(), true);
            userDefault->flush();
            _goreItem->setLabelText(getLabelString(OptionActionToggleGore));
        }
        _goreWindow = nullptr;
    }
    else if (_resetGameWindow == window)
    {
        // Confirming has no effect.
        _resetGameWindow = nullptr;
    }
    else if (_sendFeedbackWindow == window)
    {
        if (buttonTag == 1)
        {
            UserDefault* userDefault = UserDefault::getInstance();
            userDefault->setBoolForKey(kSendFeedbackDisabledKey.c_str(), true);
            userDefault->flush();
            _sendFeedbackItem->setLabelText(getLabelString(OptionActionToggleSendFeedback));
            Settings::getInstance()->getTracker()->setSendFeedback(false);
        }
        _sendFeedbackWindow = nullptr;
    }
}

// @0057cabc (non-virtual thunk @0057cafc)
void AdvancedOptionsMenu::hwWindowWasDismissed(HWWindow* window)
{
    // _sendFeedbackWindow is not cleared here (as in the original).
    if (_resetLevelProgressWindow == window)
    {
        _resetLevelProgressWindow = nullptr;
    }
    else if (_goreWindow == window)
    {
        _goreWindow = nullptr;
    }
    else if (_resetGameWindow == window)
    {
        _resetGameWindow = nullptr;
    }
}
