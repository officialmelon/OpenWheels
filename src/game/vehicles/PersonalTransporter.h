#pragma once

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include "Vehicle.h"

#include <new>
#include <string>

class CharacterB2D;
class Sound;

// Business Guy's self-balancing personal transporter (iOS class `Segway`): one motorised wheel
// (_wheelBody, Vehicle::_frontWheelFixture) under a shock body sliding on a prismatic joint below the
// stand, and a frame/handle body hinged to the stand (_standFrame, leaned by the lean buttons).
// special1 = jump (shock joint motor). Smashing the handle (impulse dictionary 20) breaks the frame
// off and ejects the rider.
//
// arm64 sizeof 0x240; own fields 0x1b8..0x23c (Vehicle's tail padding 0x1b4 is unused).
// No user-provided constructor or destructor: create() value-initialises the object
// (new (std::nothrow) PersonalTransporter() → zero fill + in-class initializers); the vtable's
// complete-object destructor is Vehicle's, the deleting destructor is @006023b0.
class PersonalTransporter : public Vehicle
{
public:
    // Inline in the original header (the kept copy is in BusinessGuy's TU; BusinessGuy::init
    // creates the vehicle).
    // @00589224
    static PersonalTransporter* create(cocos2d::Vec2 position, std::string name, int groupID)
    {
        PersonalTransporter* ret = new (std::nothrow) PersonalTransporter();
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

    // ---- LevelItem overrides ----
    void paint() override;                                                  // @00601f3c  vptr+0x28
    void actions() override;                                                // @00601f38  vptr+0x30

    // ---- Vehicle overrides (vtable order) ----
    bool init(cocos2d::Vec2 position, std::string name, int groupID) override;  // @005ffb1c  vptr+0x110
    void addCharacter(CharacterB2D* character) override;                    // @005ffc7c  vptr+0x118
    void forwardButtonPressed() override;                                   // @0060209c  vptr+0x128
    void backButtonPressed() override;                                      // @006020c0  vptr+0x130
    void forwardBackButtonsNull() override;                                 // @006020e4  vptr+0x138
    void leanForwardButtonPressed() override;                               // @00602128  vptr+0x140
    void leanBackButtonPressed() override;                                  // @00602144  vptr+0x148
    void leanButtonsNull() override;                                        // @00602158  vptr+0x150
    void special1ButtonPressed() override;                                  // @00602174  vptr+0x158
    void special1ButtonNull() override;                                     // @00602300  vptr+0x160
    bool ejectCharacter(CharacterB2D* character) override;                  // @00601cec  vptr+0x170
    void ejectAllCharacters() override;                                     // @00601ce8  vptr+0x178
    void lockWheels() override;                                             // @00601468  vptr+0x180
    void createSprites() override;                                          // @00600278  vptr+0x198
    void createFilters() override;                                          // @00600820  vptr+0x1a0
    void createBodies() override;                                           // @00600824  vptr+0x1a8
    void createFixtures() override;                                         // @0060149c  vptr+0x1b0
    void createJoints() override;                                           // @006014a0  vptr+0x1b8
    void setLimits() override;                                              // @006017f4  vptr+0x1d0
    void addContactListeners() override;                                    // @006017f8  vptr+0x1d8
    void createDictionaries() override;                                     // @006017fc  vptr+0x1e0
    void leanForwardPose() override;                                        // @006023a8  vptr+0x218
    void leanBackPose() override;                                           // @006023a4  vptr+0x220
    void noLeanBackPose() override;                                         // @006023ac  vptr+0x228
    void checkStateOfCharacter(CharacterB2D* character) override;           // @00601a3c  vptr+0x2a8
    void handleContactResults() override;                                   // @00601be4  vptr+0x2b8

    // QOL (PC addition): re-grab vehicle (Vehicle.h).
    b2Body* qolFrameBody() override;
    bool qolCanRemount(CharacterB2D* character) override;
    void qolRemount(CharacterB2D* character) override;

    // ---- non-virtual ----
    // Handle smashed: destroys the stand/shock bodies and the stand-frame joint, ejects the rider.
    // Both arguments are unused (handleContactResults passes the contact impulse and an
    // uninitialised b2Vec2).
    void frameSmash(float impulse, b2Vec2 point);                           // @0060196c
    // "SegwayLoop1" on the wheel body, faded to 0.25 over 0.2 s.
    void addMotorSound();                                                   // @00601dcc
    void stopMotorSound();                                                  // @00601ef8

protected:
    Sound* _motorSound;                    // +0x1b8  "SegwayLoop1" while accelerating/braking
    cocos2d::Sprite* _frameSprite;         // +0x1c0  <name>_frame.png (vehicle background; _frameBody userData)
    cocos2d::Sprite* _wheelCoverSprite;    // +0x1c8  <name>_wheelCover.png (vehicle foreground; _standBody userData)
    cocos2d::Sprite* _shockSprite;         // +0x1d0  <name>_shock.png (vehicle foreground; hidden by frameSmash)
    cocos2d::Sprite* _wheelSprite;         // +0x1d8  <name>_wheel.png (vehicle foreground)
    cocos2d::Sprite* _innerSprite;         // +0x1e0  <name>_inner.png (child of _wheelSprite)
    b2Body* _frameBody;                    // +0x1e8  frame + handle; the rider's hands and feet are jointed to it
    b2Body* _wheelBody;                    // +0x1f0
    b2Body* _shockBody;                    // +0x1f8  fixed rotation, collides with nothing
    b2Body* _standBody;                    // +0x200  fixed rotation
    // RE-TODO(@00600824): 8-byte member never read or written by the Android build (zero from
    // create's value-initialisation); type and purpose unknown.
    void* _unk0x208;                       // +0x208
    b2Fixture* _frameFixture;              // +0x210  "frameShape"
    b2Fixture* _handleFixture;             // +0x218  "handleShape"; postSolve-registered, smash threshold 20
    b2RevoluteJoint* _wheelJoint;          // +0x220  shock - wheel (Vehicle::addWheelJoint)
    b2PrismaticJoint* _shockJoint;         // +0x228  stand - shock (jump)
    b2RevoluteJoint* _standFrame;          // +0x230  stand - frame at "frameAnchor" (lean); nulled by frameSmash
    float _jumpTranslation = 0.48f;        // +0x238  shock travel for the jump (0x3ef5c28f)
};
