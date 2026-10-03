#pragma once

#include "LevelItem.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

class LevelDataElement;

// Level item type 0x1f (LevelB2D::addSpecial: new(nothrow) Token() + init). A stub on Android 1.1.3:
// init() loads the level-items sprite frames (LevelItemTextureId 0), reads p0, p1, p2 into locals
// that are never used, and returns true. Nothing is created. (Identical code to the other stubs
// among Token, Chain and SlowMotionPanel.)
//
// arm64 sizeof 0x98 (no members of its own). No user-provided constructor (factory
// value-initialises), trivial destructor (vtable slot 0 is LevelItem's D1).
// iOS Token had real behaviour (ivars mc, frames, frameIndex, updateAnimation, shape); the Android port dropped it.
class Token : public LevelItem
{
public:
    ~Token() override;  // @00636ed0 (D0; D1 is LevelItem's)

    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;  // @00636d84  vptr+0x18
};
