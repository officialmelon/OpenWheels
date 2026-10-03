#pragma once

#include "cocos2d.h"

#include <functional>

// Shared building blocks of the menu screens (MainMenu, CharacterSelectLayer, LevelSelectMenu,
// SecondaryMenu, CreditsLayer). No RTTI and no instance: only static functions.
class MenuHelper
{
public:
    // Adds "menus/main/menu_main.plist" to the SpriteFrameCache unless already loaded.
    static void loadSprites();                                                          // @005ecb24

    // Full-screen "menus/menu_main_bg.png" (loaded as RGBA8888; the previous default pixel format
    // is restored), anchor (0,0), scaled to the visible size, added to parent at zOrder.
    static cocos2d::Sprite* addBg(cocos2d::Node* parent, int zOrder);                  // @005ecbf8
    // Black LayerColor with alpha 175 added to parent at zOrder.
    static cocos2d::LayerColor* addOverlay(cocos2d::Node* parent, int zOrder);         // @005ecd40

    // Bottom-left "back" button ("menu_main_back_light.png" / "menu_main_back_dark.png"), 90 px
    // from the visible edges, inside its own Menu (at 0,0) added to parent at zOrder.
    // RE-TODO(@005ecdd0): return type is not in the symbol; the item is a MenuItemImage and
    // Ghidra types it MenuItemSprite*.
    static cocos2d::MenuItemSprite* addBackBtn(cocos2d::Node* parent, int zOrder,
                                               const std::function<void(cocos2d::Ref*)>& callback);  // @005ecdd0
    // Bottom-right "confirm" button ("menu_main_confirm_light.png" / "menu_main_confirm_dark.png"),
    // otherwise like addBackBtn.
    static cocos2d::MenuItemSprite* addConfirmBtn(cocos2d::Node* parent, int zOrder,
                                                  const std::function<void(cocos2d::Ref*)>& callback);  // @005ed078
};
