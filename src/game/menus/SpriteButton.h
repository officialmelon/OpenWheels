#pragma once

#include "cocos2d.h"

// A Ref holding up to three sprites and a hit rectangle. Nothing in the Android 1.1.3 binary
// creates or uses it (no callers of any of its methods); it is kept for completeness. It is the
// remains of the iOS ButtonWithBatchedSprite (defaultSprite / disabledSprite / pressSprite ivars,
// in that order), which gives the member names.
//
// arm64 layout: cocos2d::Ref (nvsize 0x21 -> 0x28), own fields 0x28..0x50. There is no factory,
// so sizeof is only known to be >= 0x50.
class SpriteButton : public cocos2d::Ref
{
public:
    SpriteButton();                                    // @006332f0
    ~SpriteButton() override;                          // @00633344 (D1), @00633358 (D0)

    // Non-virtual (not in the vtable); always succeeds.
    bool init();                                       // @0063337c
    void setSprites(cocos2d::Sprite* defaultSprite, cocos2d::Sprite* disabledSprite,
                    cocos2d::Sprite* pressSprite);     // @00633384
    // Only acts when hitArea is empty (width or height <= 0) and a default sprite is set: the hit
    // area then becomes the default sprite's bounding box. A non-empty rect is ignored (sic).
    void setHitArea(cocos2d::Rect hitArea);            // @00633390

protected:
    cocos2d::Sprite* _defaultSprite;                   // +0x28
    cocos2d::Sprite* _disabledSprite;                  // +0x30
    cocos2d::Sprite* _pressSprite;                     // +0x38
    cocos2d::Rect _hitArea;                            // +0x40
};
