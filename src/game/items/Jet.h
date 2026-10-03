#pragma once

#include "LevelItem.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

class LevelDataElement;
class Sound;

// Level item type 0x1c (LevelB2D::addSpecial: new(nothrow) Jet() + init). A jet engine box that
// pushes itself along its axis while firing and explodes on a hard impact.
//
// XML attributes: p0 x, p1 y, p2 angle (deg), p3 sleeping (starts asleep, fires when woken),
// p4 power (also the sprite scale: p4/10*0.5 + 0.5), p5 fire duration (s; < 1 = forever),
// p6 acceleration time (s), p7 fixed rotation.
//
// arm64 sizeof 0xf8. No user-provided constructor (factory value-initialises = zero-fill) and a
// trivial destructor (vtable slot 0 is LevelItem's D1). The first member sits in LevelItem's tail
// padding (0x94). Members follow the iOS ivars in order.
class Jet : public LevelItem
{
public:
    ~Jet() override;  // @005cd3a8 (D0; D1 is LevelItem's)

    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;  // @005cbf04  vptr+0x18

    void createBody(b2Vec2 position, float angleDegrees, bool sleeping, float fixedRotation, float power);  // @005cc6a0
    void fireEngine();                                    // @005cc858  (no callers)
    void explode();                                       // @005cc910
    void blastBodies(b2Vec2 center, float radius);        // @005cce4c
    void explosionAnimationComplete();                    // @005cd048  CallFunc target
    void stopSoundLoop();                                 // @005cd078  (no callers)

    // LevelItem overrides
    void die() override;                                  // @005cd370  vptr+0x20
    void singleAction() override;                         // @005cc90c  vptr+0x38  -> explode()
    void frameAction() override;                          // @005cd0ac  vptr+0x40
    b2Body* getJointBody(b2Vec2 point) override;          // @005cd368  vptr+0x50
    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override;     // @005cc8a0  vptr+0xa0

protected:
    unsigned int _smashImpulse;               // +0x94  mass * 100; post-solve impulse above it explodes
    cocos2d::Sprite* _mc;                     // +0x98  "jet_body.png"
    cocos2d::Sprite* _flames;                 // +0xa0  "jet_flame.png", child of _mc
    cocos2d::Sprite* _lights;                 // +0xa8  "jet_spin.png", child of _mc
    b2Body* _body;                            // +0xb0  null after explode()
    b2Fixture* _jetShape;                     // +0xb8  post-solve listener
    unsigned int _power;                      // +0xc0  p4 (float -> unsigned)
    float _accelScaler;                       // +0xc4  0..1 thrust ramp
    float _accelStep;                         // +0xc8  ramp increment per step (1 / (fps * p6))
    bool _firing;                             // +0xcc  flames/lights visible
    unsigned int _fireCount;                  // +0xd0  steps fired so far
    int _fireTotal;                           // +0xd4  fps * p5, or -1 = unlimited
    bool _firingAllowed;                      // +0xd8  init 1
    bool _skipFrame;                          // +0xd9  thrust applied every other step
    Sound* _soundLoop;                        // +0xe0  never assigned on Android (only stopped/cleared)
    float _fadeTime;                          // +0xe8  p6 (as float) when > 0
    cocos2d::Sprite* _explosion;              // +0xf0  explosion animation sprite
};
