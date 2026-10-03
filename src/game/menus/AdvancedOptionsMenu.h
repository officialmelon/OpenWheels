#pragma once

#include "cocos2d.h"
#include "HWWindowDelegate.h"
#include "OptionsMenu.h"  // OptionAction
#include "SecondaryMenu.h"

#include <string>

class HWWindow;
class OptionsMenuItem;

// "Advanced Options" screen: override special side, notch size, controls size, gore, send
// feedback, reset level progress. Gore, feedback and the two resets ask for confirmation in an
// HWWindow first. onExit() reports changes to the Tracker.
//
// arm64 sizeof 0x3f0, HWWindowDelegate at 0x350. There is no user-provided ctor, and create() has
// no symbol because CREATE_FUNC is inlined at both call sites (createScene,
// OptionsMenu::menuItemPressed).
// New primary-vtable entries, in order: hwWindowButtonPressed (+0x658), hwWindowWasDismissed (+0x660).
class AdvancedOptionsMenu : public SecondaryMenu, public HWWindowDelegate
{
public:
    static cocos2d::Scene* createScene();                                   // @0057a038

    CREATE_FUNC(AdvancedOptionsMenu);                                       // (inlined everywhere, no symbol)

    bool init() override;                                                   // @0057a108  vptr+0x4f8
    void addContent() override;                                             // @0057a220  vptr+0x648
    std::string getLabelString(OptionAction action);                        // @0057a81c
    void menuItemPressed(cocos2d::Ref* sender);                             // @0057ab98
    OptionsMenuItem* createMenuItemLabel(std::string text, OptionAction action);  // @0057ac58
    void backBtnPressed() override;                                         // @0057ad90  vptr+0x650

    void toggleGore();                                                      // @0057ae60
    void resetLevelProgess();                                               // @0057b174  (sic: "Progess")
    void resetGame();                                                       // @0057b388  (no menu row reaches it)
    void toggleSendFeedback();                                              // @0057b5c0
    void toggleOverrideSpecialPosition();                                   // @0057b8d4
    void toggleAdjustForNotch();                                            // @0057ba24
    void adjustUserScale();                                                 // @0057bcf4

    void onExit() override;                                                 // @0057bdf8  vptr+0x330

    void hwWindowButtonPressed(int buttonTag, HWWindow* window) override;   // @0057c77c (thunk @0057cab4)
    void hwWindowWasDismissed(HWWindow* window) override;                   // @0057cabc (thunk @0057cafc)

    // ~AdvancedOptionsMenu(): implicit. D0 @0057cb3c; the D1 slot is SecondaryMenu's (see OptionsMenu.h).

protected:
    int _specialSideOverride;               // +0x358  "override_special_position": 0 off, 1 right, 2 left
    cocos2d::Menu* _menu;                   // +0x360
    OptionsMenuItem* _resetGameItem;        // +0x368  nulled in init, never created. RE-TODO: type/name are a guess
    OptionsMenuItem* _resetLevelProgressItem;      // +0x370
    OptionsMenuItem* _goreItem;                    // +0x378
    OptionsMenuItem* _sendFeedbackItem;            // +0x380
    OptionsMenuItem* _overrideSpecialPositionItem; // +0x388
    OptionsMenuItem* _adjustForNotchItem;          // +0x390
    int _notchSize;                         // +0x398  "adjust_controls_for_notch": 0 none, 1 small, 2 large
    OptionsMenuItem* _userScaleItem;        // +0x3a0  "controls size: ..."
    float _menuItemPadding = 35.0f;         // +0x3a8  alignItemsVerticallyWithPadding
    HWWindow* _goreWindow;                  // +0x3b0
    HWWindow* _resetLevelProgressWindow;    // +0x3b8
    HWWindow* _resetGameWindow;             // +0x3c0
    HWWindow* _sendFeedbackWindow;          // +0x3c8  (not cleared in hwWindowWasDismissed, sic)
    // Values at init(); onExit() tracks the changes.
    int _initialGoreDisabled;               // +0x3d0  a bool value held in a 32-bit field (str w / ldr w)
    bool _initialSendFeedbackDisabled;      // +0x3d4
    int _initialNotchSize;                  // +0x3d8
    int _initialSpecialSideOverride;        // +0x3dc
    float _initialUserScale;                // +0x3e0  (float)getIntegerForKey("controls_user_scale")
};
