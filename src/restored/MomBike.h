#pragma once

// RESTORED (PC addition): Irresponsible Mom's bicycle with her kids' rides (browser game
// character 10, Flash v1.87 com.totaljerkface.game.character.IrresponsibleMom : BicycleGuy, IMDaughter,
// IMSon), which the mobile port left out. One mobile Vehicle carries all three:
//   * the mom's bicycle (Flash BicycleGuy): frame, wheels and pedal gear geared to the back wheel,
//     frame smash limit 100 (IrresponsibleMom), wheel smash limit 200, smashed into fork, broken
//     frame and seat;
//   * the daughter's trailer bike (IMDaughter): her own frame, wheel and pedal gear on a limited
//     revolute joint behind the mom's frame (detached above 400 Flash = 800 mobile reaction
//     force); she pedals with the same up/down keys and brakes with space, smashes at 50;
//   * the son's basket (IMSon): welded on the handlebar (detached above 100 Flash = 200 mobile),
//     the son held by his hands and a prismatic pelvis joint; smashes at 3.
// Shift ejects the son, Ctrl the daughter, Z everyone (Flash shift/ctrl/zPressedActions); the
// restored controls give them on-screen buttons (control bits 0x20 / 0x40, IrresponsibleMom).
//
// Shapes come from vehicles/bodies/mom_bike.plist (the mom's shapeGuide, then the daughter's and
// the son's under "daughter_" / "son_"), sprites from vehicles/mom_bike_sprites.plist; both are
// generated from the browser game's character10.swf by tools/assets/extract_character.py.
// Constants follow the mobile port's own conversion of the same Flash BicycleGuy (RoadBike):
// motor speeds x2 with the same per-frame step, lean impulses x s_timeStepOverFlashTimeStep,
// joint break limits x2 (ChildSeatKid 200 -> RoadBike 400), smash limits as in Flash. Lengths: the
// Flash world is 62.5 px per metre (character art 125 symbol px per metre), the same metres as here.

#include "Vehicle.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <map>
#include <string>
#include <vector>

class CharacterB2D;

class MomBike : public Vehicle
{
public:
    static MomBike* create(cocos2d::Vec2 position, std::string name, int groupID);

    MomBike();
    ~MomBike() override;

    bool init(cocos2d::Vec2 position, std::string name, int groupID) override;
    void createSprites() override;
    void createBodies() override;
    void createJoints() override;
    void createDictionaries() override;
    void lockWheels() override;

    void addMom(CharacterB2D* mom);
    void addDaughter(CharacterB2D* daughter);
    void addSon(CharacterB2D* son);

    void handleInjury(CharacterInjury injury, CharacterB2D* character) override;
    void checkStateOfCharacter(CharacterB2D* character) override;
    bool ejectCharacter(CharacterB2D* character) override;

    // QOL (PC addition): re-grab vehicle (src/game/vehicles/Vehicle.h).
    b2Body* qolFrameBody() override;
    bool qolCanRemount(CharacterB2D* character) override;
    void qolRemount(CharacterB2D* character) override;
    void qolRemountResetBodies(std::vector<b2Body*>* bodies) override;

    void forwardButtonPressed() override;
    void backButtonPressed() override;
    void forwardBackButtonsNull() override;
    void special1ButtonPressed() override;
    void leanForwardButtonPressed() override;
    void leanBackButtonPressed() override;
    void leanBackPose() override;
    void leanForwardPose() override;

    // The kids' own key actions, every frame whatever the mom does (Flash IrresponsibleMom
    // .checkKeyStates): the daughter pedals her wheel with up/down and brakes with space while
    // she rides; shift (0x20) ejects the son, ctrl (0x40) the daughter.
    void kidControls(unsigned char state);
    bool kidRiding(CharacterB2D* kid);

