#include "OptionsMenu.h"

#include "qol/QoLMenu.h"  // QOL (PC addition)

// QOL (PC addition): menu tag of the "quality of life" row (not an Android OptionAction value).
static const int kOptionActionQualityOfLife = 17;

#include "AdController.h"
#include "AdvancedOptionsMenu.h"
#include "GameText.h"
#include "Globals.h"
#include "HWWindow.h"
#include "IAPController.h"
#include "OptionsMenuItem.h"
#include "Settings.h"
#include "SoundController.h"
#include "Tracker.h"

USING_NS_CC;

// TU-local statics, in the order of this TU's static initialiser (_INIT_15 @005fccd0).
// UserDefault keys:
static const std::string kParticlesDisabledKey = "particles_disabled";          // @00ac6430
static const std::string kSendFeedbackDisabledKey = "send_feedback_disabled";   // @00ac6448 (unused here)
static const std::string kSoundVolumeOffsetKey = "sound_volume_offset";         // @00ac6460
static const std::string kLastAudibleSoundOffsetKey = "last_audible_sound_offset";  // @00ac6480
static const std::string kUseLowResGraphicsKey = "use_low_res_graphics";        // @00ac6498
static const std::string kIntroMusicDisabledKey = "intro_music_disabled";       // @00ac64b0
// Tracker category:
static const std::string kTrackerCategory = "options";                          // @00ac64c8

// @005fa678
Scene* OptionsMenu::createScene()
{
    Scene* scene = Scene::create();
    scene->addChild(OptionsMenu::create());
    return scene;
}

// @005fa770
bool OptionsMenu::init()
{
    _introMusicItem = nullptr;
    _title = "Options";

    UserDefault* userDefault = UserDefault::getInstance();
    _initialSoundVolumeOffset = userDefault->getIntegerForKey(kSoundVolumeOffsetKey.c_str());
    _initialLowResGraphics = userDefault->getBoolForKey(kUseLowResGraphicsKey.c_str());
    _initialParticlesDisabled = userDefault->getBoolForKey(kParticlesDisabledKey.c_str());
    bool introMusicDisabled = userDefault->getBoolForKey(kIntroMusicDisabledKey.c_str());
    _restoringPurchases = false;
    _initialIntroMusicDisabled = introMusicDisabled;
    _introMusicDisabled = introMusicDisabled;

    scheduleUpdate();
    return SecondaryMenu::init();
}

// @005fa854
void OptionsMenu::addContent()
{
    Size visibleSize = Director::getInstance()->getVisibleSize();

    _particlesItem = createMenuItemLabel(getLabelString(OptionActionToggleParticles), OptionActionToggleParticles);
    _graphicsItem = createMenuItemLabel(getLabelString(OptionActionToggleGraphicsResolution),
                                        OptionActionToggleGraphicsResolution);
    _soundItem = createMenuItemLabel(getLabelString(OptionActionToggleSound), OptionActionToggleSound);
    _introMusicItem = createMenuItemLabel(getLabelString(OptionActionToggleIntroMusic), OptionActionToggleIntroMusic);

    bool adsRemoved = Settings::getInstance()->getAdController()->getAdsRemoved();
    OptionsMenuItem* removeAdsItem = nullptr;
    if (!adsRemoved)
    {
        removeAdsItem = createMenuItemLabel("remove ads", OptionActionRemoveAds);
    }
    // QOL (PC addition): OpenWheels has no store, so the "restore purchases" row opens the
    // Quality of Life page instead (blue, like "advanced options").
    OptionsMenuItem* restorePurchasesItem =
        OptionsMenuItem::create("quality of life", kOptionActionQualityOfLife,
                                CC_CALLBACK_1(OptionsMenu::menuItemPressed, this), OptionsMenuItemAppearanceBlue);
    OptionsMenuItem* advancedOptionsItem =
        OptionsMenuItem::create("advanced options", OptionActionAdvancedOptions,
                                CC_CALLBACK_1(OptionsMenu::menuItemPressed, this), OptionsMenuItemAppearanceBlue);

    if (!adsRemoved)
    {
        _menu = Menu::create(_particlesItem, _graphicsItem, _soundItem, _introMusicItem, removeAdsItem,
                             restorePurchasesItem, advancedOptionsItem, nullptr);
    }
    else
    {
        _menu = Menu::create(_particlesItem, _graphicsItem, _soundItem, _introMusicItem, restorePurchasesItem,
                             advancedOptionsItem, nullptr);
    }
    _menu->alignItemsVerticallyWithPadding(_menuItemPadding);
    _menu->setPosition(Vec2(visibleSize.width * 0.5f, visibleSize.height * 0.5f + 37.5f));

    // "advanced options" sits a little further down than the other rows.
    const Vec2& position = advancedOptionsItem->getPosition();
    advancedOptionsItem->setPosition(Vec2(position.x, position.y + -75.0f));

    addChild(_menu, 100);
}

