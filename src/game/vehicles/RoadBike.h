#pragma once

#include "Vehicle.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <map>
#include <new>
#include <string>

class CharacterB2D;
class Sound;

// Irresponsible Dad's bicycle with the child seat. The dad's feet ride the pedal gear body, which
// a b2GearJoint couples (ratio -1) to the back wheel; both wheel joints get the drive motor. The
// child sits in a seat body on a breakable revolute joint (_childSeatBreakLimit), held by four
// joints of its own (seatChest/seatPelvis/seatUpperLeg1/2). Frame and wheels are smashable.
//
// Bodies from vehicles/bodies/<name>.plist ("bodies", "joints"/"frameSeatAnchor", ...), sprites
// <name>_wheel/_frame/_gear/_childSeat.png.
//
// arm64 sizeof 0x870 (align 16: it embeds a cocos2d::Sprite by value). Own fields start at 0x1b4,
// in Vehicle's tail padding (Vehicle dsize 0x1b4). This class folds the iOS Bike base class into
// itself (iOS: RoadBike : Bike : Vehicle); names follow the iOS Bike/RoadBike ivars. The Android
// port moved the wheel fixtures into Vehicle (+0x100 front, +0x108 back) and the frame body/sprite,
// rider joints, lean constants and contact-result map from iOS Vehicle into this class.
//
// The ctor and create() were inline in the original header (both are emitted in
// IrresponsibleDad's translation unit). The ctor leaves _dad and _destroyFrame uninitialised and
// assigns _seatChest = _seatPelvis = _seatUpperLeg1 = _seatUpperLeg2 (the last one is itself
// uninitialised; RoadBike is created without zero-fill).
class RoadBike : public Vehicle
{
public:
    // @005cb794 (inline; copy emitted in IrresponsibleDad's translation unit)
    static RoadBike* create(cocos2d::Vec2 position, std::string name, int groupID)
    {
        RoadBike* roadBike = new (std::nothrow) RoadBike;
        if (roadBike) {
            if (roadBike->init(position, name, groupID)) {
                roadBike->autorelease();
            } else {
                delete roadBike;
                roadBike = nullptr;
            }
        }
        return roadBike;
    }

    // @005cbc08 (inline; copy emitted in IrresponsibleDad's translation unit)
    // Leaves _dad and _destroyFrame uninitialised. The seat joints are all assigned the (itself
    // uninitialised) value of _seatUpperLeg2, as in the original.
    RoadBike()
        : _frameAnchor(0.445f, 1.405f),
          _forkAnchor(-1.29f, 1.46f),
          _seatAnchor(1.14f, 3.18f),
          _brokenFrameAnchor(0.55f, 1.88f),
          _frameSmashLimit(200.0f),
          _wheelSmashLimit(200.0f),
          _impulseMagnitudeMax(3.0f),
          _impulseOffset(1.0f),
          _maxSpinAV(5.0f),
          _childEjected(false),
          _frontWheelSprite(nullptr),
          _backWheelSprite(nullptr),
          _frameSprite(nullptr),
          _gearSprite(nullptr),
          _seatBody(nullptr),
          _seatJoint(nullptr),
          _seatSprite(nullptr),
          _seatDetached(false),
          _childGroupIndex(0),
          _child(nullptr),
          _childPreviousMaskBits(0),
          _childSeatBreakLimit(400.0f),
          _frontWheelAngOffset(0.0f),
          _backWheelAngOffset(0.0f),
          _backWheelBody(nullptr),
          _frontWheelBody(nullptr),
          _gearBody(nullptr),
          _frameBody(nullptr),
          _frame1Fixture(nullptr),
          _frame2Fixture(nullptr),
          _frame3Fixture(nullptr),
          _vehicleHand1(nullptr),
          _vehicleHand2(nullptr),
          _vehicleLowerLeg1(nullptr),
          _vehicleLowerLeg2(nullptr),
          _vehiclePelvis(nullptr),
          _backWheelJoint(nullptr),
          _frontWheelJoint(nullptr),
          _frameGearJoint(nullptr),
          _gearJoint(nullptr),
          _previousMaskBits(0),
          _frontWheelSound(nullptr),
          _backWheelSound(nullptr),
          _frameSound(nullptr)
    {
        _frontWheelFixture = nullptr;
        _backWheelFixture = nullptr;
        _seatChest = _seatPelvis = _seatUpperLeg1 = _seatUpperLeg2;
    }
    ~RoadBike() override;   // @0060ea28 (D2), @0060ea6c (D0)

