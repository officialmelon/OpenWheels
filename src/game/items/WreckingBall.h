#pragma once

#include "LevelItem.h"
#include "LevelItemsDrawNodeWreckingBallDelegate.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

class LevelDataElement;

// Level item type 7 (LevelB2D::addSpecial: new(nothrow) WreckingBall() + init). A heavy ball on a
// revolute joint to the level body, driven by the joint motor between two angle limits. The chain
// is drawn by the background LevelItemsDrawNode from getPointA()/getPointB().
//
// XML attributes: p0 x, p1 y, p2 chain length (LevelB2D::convertLengthData).
//
// arm64 sizeof 0xe0; LevelItemsDrawNodeWreckingBallDelegate sub-object at +0x98 (its vptr is the
// secondary vtable; getPointA/getPointB also occupy primary slots vptr+0x110/+0x118). User-provided
// constructor, inline in the original (inlined into LevelB2D::addSpecial case 7): it zeroes every
// member. Trivial destructor (vtable slot 0 is LevelItem's D1). Members are the iOS ivars in order
// (mc, ball, body, shape, joint, ballUpperAngle, ballLowerAngle, speed, chainPointA,
// chainPointBOffset).
class WreckingBall : public LevelItem, public LevelItemsDrawNodeWreckingBallDelegate
{
public:
    WreckingBall();             // inlined into LevelB2D::addSpecial @005cff00 (case 7)
    ~WreckingBall() override;   // @00648044 (D0; D1 is LevelItem's)

    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;  // @00647330  vptr+0x18

    void createBody(b2Vec2 position, float length);       // @00647a28
    void createJoint(b2Vec2 anchor);                      // @00647c0c  (no callers; init inlines it)
    cocos2d::Vec2 pointA();                               // @00647d3c  _chainPointA (px)
    cocos2d::Vec2 pointB();                               // @00647d44  ball end of the chain (px)

    // LevelItem overrides
    void paint() override;                                // @00647d18  vptr+0x28
    void actions() override;                              // @00647e9c  vptr+0x30  motor reversal

    // LevelItemsDrawNodeWreckingBallDelegate (primary vptr+0x110/+0x118; thunks @00647db8/@00647e2c)
    cocos2d::Vec2 getPointA() override;                   // @00647db0
    cocos2d::Vec2 getPointB() override;                   // @00647dc0

protected:
    // +0x98: LevelItemsDrawNodeWreckingBallDelegate vptr
    cocos2d::Sprite* _mc;                     // +0xa0  unused
    cocos2d::Node* _ball;                     // +0xa8  container with 2x "wreckingball.png" (rotated in paint())
    b2Body* _body;                            // +0xb0  ball body (circle, density 500)
    b2Fixture* _shape;                        // +0xb8  ball circle
    b2RevoluteJoint* _joint;                  // +0xc0  level body <-> ball, motor + limits
    float _ballUpperAngle;                    // +0xc8  0.785398185
    float _ballLowerAngle;                    // +0xcc  -3.92699099
    float _speed;                             // +0xd0  motor speed, -28 / length; sign flipped at the limits
    cocos2d::Vec2 _chainPointA;               // +0xd4  joint anchor in px
    float _chainPointBOffset;                 // +0xdc  1.52 m: chain end in ball coordinates (0, offset)
};
