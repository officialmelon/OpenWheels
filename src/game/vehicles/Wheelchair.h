#pragma once

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include "Vehicle.h"

#include <new>
#include <string>

class CharacterB2D;
class Sound;

// Wheelchair Guy's rocket wheelchair: chair (_frameBody, three postSolve-watched "chair" fixtures),
// a small front wheel and a big motorised back wheel, and a jet (special1) mounted at the back
// axle that pushes the back wheel along the chair's heading. Leaning applies impulse pairs to the
// chair. Smashing the chair (impulse > _frameSmashLimit) ejects the rider, drops the wheels and
// turns jet and fuel tank into separate bodies; a smashed fuel tank explodes (blastBodies) and a
// smashed jet is destroyed.
//
// arm64 sizeof 0x298; own fields 0x1b4..0x298 (the first one sits in Vehicle's tail padding).
// User-provided constructor (all members value-initialised as below) and destructor, both in
// Wheelchair.cpp; create() is inline (kept copy in WheelchairGuy's TU, WheelchairGuy::init
// creates the vehicle).
class Wheelchair : public Vehicle
{
public:
    // @006471a8
    static Wheelchair* create(cocos2d::Vec2 position, std::string name, int groupID)
    {
        Wheelchair* ret = new (std::nothrow) Wheelchair();
        if (ret)
        {
            if (ret->init(position, name, groupID))
            {
                ret->autorelease();
            }
            else
            {
                delete ret;
                ret = nullptr;
            }
        }
        return ret;
    }

    // Besides the member defaults below, the body nulls Vehicle::_frontWheelFixture and
    // Vehicle::_backWheelFixture (Vehicle's constructor leaves them alone).
    Wheelchair();                                                            // @00641c18
    ~Wheelchair() override;                                                  // @00641d34 (D1), @00641d38 (D0)

    // ---- LevelItem overrides ----
    void paint() override;                                                   // @006444c4  vptr+0x28
    void actions() override;                                                 // @00644604  vptr+0x30
    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override;                // @0064484c  vptr+0xa0

    // ---- Vehicle overrides (vtable order) ----
    // _maxSpeed 20, _accelStep 0.1, _maxTorque 20, wheel sound "BikeLoop1"; then Vehicle::init and
    // the mine-explosion sprite frames (fuel tank explosion).
    bool init(cocos2d::Vec2 position, std::string name, int groupID) override;  // @00641d5c  vptr+0x110
    void addCharacter(CharacterB2D* character) override;                     // @00643764  vptr+0x118
    void forwardButtonPressed() override;                                    // @00643ccc  vptr+0x128
    void backButtonPressed() override;                                       // @00643d48  vptr+0x130
    void forwardBackButtonsNull() override;                                  // @00643dc0  vptr+0x138
    void leanForwardButtonPressed() override;                                // @00643e20  vptr+0x140
    void leanBackButtonPressed() override;                                   // @00644034  vptr+0x148
    void leanButtonsNull() override;                                         // @00644250  vptr+0x150
    void special1ButtonPressed() override;                                   // @00644274  vptr+0x158
    void special1ButtonNull() override;                                      // @00644438  vptr+0x160
    bool ejectCharacter(CharacterB2D* character) override;                   // @00644754  vptr+0x170
    void lockWheels() override;                                              // @006431ac  vptr+0x180
    void createSprites() override;                                           // @00641e5c  vptr+0x198
    void createBodies() override;                                            // @006423b0  vptr+0x1a8
    void createJoints() override;                                            // @006431e0  vptr+0x1b8
    void createDictionaries() override;                                      // @006432b4  vptr+0x1e0
    void handleContactResults() override;                                    // @00645028  vptr+0x2b8

    // QOL (PC addition): re-grab vehicle (Vehicle.h).
    b2Body* qolFrameBody() override;
    bool qolCanRemount(CharacterB2D* character) override;
    void qolRemount(CharacterB2D* character) override;

