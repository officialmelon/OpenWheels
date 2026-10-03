#include "InfoMenu.h"

#include "CreditsLayer.h"
#include "GameText.h"
#include "Globals.h"
#include "HWWindow.h"
#include "OptionsMenuItem.h"
#include "Settings.h"
#include "Tracker.h"
#include "sdkbox/PluginReview.h"

USING_NS_CC;

// TU-local statics, in the order of this TU's static initialiser (_INIT_11 @005c9e6c).
static const std::string kSupportURL = "https://www.fancyforce.com/mobile-support.html";       // @00ac6220
static const std::string kTrackerCategory = "info";                                            // @00ac6238
static const std::string kPrivacyPolicyURL = "http://totaljerkface.com/mobile_terms_android.tjf"; // @00ac6250

// @005c8bb0
Scene* InfoMenu::createScene()
{
    Scene* scene = Scene::create();
    scene->addChild(InfoMenu::create());
    return scene;
}

// @005c8cb0 (also inlined into InfoMenu::create @005c8bec)
bool InfoMenu::init()
{
    _title = "Info";
    return SecondaryMenu::init();
}

// @005c8ce0
void InfoMenu::addContent()
{
    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();  // unused

    OptionsMenuItem* moreItem = OptionsMenuItem::create("more happy wheels?", 0,
                                                        CC_CALLBACK_1(InfoMenu::menuItemPressed, this),
                                                        OptionsMenuItemAppearanceDefault);
    OptionsMenuItem* supportItem = OptionsMenuItem::create("support", 2, CC_CALLBACK_1(InfoMenu::menuItemPressed, this),
                                                           OptionsMenuItemAppearanceDefault);
    OptionsMenuItem* rateItem = OptionsMenuItem::create("rate", 4, CC_CALLBACK_1(InfoMenu::menuItemPressed, this),
                                                        OptionsMenuItemAppearanceDefault);
    OptionsMenuItem* creditsItem = OptionsMenuItem::create("credits", 3, CC_CALLBACK_1(InfoMenu::menuItemPressed, this),
                                                           OptionsMenuItemAppearanceDefault);
    OptionsMenuItem* privacyPolicyItem =
        OptionsMenuItem::create("privacy policy", 5, CC_CALLBACK_1(InfoMenu::menuItemPressed, this),
                                OptionsMenuItemAppearanceDefault);

    _menu = Menu::create(moreItem, supportItem, rateItem, creditsItem, privacyPolicyItem, nullptr);
    _menu->alignItemsVerticallyWithPadding(_menuItemPadding);
    _menu->setPosition(Vec2(visibleSize.width * 0.5f, visibleSize.height * 0.5f));
    addChild(_menu, 100);
}

// @005c9214
void InfoMenu::menuItemPressed(Ref* sender)
{
    switch (static_cast<Node*>(sender)->getTag())
    {
    case 0:
    {
        Settings::getInstance()->getTracker()->submitAction(kTrackerCategory, "more_happy_wheels_pressed", "", -1);
        std::string title = "More!?";
        std::string message = OW_GAMETEXT(infoMoreHappyWheelsMessage, 0x3f9f77);
        HWWindow* window = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
        window->showAlertMessage(title, message, "ok", "", true);
        break;
    }
    case 2:
        _supportWindow = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
        _supportWindow->addDelegate(this);
        _supportWindow->showAlertMessage("Visit support website?", "This will exit the app.", "yes", "no", true);
        break;
    case 3:
        Settings::getInstance()->getTracker()->submitAction(kTrackerCategory, "credits_pressed", "", -1);
        Director::getInstance()->pushScene(
            TransitionFade::create(globals::ui::menuFadeTime, CreditsLayer::createScene(false), Color3B(0, 0, 0)));
        break;
    case 4:
        Settings::getInstance()->getTracker()->submitAction(kTrackerCategory, "rate_pressed", "", -1);
        sdkbox::PluginReview::rate();
        break;
    case 5:
        Application::getInstance()->openURL(kPrivacyPolicyURL);
        break;
    default:
        break;
    }
}

// @005c99c8 (non-virtual thunk @005c9c20)
void InfoMenu::hwWindowButtonPressed(int buttonTag, HWWindow* window)
{
    if (_supportWindow == window)
    {
        Tracker* tracker = Settings::getInstance()->getTracker();
        if (buttonTag == 1)
        {
            tracker->submitAction(kTrackerCategory, "visited_support", "", -1);
            Application::getInstance()->openURL(kSupportURL);
        }
        else
        {
            tracker->submitAction(kTrackerCategory, "visit_support_declined", "", -1);
        }
        _supportWindow = nullptr;
    }
}

// @005c9c28 (non-virtual thunk @005c9d84)
void InfoMenu::hwWindowWasDismissed(HWWindow* window)
{
    if (_supportWindow == window)
    {
        Settings::getInstance()->getTracker()->submitAction(kTrackerCategory, "visit_support_declined", "", -1);
        _supportWindow = nullptr;
    }
}