// @005fadb0
OptionsMenuItem* OptionsMenu::createMenuItemLabel(std::string text, OptionAction action)
{
    return OptionsMenuItem::create(text, action, CC_CALLBACK_1(OptionsMenu::menuItemPressed, this),
                                   OptionsMenuItemAppearanceDefault);
}

// @005faee8
std::string OptionsMenu::getLabelString(OptionAction action)
{
    std::string label;
    UserDefault* userDefault = UserDefault::getInstance();
    switch (action)
    {
    case OptionActionToggleParticles:
        label = userDefault->getBoolForKey(kParticlesDisabledKey.c_str()) ? "particles: off" : "particles: on";
        break;
    case OptionActionToggleGraphicsResolution:
        label = userDefault->getBoolForKey(kUseLowResGraphicsKey.c_str()) ? "graphics: low-res" : "graphics: high-res";
        break;
    case OptionActionToggleSound:
    {
        int soundVolumeOffset = userDefault->getIntegerForKey(kSoundVolumeOffsetKey.c_str());
        if (soundVolumeOffset == -100)
        {
            label = "sound: off";
        }
        else if (soundVolumeOffset == -65)
        {
            label = "sound: low";
        }
        else if (soundVolumeOffset == 0)
        {
            label = "sound: high";
        }
        break;
    }
    case OptionActionToggleIntroMusic:
        label = _introMusicDisabled ? "intro music: off" : "intro music: on";
        break;
    default:
        break;
    }
    return label;
}

// @005fb06c
void OptionsMenu::menuItemPressed(Ref* sender)
{
    switch (static_cast<Node*>(sender)->getTag())
    {
    case OptionActionToggleParticles:
        toggleParticles();
        break;
    case OptionActionToggleGraphicsResolution:
        promptToggleGraphicsResolution();
        break;
    case OptionActionToggleSound:
        toggleSound();
        break;
    case OptionActionToggleIntroMusic:
        toggleIntroMusic();
        break;
    case OptionActionRemoveAds:
        Settings::getInstance()->getTracker()->submitAction(kTrackerCategory, "remove_ads_pressed", "", -1);
        Settings::getInstance()->getIAPController()->removeAds();
        break;
    case OptionActionRestorePurchases:
        Settings::getInstance()->getTracker()->submitAction(kTrackerCategory, "restore_purchases_pressed", "", -1);
        if (!_restoringPurchases)
        {
            IAPController* iapController = Settings::getInstance()->getIAPController();
            iapController->setDelegate(this);
            iapController->restorePurchases();
            _restoringPurchases = true;
        }
        else
        {
            showCurrentTransactionAlreadyHappeningWindow();
        }
        break;
    case kOptionActionQualityOfLife:  // QOL (PC addition)
        if (!_popSceneOnExit)
        {
            Director::getInstance()->replaceScene(
                TransitionFade::create(globals::ui::menuFadeTime, QoLMenu::createScene(), Color3B(0, 0, 0)));
        }
        else
        {
            QoLMenu* qolMenu = QoLMenu::create();
            qolMenu->setPopSceneOnExit(true);
            Scene* scene = Scene::create();
            scene->addChild(qolMenu);
            Director::getInstance()->pushScene(scene);
        }
        break;
    case OptionActionAdvancedOptions:
        Settings::getInstance()->getTracker()->submitAction(kTrackerCategory, "advanced_options_pressed", "", -1);
        if (!_popSceneOnExit)
        {
            Director::getInstance()->replaceScene(TransitionFade::create(
                globals::ui::menuFadeTime, AdvancedOptionsMenu::createScene(), Color3B(0, 0, 0)));
        }
        else
        {
            // Opened from the pause menu: push without a transition, the back button pops it.
            AdvancedOptionsMenu* advancedOptionsMenu = AdvancedOptionsMenu::create();
            advancedOptionsMenu->setPopSceneOnExit(true);
            Scene* scene = Scene::create();
            scene->addChild(advancedOptionsMenu);
            Director::getInstance()->pushScene(scene);
        }
        break;
    default:
        break;
    }
}

