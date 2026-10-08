#pragma once

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include "LevelItem.h"
#include "CharacterB2D.h"  // CharacterB2D, CharacterInjury

#include <functional>
#include <map>
#include <string>
#include <vector>

class Sound;

// Pose a vehicle drives its rider(s) into every frame (Vehicle::checkPose()). The values come from
// the binary (checkPose dispatches value k to the k-th pose hook, vtable 0x208 + (k-1)*8, the same
// switch as the iOS original); the enumerator names do not survive and follow the hook names.
enum VehiclePose
{
    VehiclePoseNone = 0,           // setCurrentPose(VehiclePoseNone) calls cancelPose()
    VehiclePoseForward = 1,        // forwardPose()       forwardButtonPressed
    VehiclePoseBack = 2,           // backPose()          backButtonPressed
    VehiclePoseLeanForward = 3,    // leanForwardPose()
    VehiclePoseLeanBack = 4,       // leanBackPose()
    VehiclePoseNoLeanBack = 5,     // noLeanBackPose()
    VehiclePoseNoLeanForward = 6,  // noLeanForwardPose()
    VehiclePoseSpecial = 7,        // specialPose()
    VehiclePoseExtra1 = 8,         // extraPose1()
    VehiclePoseExtra2 = 9,         // extraPose2()
    VehiclePoseExtra3 = 10,        // extraPose3()
};

// Value type of the per-fixture contact-result buffers of Moped, MotorCart, RoadBike and PogoStick
// (std::map<b2Fixture*, VehicleContact>); Moped::handleFramePostSolve takes it by value.
// 12 bytes, an HFA of three floats (passed in s0..s2). The postSolve overrides store the strongest
// normal impulse and copy the contact manifold's localNormal (b2Contact+0xa0, e.g. Moped::postSolve
// @005f3c18).
struct VehicleContact
{
    b2Vec2 normal;   // +0x0  b2Manifold::localNormal
    float impulse;   // +0x8
};

