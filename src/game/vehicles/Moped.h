#pragma once

#include "Vehicle.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <map>
#include <new>
#include <string>

class CharacterB2D;
class Sound;

// Moped Couple's scooter: a driver and a passenger on one frame body, two wheels on revolute
// joints (motors driven by the Vehicle base), a boost button (special1) with a refilling meter,
// an engine loop sound whose pitch follows the back wheel speed, and breakable frame/wheels.
//
// Bodies/fixtures come from vehicles/bodies/<name>.plist ("bodies" dict: "engine", "middle",
// "rear", "backWheelShape", "frontWheelShape", "fork", "tank", "seat"); sprites from
// vehicles/<name>_sprites.plist (<name>_frame/_frontWheel/_backWheel/_spokes.png).
//
// arm64 sizeof 0x2e8. Own fields start at 0x1b8. Field names follow the iOS (Objective-C) ivars of
// the same class where they exist; the Android port moved the wheel fixtures into Vehicle
// (+0x100 front, +0x108 back) and the frame body/sprite, the lean constants and the contact-result
// map from iOS Vehicle into this class.
class Moped : public Vehicle
{
public:
    // Inline in the original header (its copy is emitted in MopedCouple's translation unit).
    // @005f4320 (inline)
    static Moped* create(cocos2d::Vec2 position, std::string name, int groupID)
    {
        Moped* moped = new (std::nothrow) Moped;
        if (moped) {
            if (moped->init(position, name, groupID)) {
                moped->autorelease();
            } else {
                delete moped;
                moped = nullptr;
            }
        }
        return moped;
    }

    Moped();             // @005ee190
    ~Moped() override;   // @005ee288 (D1), @005ee2c4 (D0)

    bool init(cocos2d::Vec2 position, std::string name, int groupID) override;  // @005ee2e8 vptr+0x110
    void lockWheels() override;                 // @005ee41c vptr+0x180  locks the back wheel
    void createSprites() override;              // @005ee450 vptr+0x198
    void customizeControls();                   // @005ee958  (no callers)
    void createBodies() override;               // @005ee99c vptr+0x1a8
    void createJoints() override;               // @005ef728 vptr+0x1b8
    void createDictionaries() override;         // @005ef84c vptr+0x1e0
    void frameAction() override;                // @005efe4c vptr+0x40   engine pitch + boost meter
    void addDriver(CharacterB2D* character);    // @005f0010  called by MopedCouple
    void attachCharacter(CharacterB2D* character);  // @005f0018
    void addPassenger(CharacterB2D* character); // @005f0978  called by MopedCouple
    void leg1Stuck();                           // @005f0980
    void leg1Free();                            // @005f0990
    void checkLegsFree();                       // @005f09ac
    void leg2Stuck();                           // @005f0a44
    void leg2Free();                            // @005f0a54
    void singleAction() override;               // @005f0a70 vptr+0x38   passenger legs stop being sensors
    void handleContactResults() override;       // @005f0ae8 vptr+0x2b8

