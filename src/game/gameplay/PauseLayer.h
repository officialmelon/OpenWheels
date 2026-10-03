#pragma once

#include "cocos2d.h"
#include "IAPControllerDelegate.h"

#include <string>

// Background art of a pause/victory menu button (btnWithIcon). Values from the binary, names ours.
// RE-TODO(@005ff050): enumerator names are invented.
enum PauseLayerColor
{
    PauseLayerColorPink = 0,  // "menu_pause_btn_pink_normal.png" / "menu_pause_btn_pink_down.png"
    PauseLayerColorBlue = 1,  // "menu_pause_btn_blue_normal.png" / "menu_pause_btn_blue_down.png"
};

// The pause menu (Gameplay::pauseGameplay adds it at z 12). A horizontal Menu of icon buttons with a
// caption label under each ("menus/pause/menu_pause.plist" frames). Button tags are
// GameplayMenuAction values (Gameplay.h): 0 resume, 1 exit, 2 reset, 4 change character, 5 view
// replay, 3 remove ads (only while ads are not removed), 7 options. btnPressed handles 3 (IAP),
// 4 when the level forces a character (alert), and 7 (pushes an OptionsMenu scene) itself; every
// other tag is dispatched as the custom event "gameplayMenuAction" with an int* payload.
//
// arm64 sizeof 0x330 (Gameplay::pauseGameplay: new(nothrow) 0x330, i.e. an inlined CREATE_FUNC).
// IAPControllerDelegate sub-object at +0x320. Primary vtable adds onStoreResponse at vptr+0x648.
class PauseLayer : public cocos2d::Layer, public IAPControllerDelegate
{
public:
    PauseLayer();             // @005fe3c8
    ~PauseLayer() override;   // @005fe400 (D1), @005fe404 (D0)

    // Inlined into Gameplay::pauseGameplay; no symbol.
    CREATE_FUNC(PauseLayer);

    // Layer::init(); _waitingForStoreResponse = false; addMenu(); scheduleUpdate().
    bool init() override;     // @005fe428  vptr+0x4f8
    void addMenu();           // @005fe468
    // Shows the alert queued in Settings (fields at Settings+0: bool pending, +0x8/+0x20/+0x38
    // strings) once, via Settings::createWindow + HWWindow::showAlertMessage.
    void update(float dt) override;   // @005fecb0  vptr+0x3d8
    // Caption under a button: "fonts/Arial Bold.ttf" 52, line spacing -22, kerning 1.5, centred,
    // WHITE text, enableOutline(color, 10), placed under the button (world space).
    cocos2d::Label* createLabel(cocos2d::MenuItemImage* button, std::string text,
                                cocos2d::Color4B color);                             // @005fee64
    cocos2d::MenuItemImage* btnWithIcon(std::string iconFrameName, PauseLayerColor color,
                                        int tag);                                    // @005ff050
    void btnPressed(cocos2d::Ref* sender);    // @005ff2fc
    // IAPController delegate reset, then Node::onExit().
    void onExit() override;                   // @005ff9b4  vptr+0x330

    // IAPControllerDelegate: only clears _waitingForStoreResponse.
    void onStoreResponse(IAPStoreAction action, std::string productId) override;  // @005ff9e4  vptr+0x648; thunk @005ff9ec

protected:
    bool _waitingForStoreResponse;  // +0x328  set when "remove ads" is pressed; never read
};
