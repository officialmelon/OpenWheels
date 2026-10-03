#pragma once

#include "LevelItem.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <map>
#include <vector>

class Arrow;
class LevelDataElement;

// Level item type 0x1d (LevelB2D::addSpecial: new(nothrow) 0x168 + ArrowGun() + init).
// A turret that tracks the closest stabbable body inside its range sensor and shoots Arrows.
//
// XML attributes: p0 x, p1 y, p2 angle (deg), p3 fixed (turret welded to the level body),
// p4 rate of fire (int; framesPerShot = max(46 - 4*p4, 6)), p5 dontShootPlayer (stored, unused).
//
// arm64 sizeof 0x168. Out-of-line constructor. Field names follow the iOS ivars (mc, turret,
// _stringMC, _stringMCFrame, _shouldUpdateFrame, rangeSensor, targetSensor, targetAngle, baseBody,
// targetBody, targetDictionary, anchorJoints, pathClear, aligned, anchorStart, arrows, arrowsLeft,
// framesPerShot, frameCount, retargetCount, _firingAllowed, _unlimitedArrows, _arrowGunType,
// _turretPos, _turretLocalPos, _currentAngle); several are leftovers that Android never uses.
class ArrowGun : public LevelItem
{
public:
    ArrowGun();            // @005801fc
    ~ArrowGun() override;  // @00580288 (D1), @005802e4 (D0)

    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;  // @00580308  vptr+0x18

    void createBody(b2Vec2 position, float angle, bool fixed);  // @005808ec
    void findClosestTarget(b2Vec2 position);                    // @00580cb0  (no callers)
    void targetRemove(b2Fixture* fixture);                      // @00580d38
    void targetRemove2(b2Fixture* fixture);                     // @00580f88  -> targetRemove
    void targetAdd(b2Fixture* fixture);                         // @00580f8c
    void targetAdd2(b2Fixture* fixture);                        // @00581078  -> targetAdd
    Arrow* fireArrow();                                         // @0058107c  retained Arrow, or null
    void advanceStringMC();                                     // @00581314
    void arrowGunDie();                                         // @005819d8
    void arrowBroken(Arrow* arrow);                             // @00581bc4
    void dealloc();                                             // @00581d2c

    // LevelItem overrides
    void paint() override;                                      // @00581a38  vptr+0x28
    void actions() override;                                    // @0058152c  vptr+0x30
    void frameAction() override;                                // @005814d4  vptr+0x40
    b2Body* getJointBody(b2Vec2 point) override;                // @00580ca8  vptr+0x50
    void stopInteractivity() override;                          // @00581ce4  vptr+0x58
    void removeSprites() override;                              // @00581cd0  vptr+0x60
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;  // @00581d38  vptr+0x88
    void endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;    // @00581d40  vptr+0x90
    void paintWithOffsetPoints(cocos2d::Vec2 offset, float angleDegrees) override;               // @00581b48  vptr+0xc8
    void setOpacity(float opacity) override;                    // @00581c40  vptr+0xd0

protected:
    cocos2d::Node* _mc;                       // +0x98  container: base sprite + turret, in the level items node
    cocos2d::Sprite* _turret;                 // +0xa0  "arrow_gun_turret.png"
    cocos2d::Sprite* _stringMC;               // +0xa8  "arrow_gun_string_<n>.png", child of _turret
    unsigned int _stringMCFrame;              // +0xb0  string animation frame (1..4)
    bool _shouldUpdateFrame;                  // +0xb4  toggles every frame (animation at half rate)
    bool _dontShootPlayer;                    // +0xb5  p5; never read
    b2Fixture* _rangeSensor;                  // +0xb8  big sensor box in front of the gun
    b2Fixture* _targetSensor;                 // +0xc0  unused (ctor 0)
    float _targetAngle;                       // +0xc8  unused (ctor 0)
    b2Body* _baseBody;                        // +0xd0  group body passed to init, else own body (type 1)
    b2Body* _targetBody;                      // +0xd8  current target
    std::map<b2Body*, unsigned int> _targetDictionary;   // +0xe0  body -> number of fixtures in range
    std::vector<b2DistanceJoint*> _anchorJoints;         // +0xf8  unused
    bool _pathClear;                          // +0x110 unused (init/ctor 0)
    bool _aligned;                            // +0x111 turret within 0.52 rad of the target
    b2Vec2 _anchorStart;                      // +0x114 unused (ctor b2Vec2_zero)
    std::vector<Arrow*> _arrows;              // +0x120 retained arrows (only with _unlimitedArrows)
    unsigned int _arrowsLeft;                 // +0x138 10; gun shuts down at 0
    unsigned int _framesPerShot;              // +0x13c max(46 - 4*rateOfFire, 6)
    unsigned int _frameCount;                 // +0x140 cooldown, decremented in frameAction()
    unsigned int _retargetCount;              // +0x144 unused (init 0)
    bool _firingAllowed;                      // +0x148 init 1; stopInteractivity() clears it
    bool _unlimitedArrows;                    // +0x149 init 0; never set
    unsigned int _arrowGunType;               // +0x14c 0 = on the level body (fixed), 1 = own body, 2 = on a group body
    b2Vec2 _turretPos;                        // +0x150 world position arrows are fired from
    b2Vec2 _turretLocalPos;                   // +0x158 _turretPos in _baseBody's frame (types 1/2)
    float _currentAngle;                      // +0x160 turret angle, radians (= -p2 * pi/180)
};
