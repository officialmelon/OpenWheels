#pragma once

#include "LevelItem.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <vector>

class LevelDataElement;
class Sound;

// Level item type 8 (LevelB2D::addSpecial: new(nothrow) Fan() + init). A fan on the level body
// with a sensor box (half extents 2.4 x 4.0 m) in front of it: each frame every dynamic body
// inside gets ApplyForceToCenter(15 * (d * 0.25)^2 * (-sin, cos)), not scaled by mass, where d is
// min(4 + offset of its world centre along the axis, 4) clamped at 0 (frameAction).
// Blade animation "fan_blade_1/3/5.png", a looping position sound.
// Trigger: prepareForTrigger stops it, the first triggerSingleActivation starts it.
//
// XML attributes: p0 x, p1 y, p2 angle (deg).
//
// arm64 sizeof 0x108. No user-provided constructor (factory value-initialises: zero-fill, then the
// two vectors are constructed); the destructor only destroys the vectors. Members are the iOS ivars
// in order.
class Fan : public LevelItem
{
public:
    ~Fan() override;  // @005adf90 (D1), @005adfe0 (D0)

    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;  // @005ad010  vptr+0x18

    // Sensor box (sets _centerMeters), the fan box, and the 7-vertex fan housing polygon, all on the
    // level body.
    void createBodies(b2Vec2 position, float angle);  // @005ad7e8
    // Same force loop as frameAction (which inlines it); no callers.
    void blowBodies();                                // @005adcc4

    // LevelItem overrides
    void frameAction() override;                      // @005adbb0  vptr+0x40
    // ONLINE (PC addition): with browser physics, Flash's Fan.actions: once per world step, the
    // bodies whose contact with the sensor persisted in that step (Box2D 2.0's Persist calls).
    void actions() override;
    // Remember other->GetBody() (once) unless the other fixture is a sensor / forget it.
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;  // @005add7c  vptr+0x88
    void endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;    // @005adf20  vptr+0x90
    void triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties) override;  // @005adab8  vptr+0xd8
    void prepareForTrigger() override;                // @005ada68  vptr+0xe8

protected:
    cocos2d::Sprite* _mc;                        // +0x98  "fan_base.png" (+ 2x "fan_cover.png" children)
    cocos2d::Sprite* _fanBlade;                  // +0xa0  child of _mc
    std::vector<cocos2d::SpriteFrame*> _frames;  // +0xa8  "fan_blade_1.png", "_3", "_5" (odd i of 1..6)
    int _frameIndex;                             // +0xc0
    float _angle;                                // +0xc4  never used (iOS ivar)
    float _sinVal;                               // +0xc8  sin(-angle)
    float _cosVal;                               // +0xcc  cos(-angle)
    std::vector<b2Body*> _bodies;                // +0xd0  bodies inside the sensor
    b2Vec2 _centerMeters;                        // +0xe8  sensor centre (position + 4.8 m along the axis)
    b2Fixture* _sensor;                          // +0xf0  begin/end contact listener
    bool _skipFrame;                             // +0xf8  toggled every frame
    Sound* _fanSound;                            // +0x100 SoundController::createPositionSound at _centerMeters
};