// @005fb62c
void OptionsMenu::toggleParticles()
{
    UserDefault* userDefault = UserDefault::getInstance();
    bool particlesDisabled = userDefault->getBoolForKey(kParticlesDisabledKey.c_str());
    userDefault->setBoolForKey(kParticlesDisabledKey.c_str(), !particlesDisabled);
    userDefault->flush();
    _particlesItem->setLabelText(getLabelString(OptionActionToggleParticles));
}

// @005fb760
void OptionsMenu::promptToggleGraphicsResolution()
{
    bool lowResGraphics = UserDefault::getInstance()->getBoolForKey(kUseLowResGraphicsKey.c_str());

    std::string title;
    std::string message;
    if (!lowResGraphics)
    {
        title = "Use low-res graphics?";
        message = OW_GAMETEXT(optionsUseLowResGraphicsMessage, 0x404963);
    }
    else
    {
        title = "Use high-res graphics?";
        message = OW_GAMETEXT(optionsUseHighResGraphicsMessage, 0x4048e5);
    }

    if (_graphicsWindow == nullptr)
    {
        _graphicsWindow = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
        _graphicsWindow->addDelegate(this);
        _graphicsWindow->showAlertMessage(title, message, "yes", "no", true);
    }
}

// @005fb9dc
void OptionsMenu::toggleSound()
{
    UserDefault* userDefault = UserDefault::getInstance();
    int soundVolumeOffset = userDefault->getIntegerForKey(kSoundVolumeOffsetKey.c_str());

    // off -> high -> low -> off
    if (soundVolumeOffset == -100)
    {
        soundVolumeOffset = 0;
    }
    else if (soundVolumeOffset == -65)
    {
        soundVolumeOffset = -100;
    }
    else if (soundVolumeOffset == 0)
    {
        soundVolumeOffset = -65;
    }

    Settings::getInstance()->getSoundController()->setMasterVolume((float)(soundVolumeOffset + 100) / 100.0f);

    if (soundVolumeOffset == 0 || soundVolumeOffset == -65)
    {
        userDefault->setIntegerForKey(kLastAudibleSoundOffsetKey.c_str(), soundVolumeOffset);
    }
    userDefault->setIntegerForKey(kSoundVolumeOffsetKey.c_str(), soundVolumeOffset);
    userDefault->flush();

    _soundItem->setLabelText(getLabelString(OptionActionToggleSound));
}

// @005fbbc0
void OptionsMenu::toggleIntroMusic()
{
    UserDefault* userDefault = UserDefault::getInstance();

    bool wasDisabled = _introMusicDisabled;
    _introMusicDisabled = !wasDisabled;
    if (!wasDisabled)
    {
        Settings::getInstance()->getSoundController()->stopBackgroundMusic();
    }

    userDefault->setBoolForKey(kIntroMusicDisabledKey.c_str(), _introMusicDisabled);
    userDefault->flush();

    _introMusicItem->setLabelText(getLabelString(OptionActionToggleIntroMusic));
}

// @005fbcf0
void OptionsMenu::showCurrentTransactionAlreadyHappeningWindow()
{
    HWWindow* window = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
    window->showAlertMessage("busy...", OW_GAMETEXT(optionsStoreTransactionBusyMessage, 0x405585), "ok", "", true);
}