    // QOL (PC addition): re-grab vehicle (Vehicle.h).
    b2Body* qolFrameBody() override;
    bool qolCanRemount(CharacterB2D* character) override;
    void qolRemount(CharacterB2D* character) override;
    void frameSmash(float impulse, b2Vec2 normal);       // @005f0f10
    void frontWheelSmash(float impulse, b2Vec2 normal);  // @005f1b68
    void backWheelSmash(float impulse, b2Vec2 normal);   // @005f1fc0
    void stopEngineSound();                     // @005f2418
    void handleFramePostSolve(VehicleContact contact);   // @005f244c
    void handleUpperLeg1Injury(CharacterB2D* character) override;  // @005f275c vptr+0x278
    void handleUpperLeg2Injury(CharacterB2D* character) override;  // @005f27bc vptr+0x280
    void handleLowerLeg1Injury(CharacterB2D* character) override;  // @005f281c vptr+0x288
    void handleLowerLeg2Injury(CharacterB2D* character) override;  // @005f2868 vptr+0x290
    void checkStateOfCharacter(CharacterB2D* character) override;  // @005f28b4 vptr+0x2a8
    void refilterLegs(unsigned int maskBits);   // @005f2df0
    void ejectAllCharacters() override;         // @005f2f24 vptr+0x178
    bool ejectCharacter(CharacterB2D* character) override;  // @005f2f50 vptr+0x170
    void special1ButtonPressed() override;      // @005f3120 vptr+0x158  boost on
    void special1ButtonNull() override;         // @005f312c vptr+0x160  boost off
    void leanForwardButtonPressed() override;   // @005f3134 vptr+0x140
    void leanBackButtonPressed() override;      // @005f3348 vptr+0x148
    void leanBackPose() override;               // @005f3564 vptr+0x220
    void leanForwardPose() override;            // @005f3620 vptr+0x218
    void noLeanBackPose() override;             // @005f36cc vptr+0x228  (empty)
    void noLeanForwardPose() override;          // @005f36d0 vptr+0x230  (empty)
    void paint() override;                      // @005f36d4 vptr+0x28
    void random();                              // @005f3864  (no callers) frameSmash(10000, 0,0)
    void die() override;                        // @005f3868 vptr+0x20
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;  // @005f389c vptr+0x88
    void endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;    // @005f3938 vptr+0x90
    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override;  // @005f39ec vptr+0xa0
    void debugFunction(int value) override;     // @005f3e1c vptr+0xf8  (empty)

protected:
    CharacterB2D* _driver;                  // +0x1b8  Android-only (iOS: Vehicle::characters[0])
    CharacterB2D* _passenger;               // +0x1c0  Android-only (iOS: additionalCharacter)
    b2Fixture* _forkFixture;                // +0x1c8
    b2Fixture* _tankFixture;                // +0x1d0
    b2Fixture* _engineFixture;              // +0x1d8
    b2Fixture* _middleFixture;              // +0x1e0
    b2Fixture* _rearFixture;                // +0x1e8
    b2Fixture* _seatFixture;                // +0x1f0
    b2Body* _frameBody;                     // +0x1f8
    cocos2d::Sprite* _backWheelSprite;      // +0x200
    cocos2d::Sprite* _frontWheelSprite;     // +0x208
    cocos2d::Sprite* _frontSpokesSprite;    // +0x210
    cocos2d::Sprite* _backSpokesSprite;     // +0x218
    b2RevoluteJoint* _backWheelJoint;       // +0x220
    b2RevoluteJoint* _frontWheelJoint;      // +0x228
    // +0x230..+0x268: never touched by the Android code; names/types from the iOS ivar list
    // (same count and position).
    b2RevoluteJoint* _framePelvis;          // +0x230  unused
    LevelItem* _additionalCharacter;        // +0x238  unused
    b2RevoluteJoint* _vehicleHand3;         // +0x240  unused
    b2RevoluteJoint* _vehicleHand4;         // +0x248  unused
    b2RevoluteJoint* _vehicleLowerLeg3;     // +0x250  unused
    b2RevoluteJoint* _vehicleLowerLeg4;     // +0x258  unused
    b2RevoluteJoint* _vehiclePelvis2;       // +0x260  unused
    unsigned int _previousMaskBits;         // +0x268  driver's leg maskBits before attaching
    unsigned int _leg1Contacts;             // +0x26c  passenger leg 1 contacts with the seat
    unsigned int _leg2Contacts;             // +0x270
    float _wheelSmashLimit;                 // +0x274  ctor: 200
    float _frameSmashLimit;                 // +0x278  ctor: 200
    bool _passengerEjected;                 // +0x27c  init: false
    cocos2d::Sprite* _meterSprite;          // +0x280  unused
    cocos2d::Sprite* _meterBG;              // +0x288  unused
    cocos2d::Sprite* _frameSprite;          // +0x290  iOS Vehicle::frameSprite; frame body user data
    bool _accelerating;                     // +0x298  unused
    bool _boosting;                         // +0x299  init: false
    float _boostVal;                        // +0x29c  init: 0
    unsigned int _boostMax;                 // +0x2a0  init: 50
    unsigned int _boostStepUp;              // +0x2a4  init: 1
    unsigned int _boostImpulse;             // +0x2a8  init: 2
    float _boostStepDown;                   // +0x2ac  init: 0.25
    Sound* _engineSound;                    // +0x2b0  init: null
    float _defaultPitch;                    // +0x2b8  init: 1
    float _targetPitch;                     // +0x2bc
    float _impulseMagnitudeMax;             // +0x2c0  ctor: 0.75 (iOS Vehicle ivar)
    float _impulseOffset;                   // +0x2c4  ctor: 1
    float _maxSpinAV;                       // +0x2c8  ctor: 5
    std::map<b2Fixture*, VehicleContact> _contactResultBufferDict;  // +0x2d0  (iOS Vehicle ivar)
};
