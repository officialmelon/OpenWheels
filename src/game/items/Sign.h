#pragma once

#include "LevelItem.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

class LevelDataElement;

// Level item type 0x17 (LevelB2D::addSpecial: new(nothrow) Sign() + init). A decorative road sign,
// sprites only (no physics). Sign types: 1..4 "sign_arrow.png" (tag 0) on a green board (1 arrow
// unrotated on "sign_back_green_h.png", 2 rotated 90 on "_v", 3 rotated 180 on "_h", 4 rotated -90
// on "_v"), 5 "sign_slow.png", 6 "sign_stop.png", >= 7 "sign_back_yellow.png" +
// "sign_decal_<type>.png" (tag 0); optional "sign_post.png" (tag 1, z -1). Type 0 takes the arrow
// path without creating a board and then uses the null _mc (would crash; never used by levels).
// RE-TODO(@00612f70): confirm type 0 behaviour in the asm.
//
// XML attributes: p0 x, p1 y, p2 angle (deg), p3 sign type (int), p4 show post.
//
// arm64 sizeof 0xa0. No user-provided constructor (factory value-initialises), trivial destructor
// (vtable slot 0 is LevelItem's D1). iOS ivar: mc.
class Sign : public LevelItem
{
public:
    ~Sign() override;  // @00613718 (D0; D1 is LevelItem's)

    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;  // @00612d28  vptr+0x18

    void addSpritesWithSignType(unsigned int signType, cocos2d::Vec2 positionPoints,
                                float rotation, bool showPost);   // @00612f70
    // 1.0f when the sign is fully opaque, else 0.0f. No callers.
    float getOpacity();                                           // @00613654

    // LevelItem overrides
    void removeSprites() override;                                // @00613688  vptr+0x60
    void paintWithOffsetPoints(cocos2d::Vec2 offset, float angleDegrees) override;  // @0061369c  vptr+0xc8
    // _mc and its children with tag 1 (post) and 0 (decal/arrow) get opacity (int)(opacity * 255).
    void setOpacity(float opacity) override;                      // @00613590  vptr+0xd0

protected:
    cocos2d::Sprite* _mc;   // +0x98  sign board (LevelItemsNode child)
};
