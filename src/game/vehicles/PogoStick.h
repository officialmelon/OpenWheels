#pragma once

#include "Vehicle.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <map>
#include <new>
#include <string>
#include <vector>

class CharacterB2D;

// The pogo stick: a frame body (handles + foot pegs) and a rod body on a prismatic spring joint
// (_pogoJoint) whose bottom "nub" fixture counts ground contacts. special1 charges/compresses the
// spring; forward/back hop when the nub is grounded; lean buttons tilt toward a target angle that
// is corrected against the rider's centre of mass (computed from _COMArray, the rider's bodies
// still attached). Arm/leg injuries drop those bodies from the COM set; the frame can break in
// two.
//
// arm64 sizeof 0x420. Own fields start at 0x1b4, in Vehicle's tail padding (Vehicle dsize 0x1b4).
// Names follow the iOS ivars; the Android port moved the frame body, the lean constants and the
// contact-result map from iOS Vehicle into this class and added the two leg flags at 0x1b4.
// The ctor sets every b2Vec2 member to b2Vec2_zero and default-constructs the four joint defs.
//
// create() was inline in the original header (emitted in PogostickGuy's translation unit).
class PogoStick : public Vehicle
{
public:
    // @00608774 (inline; copy emitted in PogostickGuy's translation unit)
    static PogoStick* create(cocos2d::Vec2 position, std::string name, int groupID)
    {
        PogoStick* pogoStick = new (std::nothrow) PogoStick;
        if (pogoStick) {
            if (pogoStick->init(position, name, groupID)) {
                pogoStick->autorelease();
            } else {
                delete pogoStick;
                pogoStick = nullptr;
            }
        }
        return pogoStick;
    }

    PogoStick();             // @0060304c
    ~PogoStick() override;   // @00603264 (D1), @006032b0 (D0)

    bool init(cocos2d::Vec2 position, std::string name, int groupID) override;  // @006032d4 vptr+0x110
    // Not an override: LevelItem::timeStepChanged() takes no argument. No direct callers.
    void timeStepChanged(float timeStep);       // @00603420  _fps = 1 / timeStep
    void createSprites() override;              // @00603430 vptr+0x198
    void createBodies() override;               // @00603730 vptr+0x1a8
    void createJoints() override;               // @00604180 vptr+0x1b8
    void createDictionaries() override;         // @00604268 vptr+0x1e0
    void addCharacter(CharacterB2D* character) override;  // @006043e0 vptr+0x118
    b2Vec2 getCenterOfMass();                   // @0060608c  mass-weighted mean of _COMArray world centers
    bool ejectCharacter(CharacterB2D* character) override;  // @00606158 vptr+0x170
    void singleAction() override;               // @00606274 vptr+0x38
    void leanForwardButtonPressed() override;   // @006062e0 vptr+0x140
    void leanBackButtonPressed() override;      // @006064a4 vptr+0x148
    void leanButtonsNull() override;            // @00606670 vptr+0x150
    void updateCOMValues();                     // @006067a8
    void forwardButtonPressed() override;       // @006068fc vptr+0x128
    void backButtonPressed() override;          // @00606ad0 vptr+0x130
    void forwardBackButtonsNull() override;     // @00606ca8 vptr+0x138
    void special1ButtonPressed() override;      // @00606d40 vptr+0x158
    void special1ButtonNull() override;         // @00606da8 vptr+0x160
    void checkStateOfCharacter(CharacterB2D* character) override;  // @00606f24 vptr+0x2a8
    void handleUpperArm1Injury(CharacterB2D* character) override;  // @00607120 vptr+0x258
    void removeFromCOMArray(b2Body* body);      // @006071f4
    void handleUpperArm2Injury(CharacterB2D* character) override;  // @00607258 vptr+0x260
    void handleLowerArm1Injury(CharacterB2D* character) override;  // @0060732c vptr+0x268
    void handleLowerArm2Injury(CharacterB2D* character) override;  // @006073a4 vptr+0x270
    void handleUpperLeg1Injury(CharacterB2D* character) override;  // @0060741c vptr+0x278
    void handleLowerLeg1Injury(CharacterB2D* character) override;  // @006074f8 vptr+0x288
    void handleUpperLeg2Injury(CharacterB2D* character) override;  // @00607584 vptr+0x280
    void handleLowerLeg2Injury(CharacterB2D* character) override;  // @00607660 vptr+0x290
    void nubContactAdd(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact);     // @006076ec
    void handleContactResults() override;       // @0060772c vptr+0x2b8