    bool init(cocos2d::Vec2 position, std::string name, int groupID) override;  // @006092a0 vptr+0x110
    void createSprites() override;              // @00609390 vptr+0x198
    void createFilters() override;              // @006098b4 vptr+0x1a0  (empty)
    void createBodies() override;               // @006098b8 vptr+0x1a8
    void lockWheels() override;                 // @0060a768 vptr+0x180  locks the back wheel
    void createJoints() override;               // @0060a79c vptr+0x1b8
    void setLimits() override;                  // @0060ab3c vptr+0x1d0  (empty)
    void addContactListeners() override;        // @0060ab40 vptr+0x1d8  (empty)
    void createDictionaries() override;         // @0060ab44 vptr+0x1e0
    void addDad(CharacterB2D* character);       // @0060b094  called by IrresponsibleDad
    void refilterLegs(unsigned int maskBits);   // @0060b78c
    void addKid(CharacterB2D* character);       // @0060b8bc  called by IrresponsibleDad
    void checkStateOfCharacter(CharacterB2D* character) override;  // @0060bba4 vptr+0x2a8
    bool ejectCharacter(CharacterB2D* character) override;         // @0060bd58 vptr+0x170
    void refilterShit(b2Fixture* fixture);      // @0060c0f0
    void leanBackPose() override;               // @0060c114 vptr+0x220
    void leanForwardPose() override;            // @0060c1f0 vptr+0x218
    void noLeanBackPose() override;             // @0060c2bc vptr+0x228  (empty)
    void noLeanForwardPose() override;          // @0060c2c0 vptr+0x230  (empty)
    void actions() override;                    // @0060c2c4 vptr+0x30   Vehicle::actions()
    void paint() override;                      // @0060c2c8 vptr+0x28
    void checkJoints() override;                // @0060c3f4 vptr+0x188  seat joint break check
    void detachSeat();                          // @0060c440
    void frameSmash(float impulse);             // @0060c73c
    void frontWheelSmash(float impulse, b2Vec2 normal);  // @0060d2e0
    void backWheelSmash(float impulse, b2Vec2 normal);   // @0060d73c
    void forwardButtonPressed() override;       // @0060db9c vptr+0x128
    void backButtonPressed() override;          // @0060dc60 vptr+0x130
    void forwardBackButtonsNull() override;     // @0060dd20 vptr+0x138
    void special1ButtonPressed() override;      // @0060ddbc vptr+0x158
    void leanBackButtonPressed() override;      // @0060de10 vptr+0x148
    void leanForwardButtonPressed() override;   // @0060df70 vptr+0x140
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;  // @0060e0c0 vptr+0x88
    void endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;    // @0060e0f8 vptr+0x90
    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override;  // @0060e124 vptr+0xa0
    void handleContactResults() override;       // @0060e4cc vptr+0x2b8

