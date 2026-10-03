#pragma once

#include "LevelItem.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <vector>

class LevelDataElement;

// Level item type 0x14 (LevelB2D::addSpecial: new(nothrow) Bottle() + init). A glass bottle
// ("bottle_<colorId>.png"): a 0.16 x 0.464 m dynamic box (density 2) when interactive. A post-solve
// impulse > 1.78 queues singleAction: "GlassLight<1|2>" position sound, a coloured bottle burst
// (colour by _colorId: 2 (102,204,255,102), 3 (100,13,18,114), 4 (240,222,83,114), else
// (86,131,65,204)), the body is destroyed.
//
// XML attributes: p0 x, p1 y, p2 angle (deg), p3 colorId (int, default 1), p4 sleeping,
// p5 interactive (no body when false).
//
// arm64 sizeof 0xa8. No user-provided constructor (factory value-initialises), trivial destructor
// (vtable slot 0 is LevelItem's D1). Members are the iOS ivars (_body, _colorId).
class Bottle : public LevelItem
{
public:
    ~Bottle() override;  // @00588e50 (D0; D1 is LevelItem's)

    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;  // @005884f0  vptr+0x18

    // LevelItem overrides
    void singleAction() override;                                    // @00588a74  vptr+0x38  shatter
    b2Body* getJointBody(b2Vec2 point) override;                     // @00588ce4  vptr+0x50  _body
    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override;        // @00588a10  vptr+0xa0
    void triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties) override;  // @00588cec  vptr+0xd8
    std::vector<b2Body*> getBodyList() override;                     // @00588ca0  vptr+0xf0

protected:
    b2Body* _body;   // +0x98  nullptr when not interactive / after shattering
    int _colorId;    // +0xa0  p3, default 1
};