// Base class of every vehicle (PersonalTransporter, Wheelchair, Moped, MotorCart, RoadBike,
// PogoStick). The rider characters are attached with revolute joints recorded in
// _bodyVehicleJointDict; injuries reported by a rider (CharacterB2D::postInjury) arrive in
// handleInjury() and break the corresponding joints.
//
// arm64: own fields 0x98..0x1b4 (dsize 0x1b4, sizeof 0x1b8). Derived classes place their first
// field in the tail padding at 0x1b4 (Wheelchair, PogoStick, RoadBike).
//
// There is no Vehicle constructor or destructor function in the binary: both are implicit. The
// default member initializers below are inlined into every derived constructor (e.g. Wheelchair()
// @00641c18, Moped() @005ee190, the implicit one in PersonalTransporter::create @00589224).
// The implicit destructor's D2 is @005cbd98 (emitted in another TU), D0 @00640008.
//
// The empty virtual hooks were defined inline in this header: their single kept copies sit in
// other translation units (Moped/MotorCart/PogoStick/RoadBike) or at the end of Vehicle's TU in
// vtable order. Their addresses are given above each definition.
class Vehicle : public LevelItem
{
public:
    // ---- LevelItem overrides ---------------------------------------------------------------
    void actions() override;                                               // @0063f944  vptr+0x30
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture,
                      b2Contact* contact) override;                        // @0063eee0  vptr+0x88
    void endContact(b2Fixture* fixture, b2Fixture* otherFixture,
                    b2Contact* contact) override;                          // @0063ef14  vptr+0x90
    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override;              // @0063ef40  vptr+0xa0

    // ---- new virtuals, in vtable order (vptr+0x110 .. vptr+0x2b8) ----------------------------

    // Loads the body plist and sprite frames for `name`, runs the create* hooks and registers
    // with the level's action list. Always returns true.
    virtual bool init(cocos2d::Vec2 position, std::string name, int groupID);   // @0063d308  vptr+0x110
    // Appends to _characters; subclasses then joint the rider to the vehicle.
    virtual void addCharacter(CharacterB2D* character);                        // @0063db00  vptr+0x118
    // Called by CharacterB2D::postInjury; dispatches to the handle*Injury hooks.
    virtual void handleInjury(CharacterInjury injury, CharacterB2D* character); // @0063fb6c  vptr+0x120

    // Control input (CharacterB2D::setState): accelerate / reverse / neither.
    virtual void forwardButtonPressed();                                       // @0063f5bc  vptr+0x128
    virtual void backButtonPressed();                                          // @0063f720  vptr+0x130
    virtual void forwardBackButtonsNull();                                     // @0063f884  vptr+0x138
    // @0064002c
    virtual void leanForwardButtonPressed() {}                                 //            vptr+0x140
    // @00640030
    virtual void leanBackButtonPressed() {}                                    //            vptr+0x148
    virtual void leanButtonsNull();                                            // @0063f920  vptr+0x150
    // @00640034
    virtual void special1ButtonPressed() {}                                    //            vptr+0x158
    // @0060eab8
    virtual void special1ButtonNull() {}                                       //            vptr+0x160
    virtual void ejectBtnPressed();                                            // @0063dc74  vptr+0x168
    // Returns true if `character` was riding this vehicle (and is now ejected).
    virtual bool ejectCharacter(CharacterB2D* character);                      // @0063dcd4  vptr+0x170
    virtual void ejectAllCharacters();                                         // @0063dc80  vptr+0x178
    // @00608354
    virtual void lockWheels() {}                                               //            vptr+0x180
    // @005f3e20
    virtual void checkJoints() {}                                              //            vptr+0x188
    // true when the joint should break: reaction force above `limit`, or anchors drifted apart
    // (squared distance > 0.25). false for a null joint.
    virtual bool checkRevJoint(b2Joint* joint, float limit);                   // @0063dd70  vptr+0x190

    // Creation hooks. init calls createSprites, createFilters, createBodies, createFixtures,
    // createJoints, setLimits, addContactListeners, createDictionaries, in that order.
    // @00640038
    virtual void createSprites() {}                                            //            vptr+0x198
    // @005f3e24
    virtual void createFilters() {}                                            //            vptr+0x1a0
    // @0064003c
    virtual void createBodies() {}                                             //            vptr+0x1a8
    // @005f3e28
    virtual void createFixtures() {}                                           //            vptr+0x1b0
    // @00640040
    virtual void createJoints() {}                                             //            vptr+0x1b8
    // _bodyVehicleJointDict[body] = joint
    virtual void addBodyVehicleJoint(b2Body* body, b2Joint* joint);            // @0063d8c8  vptr+0x1c0
    virtual void removeBodyVehicleJoint(b2Body* body);                         // @0063d98c  vptr+0x1c8
    // @005f3e2c
    virtual void setLimits() {}                                                //            vptr+0x1d0
    // @005f3e30
    virtual void addContactListeners() {}                                      //            vptr+0x1d8
    // @00640044
    virtual void createDictionaries() {}                                       //            vptr+0x1e0
    // Registers a motorised wheel; its speed ratio is relative to the first wheel's radius.
    virtual void addWheelJoint(b2RevoluteJoint* joint, b2Body* wheelBody);     // @0063f2e4  vptr+0x1e8

    // Rider poses.
    virtual void setCurrentPose(VehiclePose pose);                             // @0063f178  vptr+0x1f0
    virtual void cancelPose();                                                 // @0063f19c  vptr+0x1f8
    virtual void checkPose();                                                  // @0063f2c0  vptr+0x200
    // @005f3e34
    virtual void forwardPose() {}                                              //            vptr+0x208
    // @005f3e38
    virtual void backPose() {}                                                 //            vptr+0x210
    // @005fa668
    virtual void leanForwardPose() {}                                          //            vptr+0x218
    // @005fa66c
    virtual void leanBackPose() {}                                             //            vptr+0x220
    // @005fa670
    virtual void noLeanBackPose() {}                                           //            vptr+0x228
    // @005fa674
    virtual void noLeanForwardPose() {}                                        //            vptr+0x230
    // @005f3e3c
    virtual void specialPose() {}                                              //            vptr+0x238
    // @005f3e40
    virtual void extraPose1() {}                                               //            vptr+0x240
    // @005f3e44
    virtual void extraPose2() {}                                               //            vptr+0x248
    // @005f3e48
    virtual void extraPose3() {}                                               //            vptr+0x250

    // Injury hooks (see handleInjury): break the rider's joints to the vehicle for the injured
    // limb, then checkStateOfCharacter().
    virtual void handleUpperArm1Injury(CharacterB2D* character);               // @0063fb98  vptr+0x258
    virtual void handleUpperArm2Injury(CharacterB2D* character);               // @0063fc00  vptr+0x260
    virtual void handleLowerArm1Injury(CharacterB2D* character);               // @0063fc68  vptr+0x268
    virtual void handleLowerArm2Injury(CharacterB2D* character);               // @0063fcb4  vptr+0x270
    virtual void handleUpperLeg1Injury(CharacterB2D* character);               // @0063fd00  vptr+0x278
    virtual void handleUpperLeg2Injury(CharacterB2D* character);               // @0063fd68  vptr+0x280
    virtual void handleLowerLeg1Injury(CharacterB2D* character);               // @0063fdd0  vptr+0x288
    virtual void handleLowerLeg2Injury(CharacterB2D* character);               // @0063fe1c  vptr+0x290
    // ejectCharacter(character)
    virtual void handleFatalWound(CharacterB2D* character);                    // @0063fe68  vptr+0x298
    // Destroys the vehicle joint recorded for `body` (if any and still active).
    virtual void destroyJointsForBody(b2Body* body);                           // @0063fe74  vptr+0x2a0
    // @00640048
    virtual void checkStateOfCharacter(CharacterB2D* character) {}             //            vptr+0x2a8
    virtual void destroyAllCharacterJoints(CharacterB2D* character);           // @0063deb4  vptr+0x2b0
    // @0064004c
    virtual void handleContactResults() {}                                     //            vptr+0x2b8

    // ---- non-virtual -------------------------------------------------------------------------
    // _bodiesDict = "vehicles/bodies/<name>.plist"
    void loadBodies(std::string name);                                         // @0063d70c
    // addToBeginContact + addToEndContact
    void addContactWheel(b2Fixture* fixture);                                  // @0063d89c
    // Drives a revolute joint's motor towards (upper limit + angle).
    void setJoint(b2RevoluteJoint* joint, float angle, float gain, float maxSpeed);  // @0063de20
    // Index in _characters, or -1. No callers in the binary.
    int indexOfCharacter(CharacterB2D* character);                             // @0063ee98
    // Finish callback of _wheelSound.
    void wheelSoundStopped();                                                  // @0063f170

    // ---- QOL (PC addition): re-grab vehicle (qol::regrabVehicle, docs/QOL.md) -----------------
    // An ejected main character whose hand grabs a body of this vehicle gets back on. At the first
    // addCharacter every rider's pose is cached relative to the main body (qolFrameBody), with the
    // vehicle's bodies and fixture filters. A re-mount puts the vehicle back into that frame for a
    // moment ("spawn space"), puts the rider back into his cached pose, runs the subclass's own
    // attach code there (its anchors are spawn-time world points), moves everything back onto
    // the vehicle's current transform and gives the rider the frame's velocity. Joints keep only
    // body-local anchors and reference angles, so they come out exactly as at the start.
    // Main body the rider poses are cached against; nullptr: no re-mount.
    virtual b2Body* qolFrameBody() { return nullptr; }
    // Not smashed, and `character` still has what checkStateOfCharacter needs to keep him on.
    virtual bool qolCanRemount(CharacterB2D* character) { return false; }
    // Undoes the subclass's eject side effects and attaches `character` again (through
    // qolMount); the lost limbs are let go afterwards (qolReplayInjuries).
    virtual void qolRemount(CharacterB2D* character) {}
    // Bodies to put back into their spawn pose relative to the frame while mounting (pedal
    // cranks, so the feet land on the pedals), see qolMount.
    virtual void qolRemountResetBodies(std::vector<b2Body*>* bodies) {}
    // The body is part of this vehicle (reachable from the frame through joints, cached at the
    // start; riders' bodies excluded).
    bool qolOwnsBody(b2Body* body, CharacterB2D* rider);
    // CharacterB2D's entry point: true when `character` rides this vehicle again.
    bool qolTryRemount(CharacterB2D* character);
    // The rider's joint is there and still holds his limb (not a dislocated stub): a re-mounted
    // rider may have lost limbs, the subclasses' attach code skips their joints.
    static bool qolLimb(CharacterB2D* character, b2RevoluteJoint* joint);
    // A body of this vehicle that the hand fixture overlaps (any filter), or nullptr.
    b2Body* qolTouchedBody(b2Fixture* hand, CharacterB2D* rider);

