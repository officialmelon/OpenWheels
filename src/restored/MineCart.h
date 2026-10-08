#pragma once

// RESTORED (PC addition): Explorer Guy's mine cart (browser game character 7, Flash v1.87
// com.totaljerkface.game.character.MiddleAgedExplorer), which the mobile port left out. The Flash
// class's cart half lives here, the rider is ExplorerGuy.
//
//   * a heavy cart (frame density 1.5, category 0x202) on two motor wheels (density 12, in the
//     rider's collision group), the explorer standing in it: hands on the rim, feet on the floor
//     (limited +-15 degrees); his lower legs are hidden inside;
//   * up/down accelerate the wheels without a speed cap (Flash wheelMaxSpeed 1000);
//   * space clamps the wheels onto level rails (browser special 27, material 4): while held, a
//     wheel that touches a rail gets a "dongle" body on a prismatic joint along the rail, and the
//     motor then drives the dongles along the rails (Flash checkAddRailJoints /
//     checkRemoveRailJoints); letting go releases them;
//   * shift stands up, ctrl crouches (Flash poses 8 / 7; on-screen buttons via control bits 0x20 /
//     0x40, see ExplorerGuy);
//   * the frame smashes above 150 into cart, side, bottom and engine pieces with a burst of cart
//     shards.
//
// Shapes come from vehicles/bodies/mine_cart.plist, sprites from vehicles/mine_cart_sprites.plist
// (generated from character7.swf by tools/assets/extract_character.py). Conversions as the mobile
// port's MotorCart (same Flash motor style): wheel and rail speeds as in Flash with half the
// per-frame step at 60 Hz, lean impulses x s_timeStepOverFlashTimeStep, smash limits as in Flash.
// Flash metres are these metres (Flash m_physScale 62.5 px per metre).

#include "Vehicle.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <map>
#include <string>
#include <vector>

class CharacterB2D;
class Sound;

class MineCart : public Vehicle
{
public:
    static MineCart* create(cocos2d::Vec2 position, std::string name, int groupID);

    MineCart();
    ~MineCart() override;

    bool init(cocos2d::Vec2 position, std::string name, int groupID) override;
    void createSprites() override;
    void createBodies() override;
    void createJoints() override;
    void createDictionaries() override;
    void lockWheels() override;
    void addCharacter(CharacterB2D* character) override;
    void handleInjury(CharacterInjury injury, CharacterB2D* character) override;
    void checkStateOfCharacter(CharacterB2D* character) override;
    bool ejectCharacter(CharacterB2D* character) override;

    // QOL (PC addition): re-grab vehicle (src/game/vehicles/Vehicle.h).
    b2Body* qolFrameBody() override;
    bool qolCanRemount(CharacterB2D* character) override;
    void qolRemount(CharacterB2D* character) override;

    void forwardButtonPressed() override;
    void backButtonPressed() override;
    void forwardBackButtonsNull() override;
    void leanForwardButtonPressed() override;
    void leanBackButtonPressed() override;
    void special1ButtonPressed() override;
    void special1ButtonNull() override;
    void extraPose1() override;  // stand (Flash leanForwardPose, shift)
    void extraPose2() override;  // crouch (Flash leanBackPose, ctrl)

    // Shift (0x20) / ctrl (0x40) while riding (ExplorerGuy::setState).
    void extraControls(unsigned char state);
    bool riderOn() const { return _rider && !_riderEjected; }

    void actions() override;
    void paint() override;
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;
    void endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;
    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override;
    void handleContactResults() override;
    void debugFunction(int value) override;  // smashes the cart

private:
    struct Painted
    {
        b2Body* body;
        cocos2d::Sprite* sprite;
    };
    // A wheel and its rail clamp (Flash front/back: wheel, dongle, rail joints).
    struct Clamp
    {
        b2Body* wheel = nullptr;
        b2RevoluteJoint* wheelJoint = nullptr;
        b2Vec2 localPos;                        // the wheel's place on the frame (frame local)
        b2Body* dongle = nullptr;
        b2RevoluteJoint* dongleJoint = nullptr; // dongle <-> frame
        b2PrismaticJoint* railJoint = nullptr;  // dongle <-> rail
        b2Body* rail = nullptr;
        b2Body* newRail = nullptr;              // touched while clamping, attached next frame
    };

    cocos2d::ValueMap shape(const std::string& name);
    b2Vec2 point(const std::string& name);      // shapeGuide point, world
    b2Fixture* addBox(b2Body* body, b2FixtureDef def, const std::string& name);
    b2Fixture* addPolygon(b2Body* body, b2FixtureDef def, const std::string& prefix);
    void paintBody(b2Body* body, const std::string& frame);
    bool wheelCloseToRail(const Clamp& clamp);
    void removeDongle(Clamp& clamp);
    void checkRemoveRailJoints();
    void checkAddRailJoints();
    void attach(Clamp& clamp);
    void accelerate(Clamp& clamp, bool forward);
    void wheelSounds();
    void startLoop(Sound*& sound, const std::string& name);
    void stopLoop(Sound*& sound);
    void frameSmash();
    bool gameplay();

    // Flash constants
    float _accelStep;            // 1 -> 0.5 per 60 Hz step
    float _prisAccelStep;        // 0.5 m/s per Flash frame -> 0.25 per step
    float _wheelMaxSpeed;        // 1000
    float _impulseLeft;          // 0.7
    float _impulseRight;         // 0.7
    float _impulseOffset;        // 1
    float _maxSpinAV;            // 5
    float _hatSmashLimit;        // 0.75 (CharacterB2D's helmet: 2)
    float _frameSmashLimit;      // 150
    float _railDistanceMin;      // 37 Flash px
    float _railJointY;           // 23 Flash px above the rail's centre line
    int _oneDongleMax;           // 35 Flash frames -> 70 steps

    CharacterB2D* _rider;
    bool _riderEjected;
    bool _connecting;
    bool _frameSmashed;
    int _oneDongleCounter;

    b2Body* _frameBody;
    b2Fixture* _bottomFixture;
    b2Fixture* _leftFixture;
    b2Fixture* _rightFixture;
    Clamp _front;
    Clamp _back;
    b2Vec2 _clampPoint;          // contact point of the last rail hit (sparks)

    cocos2d::Sprite* _frameSprite;
    cocos2d::Sprite* _frontWheelSprite;
    cocos2d::Sprite* _backWheelSprite;
    std::vector<Painted> _painted;
    std::map<b2Fixture*, float> _results;  // strongest frame hit this step

    Sound* _rollLoop[3];
};