    // QOL (PC addition): re-grab vehicle (Vehicle.h).
    b2Body* qolFrameBody() override;
    bool qolCanRemount(CharacterB2D* character) override;
    void qolRemount(CharacterB2D* character) override;
    void qolRemountResetBodies(std::vector<b2Body*>* bodies) override;
    void debugFunction(int value) override;     // @0060ea24 vptr+0xf8   detachSeat()

protected:
    cocos2d::Vec2 _frameAnchor;             // +0x1b4  ctor: (0.445, 1.405)
    cocos2d::Vec2 _forkAnchor;              // +0x1bc  ctor: (-1.29, 1.46)
    cocos2d::Vec2 _seatAnchor;              // +0x1c4  ctor: (1.14, 3.18)
    cocos2d::Vec2 _brokenFrameAnchor;       // +0x1cc  ctor: (0.55, 1.88)
    float _frameSmashLimit;                 // +0x1d4  ctor: 200
    float _wheelSmashLimit;                 // +0x1d8  ctor: 200
    float _impulseMagnitudeMax;             // +0x1dc  ctor: 3 (iOS Vehicle ivar)
    float _impulseOffset;                   // +0x1e0  ctor: 1
    float _maxSpinAV;                       // +0x1e4  ctor: 5
    bool _childEjected;                     // +0x1e8  ctor: false
    cocos2d::Sprite* _frontWheelSprite;     // +0x1f0
    cocos2d::Sprite* _backWheelSprite;      // +0x1f8
    cocos2d::Sprite* _frameSprite;          // +0x200  (iOS Vehicle ivar)
    cocos2d::Sprite* _gearSprite;           // +0x208
    b2Body* _seatBody;                      // +0x210
    b2RevoluteJoint* _seatJoint;            // +0x218  frame <-> seat ("frameSeatAnchor")
    cocos2d::Sprite* _seatSprite;           // +0x220
    b2Filter _extraFilter;                  // +0x228  ctor: {1, 0xffff, 0}; addKid: {4, 0xffff, child group}
    b2RevoluteJoint* _seatChest;            // +0x230
    b2RevoluteJoint* _seatPelvis;           // +0x238
    b2RevoluteJoint* _seatUpperLeg1;        // +0x240
    b2RevoluteJoint* _seatUpperLeg2;        // +0x248
    bool _seatDetached;                     // +0x250  ctor: false
    int _childGroupIndex;                   // +0x254  ctor: 0
    CharacterB2D* _dad;                     // +0x258  Android-only (iOS: Vehicle::characters[0])
    CharacterB2D* _child;                   // +0x260  ctor: null
    unsigned int _childPreviousMaskBits;    // +0x268  ctor: 0
    float _childSeatBreakLimit;             // +0x26c  ctor: 400
    float _frontWheelAngOffset;             // +0x270  ctor: 0  sprite angle offset after a wheel smash
    float _backWheelAngOffset;              // +0x274  ctor: 0
    b2Body* _backWheelBody;                 // +0x278
    b2Body* _frontWheelBody;                // +0x280
    b2Body* _gearBody;                      // +0x288  pedal crank
    b2Body* _frameBody;                     // +0x290  (iOS Vehicle ivar)
    b2Fixture* _frame1Fixture;              // +0x298
    b2Fixture* _frame2Fixture;              // +0x2a0
    b2Fixture* _frame3Fixture;              // +0x2a8
    b2RevoluteJoint* _vehicleHand1;         // +0x2b0  frame <-> dad lower arm 1 (iOS Vehicle ivars)
    b2RevoluteJoint* _vehicleHand2;         // +0x2b8  frame <-> dad lower arm 2
    b2RevoluteJoint* _vehicleLowerLeg1;     // +0x2c0  pedal <-> dad lower leg 1
    b2RevoluteJoint* _vehicleLowerLeg2;     // +0x2c8  pedal <-> dad lower leg 2
    b2RevoluteJoint* _vehiclePelvis;        // +0x2d0  frame <-> dad pelvis
    b2RevoluteJoint* _backWheelJoint;       // +0x2d8  frame <-> back wheel (gear joint's joint1)
    b2RevoluteJoint* _frontWheelJoint;      // +0x2e0  frame <-> front wheel
    b2RevoluteJoint* _frameGearJoint;       // +0x2e8  frame <-> gear (gear joint's joint2)
    b2GearJoint* _gearJoint;                // +0x2f0  back wheel <-> gear, ratio -1
    unsigned int _previousMaskBits;         // +0x2f8  dad's leg maskBits before attaching; ctor: 0
    Sound* _frontWheelSound;                // +0x300  unused (ctor: null)
    Sound* _backWheelSound;                 // +0x308  unused (ctor: null)
    Sound* _frameSound;                     // +0x310  unused (ctor: null)
    bool _destroyFrame;                     // +0x318  unused, not initialised
    // RE-TODO(@005cbc08): a cocos2d::Sprite held by value (constructed by the ctor, destroyed by
    // the dtor, never used otherwise). Not in the iOS ivar list; the name is ours.
    cocos2d::Sprite _embeddedSprite;        // +0x320 (0x530 bytes)
    std::map<b2Fixture*, VehicleContact> _contactResultBufferDict;  // +0x850  (iOS Vehicle ivar)
};
