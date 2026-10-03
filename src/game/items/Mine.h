#pragma once

#include "LevelItem.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <vector>

class LevelDataElement;

// Level item type 2 (LevelB2D::addSpecial: new(nothrow) Mine() + init). A flat land mine with a
// blinking light; any solved contact on its small trigger fixture (post-solve, no impulse
// threshold) or a trigger action makes it explode (blastBodies + explosion animation).
//
// XML attributes: p0 x, p1 y, p2 angle (deg).
//
// arm64 sizeof 0xc8. No user-provided constructor (factory value-initialises = zero-fill) and a
// trivial destructor (vtable slot 0 is LevelItem's D1). iOS ivars: exploded, mc, light, counter,
// body, sensor, explosion, slowMoDuration (the Android order differs, see offsets).
class Mine : public LevelItem
{
public:
    ~Mine() override;  // @005ee0b4 (D0; D1 is LevelItem's)

    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;  // @005ed34c  vptr+0x18

    void createBody(b2Vec2 position, float angle);        // @005ed660
    void blastBodies(b2Vec2 center, float radius);        // @005edd84
    void animationComplete();                             // @005edf80  CallFunc target

    // LevelItem overrides
    void singleAction() override;                         // @005ed87c  vptr+0x38  explode
    void frameAction() override;                          // @005ed810  vptr+0x40  light blink
    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override;     // @005edfb0  vptr+0xa0
    void triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties) override;  // @005ee00c  vptr+0xd8
    std::vector<b2Body*> getBodyList() override;          // @005ee070  vptr+0xf0

protected:
    b2Body* _body;                            // +0x98  null after the explosion
    cocos2d::Sprite* _mc;                     // +0xa0  "mine.png" (body user data)
    cocos2d::Sprite* _explosion;              // +0xa8  explosion animation sprite
    cocos2d::Sprite* _light;                  // +0xb0  "mine_light.png", child of _mc
    b2Fixture* _sensor;                       // +0xb8  trigger button (post-solve listener)
    bool _exploded;                           // +0xc0
    int _counter;                             // +0xc4  light blink countdown (8 frames)
};
