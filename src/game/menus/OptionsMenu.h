#pragma once

#include "cocos2d.h"
#include "HWWindowDelegate.h"
#include "IAPControllerDelegate.h"  // M10: IAPControllerDelegate, IAPStoreAction
#include "SecondaryMenu.h"

#include <string>

class HWWindow;
class OptionsMenuItem;

// Item tags (getTag()) of the option rows in OptionsMenu and AdvancedOptionsMenu, and the argument
// of their createMenuItemLabel/getLabelString. Values are from the binary; enumerator names are
// invented after the handler methods. 11 and 12 are never referenced.
// RE-TODO(@005fb06c): enumerator names.
enum OptionAction
{
    OptionActionToggleParticles = 0,
    OptionActionToggleGraphicsResolution = 1,
    OptionActionToggleSound = 2,
    OptionActionToggleIntroMusic = 3,
    OptionActionToggleSendFeedback = 4,          // AdvancedOptionsMenu
    OptionActionRemoveAds = 5,
    OptionActionRestorePurchases = 6,
    OptionActionAdvancedOptions = 7,
    OptionActionToggleGore = 8,                  // AdvancedOptionsMenu
    OptionActionResetLevelProgress = 9,          // AdvancedOptionsMenu
    OptionActionResetGame = 10,                  // AdvancedOptionsMenu (label only, no menu row)
    OptionActionOverrideSpecialPosition = 13,    // AdvancedOptionsMenu
    OptionActionAdjustForNotch = 14,             // AdvancedOptionsMenu
    OptionActionAdjustUserScale = 15,            // AdvancedOptionsMenu ("controls size")
};

// "Options" screen: particles, graphics resolution, sound level, intro music, remove ads (only
// while ads are not removed), restore purchases, and "advanced options". It reports setting changes
// to the Tracker in onExit().
//
// arm64 sizeof 0x3a0. HWWindowDelegate is at 0x350 and IAPControllerDelegate at 0x358. There is no
// user-provided ctor (CREATE_FUNC zero-fills).
// New primary-vtable entries, in order: hwWindowButtonPressed (+0x658), hwWindowWasDismissed
// (+0x660), onStoreResponse (+0x668).
class OptionsMenu : public SecondaryMenu, public HWWindowDelegate, public IAPControllerDelegate
{
public:
    static cocos2d::Scene* createScene();                                   // @005fa678

    // @005fa6b4  OptionsMenu::create() (header-inline; PauseLayer inlines it)
    CREATE_FUNC(OptionsMenu);

    bool init() override;                                                   // @005fa770  vptr+0x4f8
    void addContent() override;                                             // @005fa854  vptr+0x648

    OptionsMenuItem* createMenuItemLabel(std::string text, OptionAction action);  // @005fadb0
    std::string getLabelString(OptionAction action);                        // @005faee8
    void menuItemPressed(cocos2d::Ref* sender);                             // @005fb06c

    void toggleParticles();                                                 // @005fb62c
    void promptToggleGraphicsResolution();                                  // @005fb760
    void toggleSound();                                                     // @005fb9dc
    void toggleIntroMusic();                                                // @005fbbc0
    // Static: the call site does not pass `this`.
    static void showCurrentTransactionAlreadyHappeningWindow();             // @005fbcf0
    void confirmToggleGraphicsResolution();                                 // @005fbec4

    void onExit() override;                                                 // @005fbff8  vptr+0x330
    void update(float dt) override;                                         // @005fc964  vptr+0x3d8

    void hwWindowButtonPressed(int buttonTag, HWWindow* window) override;   // @005fcb38 (thunk @005fcb74)
    void hwWindowWasDismissed(HWWindow* window) override;                   // @005fcbb0 (thunk @005fcbc8)
    void onStoreResponse(IAPStoreAction action, std::string message) override;  // @005fcbe0 (thunk @005fcbe8)

    // ~OptionsMenu(): implicit. D0 @005fcbf0 is emitted with the vtable; the D1 slot points at
    // SecondaryMenu::~SecondaryMenu (clang reuses the base dtor for an implicit trivial-bodied
    // dtor). Do not declare one.

protected:
    bool _restoringPurchases;               // +0x360  restore in progress (set when started, cleared by onStoreResponse)
    float _menuItemPadding = 30.0f;         // +0x364  alignItemsVerticallyWithPadding
    int _initialSoundVolumeOffset;          // +0x368  values at init(); onExit() tracks the changes
    bool _initialLowResGraphics;            // +0x36c
    bool _initialParticlesDisabled;         // +0x36d
    bool _initialIntroMusicDisabled;        // +0x36e
    bool _introMusicDisabled;               // +0x36f  current value (toggleIntroMusic)
    cocos2d::Menu* _menu;                   // +0x370
    OptionsMenuItem* _particlesItem;        // +0x378
    OptionsMenuItem* _graphicsItem;         // +0x380
    OptionsMenuItem* _soundItem;            // +0x388
    OptionsMenuItem* _introMusicItem;       // +0x390
    HWWindow* _graphicsWindow;              // +0x398  "Use low/high-res graphics?" prompt while open
};