    // QOL (PC addition): re-grab vehicle (Vehicle.h).
    b2Body* qolFrameBody() override;
    bool qolCanRemount(CharacterB2D* character) override;
    void qolRemount(CharacterB2D* character) override;
    void frameSmash(float impulse);             // @006078c4
    void nubContactRemove(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact);  // @00607f14
    void extraPose1() override;                 // @00607f50 vptr+0x240
    void actions() override;                    // @00607fe0 vptr+0x30
    void paint() override;                      // @00607ff4 vptr+0x28
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;  // @00608074 vptr+0x88
    void endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;    // @006080c8 vptr+0x90
    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override;  // @00608110 vptr+0xa0

protected:
    // Android-only. Note the order: leg 2's flag comes first.
    bool _leg2Injured;                      // +0x1b4  set by handleUpper/LowerLeg2Injury; ctor/init: false
    bool _leg1Injured;                      // +0x1b5  set by handleUpper/LowerLeg1Injury
    float _impulseMagnitudeMax;             // +0x1b8  ctor: 0.5 (iOS Vehicle ivar)
    float _impulseOffset;                   // +0x1bc  ctor: 1
    float _maxSpinAV;                       // +0x1c0  ctor: 5
    unsigned int _nubContacts;              // +0x1c4  ctor: 0
    bool _charging;                         // +0x1c8  ctor: false
    unsigned int _jumpFrames;               // +0x1cc  ctor: 0; set to 10 on a hop
    bool _airLean;                          // +0x1d0  ctor/init: false
    float _targetAngle;                     // +0x1d4  ctor: 0; init: pi/2
    float _frameSmashLimit;                 // +0x1d8  ctor: 40
    bool _frameSmashed;                     // +0x1dc  ctor: false
    bool _destroyFrameBody;                 // +0x1dd  ctor: false
    b2Vec2 _handleAnchorPoint;              // +0x1e0
    b2Vec2 _footAnchorPoint;                // +0x1e8
    b2RevoluteJointDef _frameHand1JointDef; // +0x1f0
    b2RevoluteJointDef _frameHand2JointDef; // +0x240
    b2RevoluteJointDef _frameFoot1JointDef; // +0x290
    b2RevoluteJointDef _frameFoot2JointDef; // +0x2e0
    b2Body* _frameBody;                     // +0x330  (iOS Vehicle ivar)
    b2Body* _rodBody;                       // +0x338
    b2Fixture* _frameShape;                 // +0x340
    b2Fixture* _rodShape;                   // +0x348
    b2Fixture* _nubShape;                   // +0x350
    b2Fixture* _stopperShape;               // +0x358  unused
    cocos2d::Sprite* _frameMC;              // +0x360  frame body user data
    cocos2d::Sprite* _rodMC;                // +0x368  rod body user data
    cocos2d::Sprite* _brokenFrame1MC;       // +0x370
    cocos2d::Sprite* _brokenFrame2MC;       // +0x378
    cocos2d::Sprite* _springMC;             // +0x380
    b2PrismaticJoint* _pogoJoint;           // +0x388  frame <-> rod
    b2RevoluteJoint* _frameHand1;           // +0x390
    b2RevoluteJoint* _frameHand2;           // +0x398
    b2RevoluteJoint* _frameFoot1;           // +0x3a0
    b2RevoluteJoint* _frameFoot2;           // +0x3a8
    std::vector<b2Body*> _COMArray;         // +0x3b0  rider bodies used for the centre of mass
    b2Vec2 _currCOM;                        // +0x3c8
    b2Vec2 _prevCOM;                        // +0x3d0
    b2Vec2 _velocityCOM;                    // +0x3d8
    float _velocityAngle;                   // +0x3e0
    b2Vec2 _pivotDirection;                 // +0x3e4
    float _pivotAngle;                      // +0x3ec
    unsigned int _tempElbowBreakLimit;      // +0x3f0  ctor: 0, otherwise unused
    unsigned int _tempElbowLigamentLimit;   // +0x3f4  ctor: 0, otherwise unused
    unsigned int _tempKneeBreakLimit;       // +0x3f8  ctor: 0, otherwise unused
    unsigned int _tempKneeLigamentLimit;    // +0x3fc  ctor: 0, otherwise unused
    float _correctionMultiplier;            // +0x400  ctor: 0, otherwise unused
    float _fps;                             // +0x404  1 / time step
    // Only ever cleared (handleContactResults); postSolve records into LevelItem's
    // map<b2Fixture*, LevelItemContact> at +0x58 instead.
    std::map<b2Fixture*, VehicleContact> _contactResultBufferDict;  // +0x408
};
