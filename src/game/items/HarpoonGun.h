#pragma once

#include "LevelItem.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <map>
#include <vector>

class Harpoon;
class LevelDataElement;

// Level item type 0xf (LevelB2D::addSpecial -> HarpoonGun::create). A turret on a revolute joint
// that aims at the closest stabbable body in its range sensor and fires one Harpoon, optionally
// tied back to the gun with a rope of 8 distance joints (drawn by LevelItemsDrawNode).
//
// XML attributes: p0 x, p1 y, p2 angle (deg), p3 useAnchor (default true), p4 fixedTurret,
// p5 turretAngle (deg), p6 triggerFiring (fires only from a trigger), p7 disabled.
//
// arm64 sizeof 0x150. The constructor was inline (only visible inlined in create @005d07d4): it
// zeroes every member except _anchorStart. Members are exactly the iOS ivars in iOS order.
class HarpoonGun : public LevelItem
{
public:
    HarpoonGun();               // inlined into create @005d07d4
    ~HarpoonGun() override;     // @005c2f44 (D2), @005c2f90 (D0)

    static HarpoonGun* create(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset);  // @005d07d4
    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;         // @005c0cb4  vptr+0x18

    void setLight();                                                            // @005c12dc
    void createBody(b2Vec2 position, float angle);                              // @005c1514
    void targetAdd(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact);     // @005c1978
    void targetAdd2(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact);    // @005c1c38
    void targetRemove(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact);  // @005c1c64
    void createAnchor();                                                        // @005c1eb4
    void createTargetSensor(float length, b2Vec2 offset, float angle);          // @005c1edc  (only length is used)
    void createAnchorWithEndBody(b2Body* endBody, b2Vec2 endLocalPoint, float length);  // @005c1ff8
    void removeSensors();                                                       // @005c23c0
    void destroyAnchor();                                                       // @005c244c
    std::vector<b2DistanceJoint*> getJoints();                                  // @005c24bc  copy of _anchorJoints
    void findClosestTarget(b2Vec2 position);                                    // @005c25b4  (no callers)
    void fireHarpoon();                                                         // @005c263c  aimed shot
    void fireHarpoon2();                                                        // @005c293c  straight shot (fixed turret / trigger)
    void harpoonHit(Harpoon* harpoon);                                          // @005c2b9c

    // LevelItem overrides
    void die() override;                                                        // @005c194c  vptr+0x20
    void actions() override;                                                    // @005c2c24  vptr+0x30
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;  // @005c1be4  vptr+0x88
    void endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;    // @005c1c60  vptr+0x90
    void triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties) override;  // @005c2ec8  vptr+0xd8

protected:
    cocos2d::Node* _mc;                       // +0x98  container: base sprite + turret
    bool _useAnchor;                          // +0xa0  p3: harpoon tied to the gun with a rope
    cocos2d::Sprite* _turret;                 // +0xa8  "harpoon_turret.png"; light sprite is its child (tag 1)
    Harpoon* _harpoon;                        // +0xb0  retained after firing; released in die()
    cocos2d::Sprite* _anchorSprite;           // +0xb8  unused
    cocos2d::Sprite* _harpoonSprite;          // +0xc0  "harpoon.png" shown in the turret until fired
    b2Fixture* _rangeSensor;                  // +0xc8  on the level body
    b2Fixture* _targetSensor;                 // +0xd0  on _turretBody (fixed turret: line of sight sensor)
    float _targetAngle;                       // +0xd8  unused
    float _targetLength;                      // +0xdc  turret centre -> far end of the range sensor (+0.5)
    b2Body* _baseBody;                        // +0xe0  unused
    b2Body* _turretBody;                      // +0xe8
    b2RevoluteJoint* _turretJoint;            // +0xf0  turret <-> level body
    b2Body* _targetBody;                      // +0xf8
    std::map<b2Body*, unsigned int> _targetDictionary;   // +0x100 body -> fixtures in range
    std::vector<b2DistanceJoint*> _anchorJoints;         // +0x118 rope joints (8)
    bool _pathClear;                          // +0x130 written (init/createBody/createTargetSensor), never read
    bool _aligned;                            // +0x131 turret within 0.52 rad (or target sensor hit)
    float _jointLength;                       // +0x134 rope segment length
    b2Vec2 _anchorStart;                      // +0x138 rope start (world); not initialised by the ctor
    bool _fixedTurret;                        // +0x140 p4
    float _turretAngle;                       // +0x144 p5 in radians (-deg*pi/180)
    bool _triggerFiring;                      // +0x148 p6
    bool _disabled;                           // +0x149 p7; also set by firing / trigger actions 1 and 2
};