// @005fbec4
void OptionsMenu::confirmToggleGraphicsResolution()
{
    UserDefault* userDefault = UserDefault::getInstance();
    bool lowResGraphics = userDefault->getBoolForKey(kUseLowResGraphicsKey.c_str());
    userDefault->setBoolForKey(kUseLowResGraphicsKey.c_str(), !lowResGraphics);
    userDefault->flush();
    _graphicsItem->setLabelText(getLabelString(OptionActionToggleGraphicsResolution));
}

// @005fbff8
void OptionsMenu::onExit()
{
    Settings::getInstance()->getIAPController()->setDelegate(nullptr);

    // Report what changed while the menu was open.
    UserDefault* userDefault = UserDefault::getInstance();
    int soundVolumeOffset = userDefault->getIntegerForKey(kSoundVolumeOffsetKey.c_str());
    bool lowResGraphics = userDefault->getBoolForKey(kUseLowResGraphicsKey.c_str());
    bool particlesDisabled = userDefault->getBoolForKey(kParticlesDisabledKey.c_str());
    bool introMusicDisabled = userDefault->getBoolForKey(kIntroMusicDisabledKey.c_str());

    if (soundVolumeOffset != _initialSoundVolumeOffset)
    {
        std::string action = "sound_changed";
        if (soundVolumeOffset == -100)
        {
            Settings::getInstance()->getTracker()->submitAction(kTrackerCategory, action, "off", -1);
        }
        else
        {
            if (soundVolumeOffset == -65)
            {
                Settings::getInstance()->getTracker()->submitAction(kTrackerCategory, action, "low", -1);
            }
            if (soundVolumeOffset == 0)
            {
                Settings::getInstance()->getTracker()->submitAction(kTrackerCategory, action, "high", -1);
            }
        }
    }

    if (_initialIntroMusicDisabled != introMusicDisabled)
    {
        Tracker* tracker = Settings::getInstance()->getTracker();
        if (!introMusicDisabled)
        {
            tracker->submitAction(kTrackerCategory, "intro_music_enabled", "", -1);
        }
        else
        {
            tracker->submitAction(kTrackerCategory, "intro_music_disabled", "", -1);
        }
    }

    if (_initialLowResGraphics != lowResGraphics)
    {
        Tracker* tracker = Settings::getInstance()->getTracker();
        if (!lowResGraphics)
        {
            tracker->submitAction(kTrackerCategory, "use_high_res", "", -1);
        }
        else
        {
            tracker->submitAction(kTrackerCategory, "use_low_res", "", -1);
        }
    }

    if (_initialParticlesDisabled != particlesDisabled)
    {
        Tracker* tracker = Settings::getInstance()->getTracker();
        if (!particlesDisabled)
        {
            tracker->submitAction(kTrackerCategory, "particles_enabled", "", -1);
        }
        else
        {
            tracker->submitAction(kTrackerCategory, "particles_disabled", "", -1);
        }
    }

    Node::onExit();
}

// @005fc964
void OptionsMenu::update(float dt)
{
    // Store results that arrived while the app was in the background are cached in Settings.
    Settings* settings = Settings::getInstance();
    if (settings->_hasCachedAlertMessage)
    {
        settings->_hasCachedAlertMessage = false;
        HWWindow* window = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
        window->showAlertMessage(settings->_cachedAlertTitle, settings->_cachedAlertMessage,
                                 settings->_cachedAlertConfirmLabel, "", true);
    }

    if (!_restoringPurchases)
    {
        Settings::getInstance()->getIAPController()->displayMessageStupidAssWorkaroundFuckThis();
    }
}

// @005fcb38 (non-virtual thunk @005fcb74)
void OptionsMenu::hwWindowButtonPressed(int buttonTag, HWWindow* window)
{
    if (_graphicsWindow == window)
    {
        if (buttonTag == 1)
        {
            confirmToggleGraphicsResolution();
        }
        _graphicsWindow = nullptr;
    }
}

// @005fcbb0 (non-virtual thunk @005fcbc8)
void OptionsMenu::hwWindowWasDismissed(HWWindow* window)
{
    if (_graphicsWindow == window)
    {
        _graphicsWindow = nullptr;
    }
}

// @005fcbe0 (non-virtual thunk @005fcbe8)
void OptionsMenu::onStoreResponse(IAPStoreAction action, std::string productId)
{
    _restoringPurchases = false;
}
