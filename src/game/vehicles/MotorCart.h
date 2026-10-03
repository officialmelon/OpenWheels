#pragma once

#include "Vehicle.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <map>
#include <new>
#include <string>

class CharacterB2D;
class Sound;

// Effective Shopper's mobility scooter: frame body with a front basket (cart) holding a soda and
// a cracker box, wheels hung on prismatic shock bodies (special1 = hop: the shock joints are
// driven down to -_mcJumpTrans), a motor loop sound while driving, and smashable cart/frame/
// groceries.
//
// Uses its own art ("motor_cart_frame.png", "_cart", "_shock", "_wheel", "_handle", ...) and the
// bodies plist vehicles/bodies/<name>.plist.
//
// arm64 sizeof 0x2d8. Own fields start at 0x1b8. Names follow the iOS ivars; the Android port
// moved the wheel fixtures into Vehicle (+0x100 front, +0x108 back) and the frame body/sprite, the
// lean constants and the contact-result map from iOS Vehicle into this class. The constructor
// nulls every pointer member (and Vehicle's two wheel fixtures).
class MotorCart : public Vehicle
{
public:
    // @005acd38 (inline; copy emitted in EffectiveShopper's translation unit)
    static MotorCart* create(cocos2d::Vec2 position, std::string name, int groupID)
    {
        MotorCart* motorCart = new (std::nothrow) MotorCart;
        if (motorCart) {
            if (motorCart->init(position, name, groupID)) {
                motorCart->autorelease();
            } else {
                delete motorCart;
                motorCart = nullptr;
            }
        }
        return motorCart;
    }

    MotorCart();             // @005f4694
    ~MotorCart() override;   // @005f47a8 (D1), @005f47e4 (D0)

    bool init(cocos2d::Vec2 position, std::string name, int groupID) override;  // @005f4808 vptr+0x110
    void lockWheels() override;                 // @005f4908 vptr+0x180  locks the back wheel
    void createSprites() override;              // @005f493c vptr+0x198
    void addHandleToFrameSprite(cocos2d::Sprite* frameSprite);  // @005f4c3c
    void addCartToFrameSprite(cocos2d::Sprite* frameSprite);    // @005f4d64
    void createDictionaries() override;         // @005f4e5c vptr+0x1e0
    void createBodies() override;               // @005f53ac vptr+0x1a8
    void addCharacter(CharacterB2D* character) override;  // @005f6994 vptr+0x118
    void createJoints() override;               // @005f70d4 vptr+0x1b8
    void cartSmash(float impulse, b2Vec2 normal, bool useSound);  // @005f72a8
    void frameSmash(float impulse, b2Vec2 normal);    // @005f7450
    void crackerSmash(float impulse, b2Vec2 normal);  // @005f84fc
    void colaSmash(float impulse, b2Vec2 normal);     // @005f86fc
    void debugFunction(int value) override;     // @005f8860 vptr+0xf8   frameSmash
    void special1ButtonPressed() override;      // @005f8864 vptr+0x158  hop
    void forwardButtonPressed() override;       // @005f8a50 vptr+0x128
    void addMotorSound();                       // @005f8a74
    void backButtonPressed() override;          // @005f8b9c vptr+0x130
    void stopMotorSound();                      // @005f8bc0
    void forwardBackButtonsNull() override;     // @005f8bf8 vptr+0x138
    void leanBackButtonPressed() override;      // @005f8c34 vptr+0x148
    void leanForwardButtonPressed() override;   // @005f8e50 vptr+0x140
    void special1ButtonNull() override;         // @005f9064 vptr+0x160
    void ejectBtnPressed() override;            // @005f9144 vptr+0x168
    bool ejectCharacter(CharacterB2D* character) override;  // @005f92b4 vptr+0x170
    void checkStateOfCharacter(CharacterB2D* character) override;  // @005f9634 vptr+0x2a8
    void actions() override;                    // @005f9bb8 vptr+0x30   (Vehicle::actions)
    void paint() override;                      // @005f9bbc vptr+0x28
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;  // @005f9e24 vptr+0x88
    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override;  // @005f9e78 vptr+0xa0
    void handleContactResults() override;       // @005fa11c vptr+0x2b8

protected:
    b2Body* _frontWheelBody;                // +0x1b8
    b2Body* _backWheelBody;                 // +0x1c0
    b2Body* _backShockBody;                 // +0x1c8
    b2Body* _frontShockBody;                // +0x1d0
    b2Body* _frameBody;                     // +0x1d8  (iOS Vehicle ivar)
    b2Fixture* _mainFixture1;               // +0x1e0
    b2Fixture* _mainFixture2;               // +0x1e8
    b2Fixture* _mainFixture3;               // +0x1f0
    b2Fixture* _mainFixture4;               // +0x1f8  "rear", then overwritten by "shaftShape" (createBodies)
    b2Fixture* _cartFixture;                // +0x200
    b2Fixture* _handleFixture;              // +0x208
    b2Fixture* _shaftFixture;               // +0x210  never assigned (createBodies stores the shaft in _mainFixture4)
    b2Fixture* _sodaFixture;                // +0x218
    b2Fixture* _crackerFixture;             // +0x220
    b2RevoluteJoint* _backWheelJoint;       // +0x228  back shock <-> back wheel
    b2RevoluteJoint* _frontWheelJoint;      // +0x230  front shock <-> front wheel
    b2PrismaticJoint* _backShockJoint;      // +0x238  frame <-> back shock
    b2PrismaticJoint* _frontShockJoint;     // +0x240  frame <-> front shock
    cocos2d::Sprite* _frameSprite;          // +0x248  (iOS Vehicle ivar) frame body user data
    cocos2d::Sprite* _cartSprite;           // +0x250
    cocos2d::Sprite* _frontShockSprite;     // +0x258
    cocos2d::Sprite* _backShockSprite;      // +0x260
    cocos2d::Sprite* _frontWheelSprite;     // +0x268
    cocos2d::Sprite* _backWheelSprite;      // +0x270
    cocos2d::Sprite* _handleSprite;         // +0x278
    cocos2d::Sprite* _smashedSodaSprite;    // +0x280
    // RE-TODO(@005f4694): only nulled by the ctor, never used. iOS has frontWheelSound and
    // backWheelSound here; Android keeps a single slot.
    Sound* _frontWheelSound;                // +0x288  unused
    Sound* _motorSound;                     // +0x290
    cocos2d::Vec2 _frameAnchor;             // +0x298  ctor: (0.215, 1.05)
    float _mcJumpTrans;                     // +0x2a0  ctor: 0.32
    float _impulseMagnitudeMax;             // +0x2a4  ctor: 1.25 (iOS Vehicle ivar)
    float _impulseOffset;                   // +0x2a8  ctor: 1 (the lean impulses use a literal 1 instead)
    float _maxSpinAV;                       // +0x2ac  ctor: 5
    float _cartSmashLimit;                  // +0x2b0  ctor: 100   (Android-only; iOS used literals)
    float _frameSmashLimit;                 // +0x2b4  ctor: 200
    float _groceriesSmashLimit;             // +0x2b8  ctor: 1     soda and cracker fixtures
    // postSolve records only the impulse (the normal stays zero); handleContactResults smashes and clears.
    std::map<b2Fixture*, VehicleContact> _contactResultBufferDict;  // +0x2c0  (iOS Vehicle ivar)
};
