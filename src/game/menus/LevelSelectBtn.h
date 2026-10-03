#pragma once

#include "cocos2d.h"

#include <cstdint>

// One slot of the 5x3 level grid of a chapter in LevelSelectMenu. Not a MenuItem: LevelSelectMenu
// hit-tests getHitArea() itself and forwards the touch with setTouch().
//
// arm64 sizeof 0x320 (create() does a plain `new`, 800 bytes).
class LevelSelectBtn : public cocos2d::Node
{
public:
    LevelSelectBtn();                                  // @005df840
    ~LevelSelectBtn() override;                        // @005df888 (D1), @005df88c (D0)

    // Plain new + init + autorelease (no nothrow, no failure path).
    static LevelSelectBtn* create(int chapter, int level, bool locked, bool completed);  // @005df8b0
    // chapter or level == -1: empty "levSelect_outline.png" slot. Otherwise "levSelect_icon.png"
    // with either the level number sprite "levSelect_<level+1>.png" or "levSelect_lock.png", plus
    // the "levSelect_check.png" check mark (tag 1) when completed. Background opacity 63.
    bool init(int chapter, int level, bool locked, bool completed);                       // @005df930

    // Size of the background sprite's texture rect.
    cocos2d::Size getSize();                           // @005dfcc4
    // Remembers the touch currently pressing this button and shows the pressed state (opacity
    // 255 while touched, 63 otherwise) unless this is an empty slot.
    void setTouch(cocos2d::Touch* touch);              // @005dfcd4
    void showPressedState(bool pressed);               // @005dfd1c
    cocos2d::Touch* getTouch();                        // @005dfd38
    // Background content size centred on this node's position (parent space).
    cocos2d::Rect getHitArea();                        // @005dfd40
    bool getLocked();                                  // @005dfde8
    int getChapter();                                  // @005dfdf0
    int getLevel();                                    // @005dfdf8
    void setChapter(int chapter);                      // @005dfe00
    void setLevel(int level);                          // @005dfe08

protected:
    cocos2d::Sprite* _bg;                              // +0x2f8  "levSelect_icon.png" / "levSelect_outline.png"
    int _chapter;                                      // +0x300  -1
    int _level;                                        // +0x304  -1
    bool _locked;                                      // +0x308  false
    bool _isLevelSlot;                                 // +0x309  true; false for an empty outline slot
    cocos2d::Touch* _touch;                            // +0x310  nullptr
    // RE-TODO(@005df8b0): 8 bytes at +0x318 are never accessed; kept so arm64 sizeof stays 0x320.
    uint8_t _unk318[8];                                // +0x318
};
