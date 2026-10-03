#pragma once
// iOS EditorLayerButton : ButtonWithBatchedSprite (instanceSize 0x2d8) - a toolbar button of the
// level editor: background frame ("editorui_btn.png" by default, "editorui_blueBtn.png" for Test)
// with a centred icon child (z 1). Lives in EditorLayer's uiSBN ("editorui" atlas).
//
// Disabled look: icon opacity 0x59, button opacity 0x3f; enabled: both 0xff (setIsEnabled). The
// press state is a dark grey / dark blue frame with its own copy of the icon.
//
// Sprite sizes: the editorui atlas is loaded from the iOS bundle via EditorAssets (-ipad / -ipadhd).
// EditorLayer lays its UI out in iPad points (its own scale = EditorAssets::pointsToDesign()), so
// sprites from a 2x atlas carry setScale(1 / EditorAssets::pixelsPerPoint()); code that used iOS
// point contentSize goes through EditorLayer::pointSize(node).

#include "ButtonWithBatchedSprite.h"

class EditorLayerButton : public ButtonWithBatchedSprite
{
public:
    // [[EditorLayerButton alloc] initWithSpriteFrameName:icon:] / initWithIcon:, autoreleased.
    static EditorLayerButton* createWithSpriteFrameName(const std::string& spriteFrameName,
                                                        const std::string& icon);
    static EditorLayerButton* createWithIcon(const std::string& icon);

    // CCSprite initWithSpriteFrameName: (-> ButtonWithBatchedSprite::initWithSpriteFrame), then
    // the icon sprite at contentSize / 2, z 1; remembers the icon frame name for the press state.
    bool initWithSpriteFrameName(const std::string& spriteFrameName, const std::string& icon);  // @ios 1000dcca8
    // initWithSpriteFrameName:@"editorui_btn.png" icon:icon.
    bool initWithIcon(const std::string& icon);                            // @ios 1000dcd5c
    void setIsEnabled(bool isEnabled) override;                            // @ios 1000dcd6c
    void createPressState();                                               // @ios 1000dce04  (grey)
    // grey ? "editorui_darkGreybtn.png" : "editorui_darkBluebtn.png", plus an icon copy.
    void createPressStateWithGrey(bool grey);                              // @ios 1000dce0c
    cocos2d::Sprite* icon();                                               // @ios 1000dceb8
    void setIcon(cocos2d::Sprite* icon);                                   // @ios 1000dcec8

protected:
    EditorLayerButton() = default;
    ~EditorLayerButton() override = default;

    cocos2d::Sprite* _icon = nullptr;       // +0x2c8  iOS "icon" (clashes with icon())
    std::string _iconSpriteFrameName;       // +0x2d0
};
