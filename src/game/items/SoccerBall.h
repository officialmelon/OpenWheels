#pragma once

#include "LevelItem.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

class LevelDataElement;

// Level item type 10 (LevelB2D::addSpecial: new(nothrow) SoccerBall() + init). A dynamic circle
// body (radius 0.16 m, density 0.5, friction 0.1, restitution 0.5) with a "soccerball.png" sprite
// as user data. No state: body and sprite are not kept.
//
// XML attributes: p0 x, p1 y.
//
// arm64 sizeof 0x98 (no members of its own). No user-provided constructor (factory
// value-initialises), trivial destructor (vtable slot 0 is LevelItem's D1).
class SoccerBall : public LevelItem
{
public:
    ~SoccerBall() override;  // @00613b24 (D0; D1 is LevelItem's)

    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;  // @006138ac  vptr+0x18
};