protected:
    std::map<b2RevoluteJoint*, float> _wheelJointSpeedDict;      // +0x98   motor speed ratio per wheel joint
    std::vector<b2RevoluteJoint*> _wheelJoints;                  // +0xb0   motorised wheels (addWheelJoint)
    float _maxSpeed = 0.0f;                                      // +0xc8   target wheel speed (negative = forward)
    float _accelStep = 1.0f;                                     // +0xcc   speed added per frame while accelerating
    float _maxTorque = 0.0f;                                     // +0xd0   maxMotorTorque of the subclasses' wheel joints
    float _mainWheelShapeRadius = 1.0f;                          // +0xd4   radius of the first wheel (addWheelJoint)
    cocos2d::Vec2 _origin;                                       // +0xd8   init position
    std::string _name;                                           // +0xe0   e.g. "personal_transporter", "wheelchair"
    int _groupID = -1;                                           // +0xf8   collision group (init's int)
    bool _ejected = false;                                       // +0xfc   set by the subclasses' ejectCharacter
    // The two wheel fixtures are NOT initialised by Vehicle's constructor (Moped and PogoStick never
    // write them before init; Wheelchair, MotorCart and RoadBike null them in their own ctors;
    // PersonalTransporter comes zero-filled). Vehicle::init nulls them before createBodies().
    b2Fixture* _frontWheelFixture;                               // +0x100  wheel contacts + tire sound (PT: its only wheel)
    b2Fixture* _backWheelFixture;                                // +0x108  wheel contacts
    int _wheelContacts = 0;                                      // +0x110  non-sensor contacts touching the wheels
    Sound* _wheelSound;                                          // +0x118  tire loop; not initialised by the ctor (init: nullptr)
    std::string _wheelSoundName;                                 // +0x120  "TireLoop1" unless a subclass set it ("BikeLoop1")
    float _wheelSoundVolume;                                     // +0x138  not initialised by the ctor (init: 0.5)
    float _soundFadeTime = 0.2f;                                 // +0x13c  Sound::fadeTo duration
    b2Filter _zeroFilter;                                        // +0x140  init: {0x104, 0xffff, 0}
    b2Filter _defaultFilter;                                     // +0x146  init: {0x104, 0x10e, groupID}
    cocos2d::ValueMap _bodiesDict;                               // +0x150  body definitions (loadBodies)
    bool _vehicleSmashed = false;                                // +0x178
    std::vector<CharacterB2D*> _characters;                      // +0x180  riders
    std::map<b2Body*, b2Joint*> _bodyVehicleJointDict;           // +0x198  rider body -> joint holding it to the vehicle
    // RE-TODO(@00641cd8): every vehicle ctor stores 9989 (0x2705) here; no enumerator has that value
    // (CharacterB2D's current pose gets the same initial value).
    VehiclePose _currentPose = static_cast<VehiclePose>(9989);   // +0x1b0

    // ---- QOL (PC addition): re-grab vehicle --------------------------------------------------
    // ejectCharacter's last rider: Vehicle::cancelPose's _characters[0] once the list is empty
    // (the binary reads the erased element left in the vector's storage, i.e. this rider).
    CharacterB2D* _qolLastRider = nullptr;
    // Position and (unwrapped) body angle: joint angles are differences of body angles, so a
    // body moved back must get its own angle back, not an angle rebuilt from a rotation.
    struct QolXf
    {
        b2Vec2 p;
        float a;
    };
    struct QolRiderPose
    {
        QolXf frame;                              // qolFrameBody at the start
        std::map<b2Body*, QolXf> bodies;          // the rider's eleven parts at the start
    };
    struct QolPlanBody
    {
        b2Body* body;
        QolXf spawn;   // in spawn space (the frame at its start transform)
        QolXf target;  // final
    };
    std::map<CharacterB2D*, QolRiderPose> _qolRiderPoses;
    std::map<b2Body*, QolXf> _qolSpawnTransforms;         // vehicle bodies at the start
    std::map<b2Fixture*, b2Filter> _qolFilters;           // vehicle fixtures' riding filters
    bool _qolCached = false;
    std::vector<QolPlanBody> _qolPlan;                    // rider bodies of the pending re-mount
    void qolCacheRider(CharacterB2D* character);
    std::vector<b2Body*> qolVehicleBodies(CharacterB2D* rider);
    bool qolPlanRemount(CharacterB2D* character);
    // Runs `attach` in spawn space (see above). Rider and vehicle bodies end up in place.
    void qolMount(CharacterB2D* character, const std::function<void()>& attach);
    void qolRestoreFilters(CharacterB2D* rider);
    // Lets go of the limbs lost since the start (handleInjury for each recorded limb injury).
    void qolReplayInjuries(CharacterB2D* character);
    // A dead / dying / fatally hurt rider (any head, chest, pelvis smash, torso or neck break)
    // can not get back on.
    bool qolRiderFit(CharacterB2D* character);
};
