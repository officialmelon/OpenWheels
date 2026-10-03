#pragma once

#include "LevelItem.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <map>
#include <vector>

class LevelDataElement;
class Sound;

// Light sprite shown on the mine (setLightColor). Values come from the binary's jump table
// (@0041c23e): 1 -> "homingMine_red.png", 2 -> "homingMine_green.png", 3 -> "homingMine_yellow.png",
// 4 -> no frame change (set by explode()). The 0 value is the never-set initial state.
// RE-TODO(@005c3f94): enumerator names are not in the binary; these are descriptive.
enum HomingMineLightColor
{
    HomingMineLightColorRed = 1,      // target in sight, chasing
    HomingMineLightColorGreen = 2,    // idle / target lost
    HomingMineLightColorYellow = 3,   // fuse counting down (blinks, see frameAction)
    HomingMineLightColorNone = 4,     // exploded
};

// Level item type 0x19 (LevelB2D::addSpecial: new(nothrow) 0x140 + HomingMine() + init).
// A hovering mine that steers toward the closest stabbable body in its range sensor, starts a
// fuse when close (or touched) and explodes (blastBodies + explosion animation).
//
// XML attributes: p0 x, p1 y, p2 seek speed (stored * 0.01; the pre-read default 0.01 is also
// scaled, so a missing p2 gives 0.0001), p3 fuse time in seconds (no default: _counter is not
// initialised by the ctor and the object is not zero-filled).
//
// arm64 sizeof 0x140. Out-of-line constructor; the factory does not zero-fill, and the ctor only
// clears the sprites, bodies/fixtures, the map, _inContact and the two sounds (init sets
// _countingDown, _explosionDistance, _seekSpeed). Members follow the iOS ivars (mc, light, jet1..4,
// explosionDistance, mineBody, target, mineShape, rangeSensor, targetDictionary, pathClear,
// inContact, countingDown, skipAFrame, counter, retargetCount, seekSpeed, totalImpulseVector,
// _mineBodyMassInv, explosion, hoverLoop, beepLoop, _lightColor); iOS's sensorRadius is a TU
// static on Android (600 / ptmRatio), _lightColor moved up next to the flags.
class HomingMine : public LevelItem
{
public:
    HomingMine();             // @005c3034
    ~HomingMine() override;   // @005c3094 (D1), @005c30d0 (D0)

    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;  // @005c30f4  vptr+0x18

    void createBodies(b2Vec2 position);                   // @005c37c0
    void findClosestTarget();                             // @005c3964  (no callers; actions() inlines it)
    void explode();                                       // @005c39f8
    void setLightColor(HomingMineLightColor color);       // @005c3f94
    void killBeepLoop();                                  // @005c4100
    void killHoverLoop();                                 // @005c4134
    void blastBodies(b2Vec2 center, float radius);        // @005c4168
    void explosionComplete();                             // @005c44e0  CallFunc target after the animation

    // LevelItem overrides
    void paint() override;                                // @005c4dd8  vptr+0x28
    void actions() override;                              // @005c4610  vptr+0x30
    void singleAction() override;                         // @005c4dac  vptr+0x38
    void frameAction() override;                          // @005c45a0  vptr+0x40
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;  // @005c5030  vptr+0x88
    void endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;    // @005c529c  vptr+0x90
    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override;     // @005c54ec  vptr+0xa0
    void triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties) override;  // @005c5024  vptr+0xd8  -> singleAction()

protected:
    cocos2d::Sprite* _mc;                     // +0x98  "homingMine.png"
    cocos2d::Sprite* _light;                  // +0xa0  child of _mc, frame per _lightColor
    cocos2d::Sprite* _jet1;                   // +0xa8  "homingMine_jet.png" x4, children of _mc,
    cocos2d::Sprite* _jet2;                   // +0xb0  scaled by the thrust in paint()
    cocos2d::Sprite* _jet3;                   // +0xb8
    cocos2d::Sprite* _jet4;                   // +0xc0
    cocos2d::Sprite* _explosion;              // +0xc8  explosion animation sprite (removed when done)
    float _explosionDistance;                 // +0xd0  squared trigger distance, (30 / ptmRatio)^2
    b2Body* _mineBody;                        // +0xd8  null after explode()
    b2Body* _target;                          // +0xe0
    b2Fixture* _mineShape;                    // +0xe8  post-solve listener
    b2Fixture* _rangeSensor;                  // +0xf0  begin/end contact listener
    std::map<b2Body*, unsigned int> _targetDictionary;   // +0xf8  body -> fixtures in range
    HomingMineLightColor _lightColor;         // +0x110 not initialised by the ctor
    bool _pathClear;                          // +0x114 previous raycast reached the target
    bool _inContact;                          // +0x115 touched a stabbable body (post-solve)
    bool _countingDown;                       // +0x116 fuse running
    bool _skipAFrame;                         // +0x117 frameAction toggle (yellow light blink)
    float _counter;                           // +0x118 p3: fuse seconds left
    float _retargetCount;                     // +0x11c seconds since the last retarget (1/6 s)
    float _seekSpeed;                         // +0x120 p2 * 0.01 (steering impulse scale)
    b2Vec2 _totalImpulseVector;               // +0x124 last steering impulse (jets in paint())
    float _mineBodyMassInv;                   // +0x12c 1 / mass
    Sound* _hoverLoop;                        // +0x130 "MineHover"
    Sound* _beepLoop;                         // +0x138 "HomingMineBeep"
};
