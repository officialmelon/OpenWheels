#pragma once

#include "LevelItem.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

class LevelDataElement;

// Level item type 0x1e (LevelB2D::addSpecial: new(nothrow) Chain() + init). A stub on Android 1.1.3:
// init() loads the level-items sprite frames (LevelItemTextureId 0), reads p0, p1, p2 into locals
// that are never used, and returns true. Nothing is created. (Identical code to the other stubs
// among Token, Chain and SlowMotionPanel.)
//
// arm64 sizeof 0x98 (no members of its own). No user-provided constructor (factory
// value-initialises), trivial destructor (vtable slot 0 is LevelItem's D1).
// iOS Chain had real behaviour (ivars _chainCount, _bodies, _joints, mc, _breakLimit, _frameCounter, aBody); the Android port dropped it.
class Chain : public LevelItem
{
public:
    ~Chain() override;  // @00589594 (D0; D1 is LevelItem's)

    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;  // @00589448  vptr+0x18
};
