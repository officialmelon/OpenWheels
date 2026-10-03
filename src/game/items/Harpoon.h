#pragma once

#include "LevelItem.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <map>
#include <vector>

class HarpoonGun;
class Sound;

// The projectile fired by HarpoonGun (fireHarpoon / fireHarpoon2). Same stabbing scheme as Arrow:
// a front sensor collects bodies (beginContact/endContact), actions() turns them into prismatic
// joints (createPrisJoint) or removes them (removeJoint).
//
// arm64 sizeof 0x130 (Harpoon::create @005bf4e0). Inline constructor (zeroes all members except
// _stabbableMaterials). iOS ivars: mc, blood, bloodCount, bloodComplete, harpoonBody, sensorShape,
// previousBody, bodiesToAdd, bodiesToRemove, bjDictionary, stabbableMaterials, fleshSound,
// solidSound (+ Android adds _harpoonGun).
class Harpoon : public LevelItem
{
public:
    Harpoon();              // inlined into create @005bf4e0
    ~Harpoon() override;    // @005c0abc (D2), @005c0b2c (D0); nulls its pointers first

    static Harpoon* create(b2Vec2 position, float angle, b2Vec2 velocity, int zOrder);  // @005bf4e0
    bool init(b2Vec2 position, float angle, b2Vec2 velocity, int zOrder);               // @005bf5cc

    void createBody(b2Vec2 position, float angle, b2Vec2 velocity);   // @005bf7b0
    void setHarpoonGun(HarpoonGun* harpoonGun);                       // @005bf9cc
    b2Body* getHarpoonBody();                                         // @005bf9d4
    void addBlood();                                                  // @005bfbe0
    void createPrisJoint(b2Body* body);                               // @005c039c
    void removeJoint(b2Body* body);                                   // @005c08d0
    void fleshSoundStopped();                                         // @005c0aac
    void solidSoundStopped();                                         // @005c0ab4

    // LevelItem overrides
    void actions() override;                                          // @005c0318  vptr+0x30
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;  // @005bf9dc  vptr+0x88
    void endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;    // @005c00d8  vptr+0x90
    void jointWillBeDestroyed(b2Joint* joint) override;               // @005bff88  vptr+0xa8
    void fixtureWillBeDestroyed(b2Fixture* fixture) override;         // @005bfe38  vptr+0xb0

protected:
    cocos2d::Sprite* _mc;                     // +0x98  "harpoon.png"
    cocos2d::Sprite* _blood;                  // +0xa0  "harpoon_blood_<n>.png", child of _mc
    int _bloodCount;                          // +0xa8  stabs so far (new blood sprite at 0 and 3)
    bool _bloodComplete;                      // +0xac  init 0; never set
    HarpoonGun* _harpoonGun;                  // +0xb0  owner until the first joint (harpoonHit), then null
    b2Body* _harpoonBody;                     // +0xb8
    b2Fixture* _sensorShape;                  // +0xc0  front sensor (begin/end contact listener)
    b2Body* _previousBody;                    // +0xc8  unused
    std::vector<b2Body*> _bodiesToAdd;        // +0xd0
    std::vector<b2Body*> _bodiesToRemove;     // +0xe8
    std::map<b2Body*, b2PrismaticJoint*> _bjDictionary;  // +0x100
    unsigned short _stabbableMaterials;       // +0x118 material mask, init 2 (flesh)
    Sound* _fleshSound;                       // +0x120 "HarpoonFlesh<n>"
    Sound* _solidSound;                       // +0x128 "HarpoonSolid<n>"
};
