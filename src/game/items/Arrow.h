#pragma once

#include "LevelItem.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <map>
#include <vector>

class ArrowGun;
class Sound;

// An arrow fired by ArrowGun::fireArrow(). Dynamic body with a tip sensor (_sensorShape) and a
// shaft (_solidShape). When the tip enters a non-static, stabbable body the arrow is attached to
// it with a prismatic joint (createPrisJoint); a hard hit on the shaft (postSolve impulse > 1)
// breaks the arrow into two sprites (_piece1/_piece2) that fall off-screen.
//
// arm64 sizeof 0x148 (Arrow::createWithPos @0057d494). Out-of-line constructor.
// iOS ivar names (ObjC original): arrowMC, _piece1, _piece2, _piece1Speed, _piece2Speed,
// _piece1Rotation, _piece2Rotation, arrowBody, sensorShape, solidShape, previousBody,
// bodiesToAdd, bodiesToRemove, bjDictionary, fleshSound, solidSound, _delegate.
class Arrow : public LevelItem
{
public:
    Arrow();            // @0057d314
    ~Arrow() override;  // @0057d380 (D1), @0057d470 (D0)

    // new + init + autorelease (init's result is not checked).
    static Arrow* createWithPos(b2Vec2 position, float angle, b2Vec2 velocity, int zOrder);  // @0057d494
    bool init(b2Vec2 position, float angle, b2Vec2 velocity, int zOrder);                     // @0057d544

    void killSounds();                                                    // @0057d424
    void setArrowGun(ArrowGun* arrowGun);                                 // @0057d730
    void createBody(b2Vec2 position, float angle, b2Vec2 velocity);       // @0057d738
    void remoteBreak();                                                   // @0057dfe0
    void arrowDie();                                                      // @0057e03c
    void createPiecesWithVelocity(b2Vec2 velocity);                       // @0057e0e8
    void createPrisJoint(b2Body* body);                                   // @0057e7d8
    void removeJoint(b2Body* body);                                       // @0057ef10
    void fleshSoundStopped();                                             // @0057f454
    void solidSoundStopped();                                             // @0057f45c
    void arrowResultHandler();                                            // @0057f4d8 (empty)

    // LevelItem overrides
    void actions() override;                                              // @0057e500  vptr+0x30
    void singleAction() override;                                         // @0057f0ec  vptr+0x38
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;  // @0057dbbc  vptr+0x88
    void endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;    // @0057dda4  vptr+0x90
    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override;             // @0057f464  vptr+0xa0
    void jointWillBeDestroyed(b2Joint* joint) override;                   // @0057da6c  vptr+0xa8
    void fixtureWillBeDestroyed(b2Fixture* fixture) override;             // @0057d91c  vptr+0xb0

protected:
    cocos2d::Sprite* _arrowMC;            // +0x98  "arrow_1.png"; replaced by "arrow_<1..5>.png" on a flesh hit
    cocos2d::Sprite* _piece1;             // +0xa0  broken halves (createPiecesWithVelocity), null when gone
    cocos2d::Sprite* _piece2;             // +0xa8
    cocos2d::Vec2 _piece1Speed;           // +0xb0  px per step; ctor = Vec2::ZERO
    cocos2d::Vec2 _piece2Speed;           // +0xb8
    float _piece1Rotation;                // +0xc0  deg per step (actions() applies it to BOTH pieces)
    float _piece2Rotation;                // +0xc4  set but never read
    ArrowGun* _arrowGun;                  // +0xc8  init() clears it; nothing in the binary sets it
    b2Body* _arrowBody;                   // +0xd0  null once destroyed
    b2Fixture* _sensorShape;              // +0xd8  tip sensor (begin/end contact listener)
    b2Fixture* _solidShape;               // +0xe0  shaft (post-solve listener after the first joint)
    b2Body* _previousBody;                // +0xe8  last body a joint was made to
    std::vector<b2Body*> _bodiesToAdd;    // +0xf0  filled by beginContact, consumed by actions()
    std::vector<b2Body*> _bodiesToRemove; // +0x108 filled by endContact, consumed by actions()
    std::map<b2Body*, b2PrismaticJoint*> _bjDictionary;  // +0x120 body -> joint holding the arrow in it
    Sound* _fleshSound;                   // +0x138 "ArrowFlesh<n>" body sound
    Sound* _solidSound;                   // +0x140 "ArrowSolid<n>" position sound
};
