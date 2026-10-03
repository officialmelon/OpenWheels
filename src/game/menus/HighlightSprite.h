#pragma once

#include "cocos2d.h"

// The bouncing yellow arrow Gameplay places over things it wants to point out
// (Gameplay::highlightSpriteAtPos creates it, Gameplay::clearHighlights calls fadeOut()).
// Member names from the iOS HighlightSprite ivars (_yRange, _sprite, _delegate).
//
// arm64 sizeof 0x310 (Gameplay's inlined create() allocates 0x310). The constructor initialises
// no fields; init() sets them.
class HighlightSprite : public cocos2d::Node
{
public:
    // Inlined into Gameplay in the binary: new (std::nothrow), virtual init(), autorelease /
    // virtual delete on failure.
    CREATE_FUNC(HighlightSprite);

    HighlightSprite();                                 // @00649008
    ~HighlightSprite() override;                       // @00649038 (D1), @0064903c (D0)

    // "images/yellow_arrow.png", invisible, fades in over 0.5s and bobs between y = 0 and
    // y = 0.25 * _yRange forever.
    bool init() override;                              // @00649060  vptr+0x4f8
    // Fades the arrow out (0.25s) and then removes this node (fadeOutComplete).
    void fadeOut();                                    // @00649224
    void fadeOutComplete();                            // @00649364  removeFromParent()

protected:
    float _yRange;                                     // +0x2f8  150; initial arrow y
    cocos2d::Sprite* _sprite;                          // +0x300  the arrow
    // iOS: id<HighlightSpriteDelegate> _delegate. Never accessed by the Android port (no
    // HighlightSpriteDelegate type exists in it); kept so arm64 sizeof stays 0x310.
    void* _delegate;                                   // +0x308
};