    void actions() override;
    void paint() override;
    void checkJoints() override;
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;
    void endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;
    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override;
    void handleContactResults() override;
    void debugFunction(int value) override;  // smashes the frame

private:
    // A body painted by paint(): at its mass centre (Flash paint() always uses GetWorldCenter;
    // sprites whose art is centred on the clip registration) or at its origin (sprites drawn
    // around the shapeGuide origin, 'inner@' frames).
    struct Painted
    {
        b2Body* body;
        cocos2d::Sprite* sprite;
        bool atCentre;
        float angleOffset;  // degrees (broken wheels: the tyre's fold follows the hit normal)
    };
    cocos2d::ValueMap shape(const std::string& name);
    b2Vec2 point(const std::string& name);  // shapeGuide point, world
    b2Fixture* addPolygon(b2Body* body, b2FixtureDef def, const std::string& prefix, int count);
    b2Body* addCircleBody(const std::string& name, b2FixtureDef def);
    b2Body* makePiece(b2Body* from, const std::string& prefix, int count, const std::string& sprite,
                      const std::string& hitSound);
    cocos2d::Sprite* sprite(const std::string& frame, cocos2d::Node* parent, int z = 0);
    void paintBody(b2Body* body, cocos2d::Sprite* sprite, bool atCentre);
    void unpaint(b2Body* body);
    void refilterKid(CharacterB2D* kid, b2Fixture* fixture, bool clearSensor);
    void refilterKidParts(CharacterB2D* kid, bool clearSensor, bool upperOnly);
    void destroyJoint(b2Joint*& joint);
    void momEject();
    void daughterEject();
    void sonEject();
    void frameSmash();
    void wheelSmash(bool front, b2Vec2 normal);
    void daughterFrameSmash();
    void detachDaughter();
    void basketSmash();
    void detachBasket();
    bool gameplay();

    // Flash constants
    float _frameSmashLimit;          // 100 (IrresponsibleMom; BicycleGuy 200)
    float _wheelSmashLimit;          // 200
    float _impulseMagnitude;         // 3
    float _impulseOffset;            // 1
    float _maxSpinAV;                // 5
    float _daughterMaxSpeed;         // 27.7 Flash -> 55.4
    float _daughterAccelStep;        // 1.385
    float _daughterFrameSmashLimit;  // 50
    float _daughterDetachLimit;      // 400 Flash -> 800
    float _basketSmashLimit;         // 3
    float _basketDetachLimit;        // 100 Flash -> 200
    float _sonEjectImpulse;          // 0.75

    CharacterB2D* _mom;
    CharacterB2D* _daughter;
    CharacterB2D* _son;
    bool _momEjected;
    bool _daughterEjected;
    bool _sonEjected;
    bool _frameSmashed;
    bool _daughterDetached;
    bool _daughterSmashed;
    bool _basketDetached;
    bool _basketSmashed;

    // mom's bicycle
    b2Body* _frameBody;
    b2Body* _frontWheelBody;
    b2Body* _backWheelBody;
    b2Body* _gearBody;
    b2Fixture* _seatFixture;   // Flash frameShape1 (category 0x201): the smash shape
    b2Fixture* _frameFixture;  // frameShape2 (zero filter)
    b2Fixture* _forkFixture;   // frameShape3 (zero filter)
    b2RevoluteJoint* _frontWheelJoint;
    b2RevoluteJoint* _backWheelJoint;
    b2RevoluteJoint* _frameGearJoint;
    b2GearJoint* _gearJoint;
    // (the riders' joints live in Vehicle::_bodyVehicleJointDict, keyed by their bodies)

    // daughter's trailer bike
    b2Body* _dFrameBody;
    b2Body* _dWheelBody;
    b2Body* _dGearBody;
    b2Fixture* _dSeatFixture;
    b2Fixture* _dFrameFixture;   // the smash shape (Flash frameShape)
    b2Fixture* _dHandleFixture;
    b2Fixture* _dMidFixture;
    b2Fixture* _dEndFixture;     // sensor until the trailer comes off
    b2Fixture* _dWheelFixture;
    b2RevoluteJoint* _dWheelJoint;
    b2RevoluteJoint* _dFrameGearJoint;
    b2GearJoint* _dGearJoint;
    b2Joint* _dConnectingJoint;

    // son's basket
    b2Body* _basketBody;
    std::vector<b2Fixture*> _basketFixtures;
    b2Joint* _basketJoint;     // Flash connectingJoint (welded to the mom's frame)

    b2Filter _daughterExtraFilter;  // {0x104, 0xffff, daughter group} (Flash extraFilter, 260)
    b2Filter _sonExtraFilter;

    cocos2d::Sprite* _frameSprite;
    cocos2d::Sprite* _gearSprite;
    cocos2d::Sprite* _frontWheelSprite;
    cocos2d::Sprite* _backWheelSprite;
    cocos2d::Sprite* _dFrameSprite;
    cocos2d::Sprite* _dWheelSprite;
    cocos2d::Sprite* _dGearSprite;
    cocos2d::Sprite* _basketSprite;
    std::map<b2Fixture*, VehicleContact> _results;  // strongest smash per bike part this step
    std::vector<Painted> _painted;
};
