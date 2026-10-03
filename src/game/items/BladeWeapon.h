#pragma once

#include "LevelItem.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <map>
#include <vector>

class LevelDataElement;
class Sound;

// Level item type 0x22 (LevelB2D::addSpecial: new(nothrow) BladeWeapon() + init). A loose blade
// (knife, axe, sword, ... 12 types) that impales bodies its blade sensor touches, like Arrow.
//
// XML attributes: p0 x, p1 y, p2 angle (deg), p3 flipped, p4 sleeping, p5 interactive (default
// true; false = sprite only), p6 bladeType (1..12, default 1) -> "blade_<type>.png".
// Blade and handle boxes come from two per-type cocos2d::Rect tables (TU statics, metres).
//
// arm64 sizeof 0x158. No user-provided constructor: the factory's value-initialisation zero-fills
// the object, then the containers are constructed. The first member sits in LevelItem's tail
// padding (0x94). Members follow the iOS ivars (+ Android's _solidSound).
class BladeWeapon : public LevelItem
{
public:
    ~BladeWeapon() override;  // @00586b24 (D2), @00586b80 (D0)

    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;  // @00584cdc  vptr+0x18

    void addBlood();                                  // @00585938
    void createPrisJoint(b2Body* body);               // @0058603c
    void removeJoint(b2Body* body);                   // @00586494
    void fleshSoundStopped();                         // @005867c0
    void solidSoundStopped();                         // @005867c8
    void killSounds();                                // @005867d0
    float getOpacity();                               // @00586a50  1.0 if fully opaque, else 0.0

    // LevelItem overrides
    void actions() override;                          // @00585fb8  vptr+0x30
    b2Body* getJointBody(b2Vec2 point) override;      // @00585fb0  vptr+0x50
    void stopInteractivity() override;                // @00586a84  vptr+0x58
    void removeSprites() override;                    // @00586b10  vptr+0x60
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;  // @0058572c  vptr+0x88
    void endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;    // @00585c1c  vptr+0x90
    void jointWillBeDestroyed(b2Joint* joint) override;               // @00586670  vptr+0xa8
    void fixtureWillBeDestroyed(b2Fixture* fixture) override;         // @00585e60  vptr+0xb0
    void paintWithOffsetPoints(cocos2d::Vec2 offset, float angleDegrees) override;   // @00586804  vptr+0xc8
    void setOpacity(float opacity) override;                          // @005869f0  vptr+0xd0
    void triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties) override;  // @00586880  vptr+0xd8

protected:
    unsigned int _stabCount;                  // +0x94  blood stage counter (max 6)
    bool _bloodied;                           // +0x98  set when the second blood sprite is shown
    int _bladeType;                           // +0x9c  p6 (1..12)
    cocos2d::Sprite* _mc;                     // +0xa0  "blade_<type>.png"
    cocos2d::Sprite* _bloodSprite;            // +0xa8  "blade_blood_<1|2>_<type>.png", child of _mc
    b2Body* _weaponBody;                      // +0xb0  own body, or the group body passed to init
    b2Fixture* _sensorShape;                  // +0xb8  blade sensor (begin/end contact listener)
    b2Fixture* _solidShape;                   // +0xc0  blade, solid (category 8)
    b2Fixture* _handleShape;                  // +0xc8
    b2Body* _previousBody;                    // +0xd0  last body a joint was made to
    float _bladeAngle;                        // +0xd8  blade direction relative to the body, radians
    Sound* _fleshSound;                       // +0xe0  "BladeFlesh<n>"
    Sound* _solidSound;                       // +0xe8  only ever cleared
    unsigned short _stabbableMaterials;       // +0xf0  init 2 (flesh)
    float _weaponAngleOffset;                 // +0xf4  unused
    b2Vec2 _bloodOffset;                      // +0xf8  holds the group offset (init with a group body); unused otherwise
    b2Vec2 _bladeOffset;                      // +0x100 blade centre in body coordinates (joint anchor)
    b2Vec2 _weaponPosOffset;                  // +0x108 unused
    std::vector<b2Body*> _bodiesToAdd;        // +0x110
    std::vector<b2Body*> _bodiesToRemove;     // +0x128
    std::map<b2Body*, b2PrismaticJoint*> _bjDictionary;  // +0x140
};
