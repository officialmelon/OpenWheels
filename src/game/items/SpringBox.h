#pragma once

#include "LevelItem.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

class LevelDataElement;

// Level item type 5 (LevelB2D::addSpecial: new(nothrow) SpringBox() + init). A spring-loaded pad:
// a dynamic pad body on a prismatic joint to the level body. The first post-solve contact arms it
// (_hit); after the delay (p3 seconds, counted down in actions() by LevelItem::s_timeStep and shown
// on a LabelAtlas "fonts/springbox_font.png") the motor fires the pad out (speed 8, upper limit
// _translation) with a "SpringBoxBounce" sound, then retracts (speed -1) and re-arms.
//
// XML attributes: p0 x, p1 y, p2 angle (deg), p3 delay (s).
//
// arm64 sizeof 0xe8. No user-provided constructor (the factory value-initialises: zero-fill) and a
// trivial destructor (vtable slot 0 is LevelItem's D1). Members are the iOS ivars in order (minus
// idCounter and delayTimeString).
class SpringBox : public LevelItem
{
public:
    ~SpringBox() override;  // @006332cc (D0; D1 is LevelItem's)

    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;  // @00632014  vptr+0x18

    // Pad body (user data _pad) with the pad box + two piston stops, and a box on the level body.
    void createBody(b2Vec2 position, float angle);   // @00632a04
    // Same prismatic joint as init builds inline; no callers.
    void createJoint(float angle);                   // @00632c40

    // LevelItem overrides
    void actions() override;                         // @00632d4c  vptr+0x30
    // removePostSolve(_padShape); _hit = true.
    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override;  // @006332a0  vptr+0xa0

protected:
    cocos2d::Sprite* _mc;               // +0x98  "springbox_base.png"
    cocos2d::Sprite* _pad;              // +0xa0  "springbox_top.png" + 2x "springbox_piston.png"
    cocos2d::Sprite* _glow;             // +0xa8  "springbox_arrow.png" (child of _mc), shown from firing until re-armed
    cocos2d::LabelAtlas* _timerText;    // +0xb0  delay countdown (Session label atlas node)
    b2Body* _body;                      // +0xb8  pad body
    b2Fixture* _padShape;               // +0xc0  post-solve listener
    b2PrismaticJoint* _joint;           // +0xc8
    float _translation;                 // +0xd0  0.512f upper joint limit
    bool _hit;                          // +0xd4  set by postSolve: count down, then fire
    int _delayCounter;                  // +0xd8  zeroed by init, never used (iOS: unsigned short)
    int _delayTotal;                    // +0xdc  zeroed by init, never used (iOS: unsigned short)
    float _delayTimeCounter;            // +0xe0  p3, counts down
    float _delayTimeTotal;              // +0xe4  p3
};
