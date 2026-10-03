#pragma once

#include "cocos2d.h"

#include <cstdint>
#include <string>

// Character picker shown before a level whose chapter has no fixed character (characterIndex -1).
// A grid of 13 character frames, two swaying spotlights, and a live Box2D preview: selecting a
// character recreates the Session world and drops the character (with its vehicle locked) onto a
// static platform.
//
// NOTE: the original declares the base without an access specifier, i.e. *private* inheritance
// (the typeinfo is __vmi_class_type_info with base flags 0). Kept as such: every Node/Layer use
// happens inside member functions.
//
// arm64 sizeof 0x390 (createScene allocates 0x390 with new (std::nothrow)).
class CharacterSelectLayer : cocos2d::Layer
{
public:
    CharacterSelectLayer();                            // @005a2328
    ~CharacterSelectLayer() override;                  // @005a23c0 (D1), @005a2400 (D0)

    // Scene + new (std::nothrow) layer + init + autorelease (no null / failure checks).
    // Both arguments are unused by init.
    static cocos2d::Scene* createScene(int unused1, unsigned long unused2);  // @005a2424
    bool init(int unused1, unsigned long unused2);     // @005a24a0  always true

    void onEnterTransitionDidFinish() override;        // @005a29a8  vptr+0x328, Layer's
    void onEnter() override;                           // @005a29ac  vptr+0x320
    // index: character to preview; trackPreview: also store the name and send "character_previewed".
    void selectCharacter(int index, bool trackPreview);  // @005a2a48
    void onExitTransitionDidStart() override;          // @005a2f3c  vptr+0x338, Layer's
    void addCharacterMenu();                           // @005a2f40
    void handleBackButtonReleased();                   // @005a3508  back to MainMenu (MenuModeLevelSelect)
    void handleConfirmButtonReleased();                // @005a35c8  starts Gameplay
    void removeUnusedTexturesAndSpriteFrames();        // @005a381c
    void update(float dt) override;                    // @005a383c  vptr+0x3d8
    void adjustSpotlights();                           // @005a3a3c
    // "<key>_icon_bw.png" / "<key>_icon.png" (key "biz" when empty), tag = character index.
    // RE-TODO(@005a3b5c): return type is not in the symbol; the item is a MenuItemImage and
    // Ghidra types it MenuItemSprite*.
    cocos2d::MenuItemSprite* createBtn(std::string key, int tag);  // @005a3b5c
    void characterBtnPressed(cocos2d::Ref* sender);    // @005a3f88
    void killCharacter();                              // @005a411c  empty
    // Static Box2D box (810 x 125 half-extents, in pixels / ptm) under the preview character.
    void createPlatform();                             // @005a4120

protected:
    // Member names from the iOS CharacterSelectLayer ivars where the Android field descends from
    // one (python tools/re/ios_ivars.py CharacterSelectLayer); the rest are Android additions.
    cocos2d::Sprite* _spotlight1;                      // +0x320  "spotlight.png", left
    cocos2d::Sprite* _spotlight2;                      // +0x328  "spotlight.png", right
    cocos2d::Sprite* _glowFrame;                       // +0x330  "charSel_frame_highlight.png"; not initialised by the constructor
    float _scaleInc1;                                  // +0x338  0     (sin phase, +0.01 per frame)
    float _rotInc1;                                    // +0x33c  0     (sin phase, +0.02 per frame)
    float _scaleInc2;                                  // +0x340  3.14  (cos phase, -0.012 per frame)
    float _rotInc2;                                    // +0x344  3.14  (cos phase, -0.02 per frame)
    // RE-TODO(@005a2328): 4 bytes zeroed by the constructor, never used (iOS has a float
    // defaultSpotlightScale next to the phases).
    int _unk348;                                       // +0x348
    cocos2d::Menu* _vehicleMenu;                       // +0x350  character buttons; not initialised by the constructor
    cocos2d::MenuItem* _lastBtn;                       // +0x358  selected character button
    int _currentID;                                    // +0x360  -1; selected character index
    std::string _selectedCharacterName;                // +0x368  tracker label ("name" of the character data)
    bool _confirmed;                                   // +0x380  set by handleConfirmButtonReleased
    bool _characterBtnPressedThisFrame;                // +0x381  cleared at the end of update()
    // RE-TODO(@005a2424): 14 bytes at +0x382 are never accessed; kept so arm64 sizeof stays 0x390.
    uint8_t _unk382[14];                               // +0x382
};