    // ---- non-virtual ----
    // Finish callback of _jetSound.
    void jetSoundStopped();                                                  // @006444bc
    // Smash handlers, called from handleContactResults with the buffered contact.
    void frameSmash(float impulse, b2Vec2 point);                            // @00645644
    void fueltankSmash(float impulse, b2Vec2 point);                         // @006461c0
    void jetSmash(float impulse, b2Vec2 point);                              // @00646824
    // End of the explosion animation (CallFunc bound with std::bind): removes _explosionSprite.
    void fueltankExplosionComplete();                                        // @00646a8c
    // Radial impulse on the bodies around `position` (b2World::QueryAABB with a QueryCallback).
    void blastBodies(b2Vec2 position, float radius);                       // @00646abc

protected:
    float _impulseMagnitudeMax = 0.5f;          // +0x1b4  lean impulse (scaled by s_timeStepOverFlashTimeStep)
    float _impulseOffset = 1.0f;                // +0x1b8  never read (the lean code uses a literal 1)
    float _maxSpinAV = 5.0f;                    // +0x1bc  lean impulses fade out as the chair's spin approaches this
    b2Body* _frontWheelBody = nullptr;          // +0x1c0  "smallWheelShape"; Vehicle::_frontWheelFixture
    b2Body* _backWheelBody = nullptr;           // +0x1c8  "bigWheelShape"; Vehicle::_backWheelFixture; jet sound body
    b2Body* _jetBody = nullptr;                 // +0x1d0  created by frameSmash
    b2Body* _fueltankBody = nullptr;            // +0x1d8  created by frameSmash
    b2Body* _frameBody = nullptr;               // +0x1e0  the chair
    bool _firing = false;                       // +0x1e8  jet on (special1)
    bool _chairSmashed = false;                 // +0x1e9  set by frameSmash
    float _jetAngle = 0.0f;                     // +0x1ec  jet direction relative to the chair
    float _frameSmashLimit = 200.0f;            // +0x1f0  impulse thresholds (LevelItem::_contactImpulseDict)
    float _wheelSmashLimit = 200.0f;            // +0x1f4
    float _jetSmashLimit = 30.0f;               // +0x1f8
    float _fueltankSmashLimit = 0.25f;          // +0x1fc
    b2Fixture* _jetFixture = nullptr;           // +0x200
    b2Fixture* _fueltankFixture = nullptr;      // +0x208
    b2Fixture* _frame1Fixture = nullptr;        // +0x210  "chair1Shape"
    b2Fixture* _frame2Fixture = nullptr;        // +0x218  "chair2Shape"
    b2Fixture* _frame3Fixture = nullptr;        // +0x220  "chair3Shape"
    b2RevoluteJoint* _backWheelJoint = nullptr;   // +0x228  motorised (forward/back buttons, lockWheels)
    b2RevoluteJoint* _frontWheelJoint = nullptr;  // +0x230
    b2RevoluteJoint* _chairPelvisJoint = nullptr; // +0x238  chair - rider's pelvis
    b2RevoluteJoint* _chairChestJoint = nullptr;  // +0x240  chair - rider's chest
    b2RevoluteJoint* _vehicleUpperLeg1 = nullptr; // +0x248  chair - rider's upper leg 1
    b2RevoluteJoint* _vehicleUpperLeg2 = nullptr; // +0x250  chair - rider's upper leg 2
    cocos2d::Sprite* _frameSprite = nullptr;      // +0x258  <name>_frame.png (_frameBody userData)
    cocos2d::Sprite* _frontWheelSprite = nullptr; // +0x260  <name>_frontWheel.png (_frontWheelBody userData)
    cocos2d::Sprite* _backWheelSprite = nullptr;  // +0x268  <name>_wheel.png (_backWheelBody userData)
    cocos2d::Sprite* _jetSprite = nullptr;        // +0x270  <name>_jet.png; nulled by jetSmash
    cocos2d::Sprite* _flameSprite = nullptr;      // +0x278  <name>_flame.png (child of _jetSprite, visible while firing)
    cocos2d::Sprite* _explosionSprite = nullptr;  // +0x280  fuel tank explosion animation
    float _backWheelAngOffset = 0.0f;             // +0x288  subtracted from the back wheel sprite's rotation; never written
    Sound* _jetSound = nullptr;                   // +0x290  "jetBlast2"
};
