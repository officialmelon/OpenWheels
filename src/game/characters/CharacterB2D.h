#pragma once

// CharacterB2D: the ragdoll rider shared by every playable character (BusinessGuy, WheelchairGuy,
// IrresponsibleDad, EffectiveShopper, PogostickGuy, MopedCouple derive from it). Box2D bodies,
// fixtures and revolute joints for 11 body parts, their sprites, joint-motor poses, injuries
// (joint breaks, smashes, dismemberment, gore emitters), voice clips and the vehicle hand-off.
//
// arm64 layout: LevelItem at +0x00 (sizeof 0x98), EmitterDelegate vptr at +0x98, own fields
// +0xa0 .. +0x510 (sizeof 0x510; IrresponsibleDad/MopedCouple add one pointer -> 0x518).
//
// There is no out-of-line constructor in the binary: the constructor is inline and is visible
// inlined into the subclass constructors (e.g. BusinessGuy() @00588e74) and into the bare
// `new CharacterB2D` of LevelB2D::createCharacter @005cf084. The default member initializers below
// reproduce exactly the stores made there; members without initializer are not touched by that
// constructor (they are set by init() or never).
//
// Member names follow the iOS original's Objective-C ivars where one exists (the Android port is
// a translation of that class; `python build/tmp/m1_objc_ivars.py <iOS Mach-O> CharacterB2D` dumps
// them). Enumerator names are not in the binary and are descriptive.

#include <map>
#include <string>
#include <vector>

#include "Box2D/Box2D.h"
#include "base/CCValue.h"
#include "math/Vec2.h"

#include "EmitterDelegate.h"
#include "LevelItem.h"

namespace cocos2d {
class Sprite;
}

class Emitter;
class IntestineChain;
class Ligament;
class Sound;
class SpinalCord;
class Vehicle;

// Reported to the vehicle through Vehicle::handleInjury (vtable +0x120), which maps value - 1
// through a jump table (@0041e238) onto its handle*Injury hooks.
enum CharacterInjury
{
    CharacterInjuryHeadSmash = 0,       // headSmash (Vehicle ignores it)
    CharacterInjuryPelvisSmash = 1,     // pelvisSmash            -> Vehicle::handleFatalWound
    CharacterInjuryChestSmash = 2,      // chestSmash             -> handleFatalWound
    CharacterInjuryFoot1Smash = 3,      // foot1Smash             -> handleLowerLeg1Injury
    CharacterInjuryFoot2Smash = 4,      // foot2Smash             -> handleLowerLeg2Injury
    CharacterInjuryShoulder1Break = 5,  // shoulderBreak1         -> handleUpperArm1Injury
    CharacterInjuryShoulder2Break = 6,  // shoulderBreak2         -> handleUpperArm2Injury
    CharacterInjuryElbow1Break = 7,     // elbowBreak1            -> handleLowerArm1Injury
    CharacterInjuryElbow2Break = 8,     // elbowBreak2            -> handleLowerArm2Injury
    CharacterInjuryHip1Break = 9,       // hipBreak1              -> handleUpperLeg1Injury
    CharacterInjuryHip2Break = 10,      // hipBreak2              -> handleUpperLeg2Injury
    CharacterInjuryKnee1Break = 11,     // kneeBreak1             -> handleLowerLeg1Injury
    CharacterInjuryKnee2Break = 12,     // kneeBreak2             -> handleLowerLeg2Injury
    CharacterInjuryTorsoBreak = 13,     // torsoBreak             -> handleFatalWound
    CharacterInjuryNeckBreak = 14,      // neckBreak              -> handleFatalWound
    // 15 is never produced (neither here nor in the iOS original); Vehicle maps it to
    // handleFatalWound.
    CharacterInjuryDeath = 16,          // setDead(true)          -> handleFatalWound
};

// Joint-motor pose applied every frame while ejected (actions/checkPose). setState maps the
// control bits 0x01/0x02/0x04/0x08 of an ejected character to poses 1..4.
enum CharacterPose
{
    CharacterPoseNone = 0,      // setCurrentPose(None) -> cancelPose()
    CharacterPoseSuperman = 1,  // supermanPose()
    CharacterPoseTuck = 2,      // tuckPose()
    CharacterPoseArch = 3,      // archPose()
    CharacterPosePushup = 4,    // pushupPose()
};

// Priority of a queued voice clip (_nextVocalPriority); checkVocals only interrupts the playing
// clip for a strictly higher priority. 0 means "nothing queued" (the iOS original used -1).
enum VocalPriority
{
    VocalPriorityNone = 0,
    VocalPriority1 = 1,  // mourn(), foot1Smash/foot2Smash, debugFunc
    VocalPriority2 = 2,  // elbowBreak1/2
    VocalPriority3 = 3,  // kneeBreak1/2
    VocalPriority4 = 4,  // shoulderBreak1/2
    VocalPriority5 = 5,  // hipBreak1/2, shapeImpale
    VocalPriority6 = 6,  // torsoBreak, pelvisSmash, debugFunc
};

class CharacterB2D : public LevelItem, public EmitterDelegate
{
public:
    CharacterB2D() {}
    ~CharacterB2D() override;                                               // @0058a7dc

    // ---- LevelItem overrides -------------------------------------------------------------------
    void actions() override;                                                // @0058eff0  vptr+0x030
    void timeStepChanged() override;                                        // @0059c858  vptr+0x048
    int getFluidType() override;                                            // @0059dd0c  vptr+0x068
    int shapeImpale(b2Fixture* fixture, bool fatal, b2Vec2 stabPosition,
                    float killDistance) override;                           // @0059d5e0  vptr+0x070
    void explodeShape(b2Fixture* fixture, float ratio) override;            // @0059d840  vptr+0x078
    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override;               // @0058ad9c  vptr+0x0a0
    void jointWillBeDestroyed(b2Joint* joint) override;                     // @0059da98  vptr+0x0a8
    void handleContactAdds() override;                                      // @0059794c  vptr+0x0c0
    void debugFunction(int value) override;                                 // @0059dd3c  vptr+0x0f8

    // ---- new virtuals, declaration order == vtable order (vptr+0x110 .. +0x198) ---------------
    // Loads "characters/bodies/<name>_<vehicleName>.plist" and the sprite frames, builds the rig.
    // Always returns true.
    virtual bool init(cocos2d::Vec2 origin, std::string name, std::string vocalPrefix,
                      std::string vehicleName, int groupID, bool showGore,
                      bool targetable);                                     // @005895b8  vptr+0x110
    // Per-frame control bits from GameplayControls: 0x01 forward, 0x02 back, 0x04 lean forward,
    // 0x08 lean back, 0x10 special/grab, 0x80 eject (forwarded to the vehicle while riding; poses
    // and grabbing once ejected).
    virtual void setState(unsigned char state);                             // @0058aa7c  vptr+0x118
    virtual void doNothing();                                               // @0058aa30  vptr+0x120
    virtual void setCurrentPose(CharacterPose pose);                        // @0059e264  vptr+0x128
    virtual void startGrab();                                               // @0059dad4  vptr+0x130
    virtual void endGrab();                                                 // @0059db48  vptr+0x138
    virtual void setMourner(CharacterB2D* mourner);                         // @005a0a38  vptr+0x140
    virtual void mourn();                                                   // @0059c6c4  vptr+0x148
    virtual void debugFunc(float a, float b, float c);                      // @0059dd1c  vptr+0x150
    virtual void loadBodies(std::string name);                              // @0058ac0c  vptr+0x158
    virtual void createSprites();                                           // @0058af3c  vptr+0x160
    virtual void createBodies();                                            // @0058c144  vptr+0x168
    virtual void createFixtures();                                          // @0058c9bc  vptr+0x170
    virtual void createFixtures(bool unused);                               // @0058c9cc  vptr+0x178
    virtual void createJoints();                                            // @0058d9ac  vptr+0x180
    // EmitterDelegate (secondary-vtable thunk @005a0958): forget a finished blood emitter.
    void emitterComplete(Emitter* emitter) override;                        // @005a0878  vptr+0x188
    virtual void taperBodies();                                             // @0059cfb0  vptr+0x190
    virtual void preloadSounds();                                           // @005a01b4  vptr+0x198

    // ---- construction helpers -----------------------------------------------------------------
    void createFilters();                                                   // @00589acc
    void setLimits();                                                       // @00589b04
    void addContactListeners();                                             // @0058a27c
    void createDictionaries();                                              // @0058a2cc
    cocos2d::Sprite* createHelmetSprite();                                  // @0058bfcc

    // ---- state ---------------------------------------------------------------------------------
    void setMainCharacter(bool mainCharacter);                              // @0058aa08
    int getGroupIndex();                                                    // @0058aa10
    bool getDead();                                                         // @0058aa18
    bool getEjected();                                                      // @0058aa20
    b2Body* getFocus();                                                     // @0058aa28
    void setFocus(b2Body* body);                                            // @0058c978
    void ejectBtnPressed();                                                 // @0058abf4
    void setDead(bool dead);                                                // @0059c380
    void eject();                                                           // @0059c4cc
    void postInjury(CharacterInjury injury);                                // @0059c6a8
    void setDying(bool dying);                                              // @0059c784
    void setVehicle(Vehicle* vehicle);                                      // @0059d924
    Vehicle* getVehicle();                                                  // @0059d95c
    bool getShowGore();                                                     // @0059dd14

    // ---- per-frame processing ------------------------------------------------------------------
    void removeFromJointsToCheck(b2Joint* joint);                           // @0058eec8
    void removeFromContactResultBufferDict(b2Fixture* fixture);             // @0058ef2c
    void checkPose();                                                       // @0058f100
    void checkVocals();                                                     // @0058f144
    void checkBleedOut();                                                   // @0058f3d0
    void handleContactResults();                                            // @0058f400
    void checkJoints();                                                     // @0058ff94

    // ---- injuries ------------------------------------------------------------------------------
    void breakJoint(const b2Joint* joint, float force);                     // @00590178
    void neckBreak(float force, bool blood, bool sound);                    // @00590240
    // The last flag is never read (every caller passes false).
    void torsoBreak(float force, bool blood, bool sound, bool unusedFlag);  // @005907e4
    void shoulderBreak1(float force, bool blood);                           // @00590f18
    void shoulderBreak2(float force, bool blood);                           // @00591cf0
    void elbowBreak1(float force);                                          // @00592acc
    void elbowBreak2(float force);                                          // @005935c0
    void hipBreak1(float force, bool blood);                                // @005940b4
    void hipBreak2(float force, bool blood);                                // @005951dc
    void kneeBreak1(float force);                                           // @005961f4
    void kneeBreak2(float force);                                           // @00596d98
    void helmetSmash(float impulse);                                        // @00597950
    void headSmash(float impulse);                                          // @00597dd8
    void chestSmash(float impulse);                                         // @00598a88
    void pelvisSmash(float impulse);                                        // @00599ca4
    void foot1Smash(float impulse);                                         // @0059aa90
    void foot2Smash(float impulse);                                         // @0059b580

    // ---- grabbing ------------------------------------------------------------------------------
    void grabAction1(b2Body* otherBody);                                    // @0059c070
    void grabAction2(b2Body* otherBody);                                    // @0059c1f8
    void openHand1(bool open);                                              // @0059d18c
    void openHand2(bool open);                                              // @0059d23c

    // ---- voice ---------------------------------------------------------------------------------
    void addVocalsWithName(std::string name, VocalPriority priority);       // @0059c774
    void playRandomVocals(b2Fixture* fixture, VocalPriority priority);      // @0059d694
    void voiceSoundFinishedPlaying();                                       // @005a0118
    std::string randomVocals(int index);                                    // @005a0124

    // ---- shapes and joint limits ---------------------------------------------------------------
    void shortenRectOfFixture(b2Fixture* fixture, float percentage, bool offTheTop);  // @0059cfac
    void taperBody(b2Body* body, float percent, bool top);                  // @0059d14c
    void resetJointLimits();                                                // @0059d2ec
    void scaleJointLimit(b2RevoluteJoint* joint, float percent);            // @0059d964
    void multiplyElbowLigamentLimit(float factor);                          // @0059da78
    void multiplyKneeLigamentLimit(float factor);                           // @0059da88

    // ---- poses ---------------------------------------------------------------------------------
    // Drives the joint motor towards (upper limit + angle).
    void setJoint(b2RevoluteJoint* joint, float angle, float gain, float maxSpeed);  // @0059e1d0
    void cancelPose();                                                      // @0059e280
    void supermanPose();                                                    // @0059e334
    void tuckPose();                                                        // @0059e750
    void archPose();                                                        // @0059eb64
    void pushupPose();                                                      // @0059ef74

    // ---- debug (DebugLayer buttons) ------------------------------------------------------------
    void debugElbowBreak1(float value);                                     // @0059f3a4
    void debugElbowBreak2(float value);                                     // @0059f3bc
    void debugShoulderBreak1(float value);                                  // @0059f3d4
    void debugShoulderBreak2(float value);                                  // @0059f3f0
    void debugHipBreak1(float value);                                       // @0059f40c
    void debugHipBreak2(float value);                                       // @0059f428
    void debugKneeBreak1(float value);                                      // @0059f444
    void debugKneeBreak2(float value);                                      // @0059f45c
    void debugTorsoBreak(float value);                                      // @0059f474
    void debugNeckBreak(float value);                                       // @0059f498
    void debugHeadSmash(float value);                                       // @0059f4b8
    void debugChestSmash(float value);                                      // @0059f4c8
    void debugPelvisSmash(float value);                                     // @0059f4d8
    void debugFoot1Smash(float value);                                      // @0059f4e8
    void debugFoot2Smash(float value);                                      // @0059f4ec
    void debugHelmetSmash(float value);                                     // @0059f4f0

    // ---- wounds and blood ----------------------------------------------------------------------
    void addNeckWoundToChest();                                             // @0059dd5c
    void addNeckBloodFlow();                                                // @0059dee4
    void addShoulderWoundToChest();                                         // @0059f500
    void addShoulder1BloodFlow(float force);                                // @0059f68c
    void addArm1BloodFlow();                                                // @0059f750
    void addShoulder2BloodFlow(float force);                                // @0059f7e4
    void addArm2BloodFlow();                                                // @0059f8a8
    void addHip1BloodFlow(float force);                                     // @0059f93c
    void addThigh1BloodFlow(float force);                                   // @0059fa00
    void addHip2BloodFlow(float force);                                     // @0059faac
    void addThigh2BloodFlow(float force);                                   // @0059fb70
    void addPelvisWoundToPelvis();                                          // @0059fc1c
    void addStomchBloodFlow();                                              // @0059fd9c
    void addNeckWoundToHead();                                              // @0059fe2c
    void addHeadBloodFlow();                                                // @005a0088

    // ---- body / joint accessors ----------------------------------------------------------------
    // The heart body once the chest has been smashed, the chest body otherwise.
    b2Body* getCentralBody();                                               // @005a079c
    b2Body* getHeadBody();                                                  // @005a07b0
    b2Body* getChestBody();                                                 // @005a07b8
    b2Body* getUpperArm1Body();                                             // @005a07c0
    b2Body* getUpperArm2Body();                                             // @005a07c8
    b2Body* getUpperArm3Body();                                             // @005a07d0
    b2Body* getUpperArm4Body();                                             // @005a07d8
    b2Body* getLowerArm1Body();                                             // @005a07e0
    b2Body* getLowerArm2Body();                                             // @005a07e8
    b2Body* getPelvisBody();                                                // @005a07f0
    b2Body* getUpperLeg1Body();                                             // @005a07f8
    b2Body* getUpperLeg2Body();                                             // @005a0800
    b2Body* getUpperLeg3Body();                                             // @005a0808
    b2Body* getUpperLeg4Body();                                             // @005a0810
    b2Body* getLowerLeg1Body();                                             // @005a0818
    b2Body* getLowerLeg2Body();                                             // @005a0820
    b2RevoluteJoint* getNeckJoint();                                        // @005a0828
    b2RevoluteJoint* getWaistJoint();                                       // @005a0830
    b2RevoluteJoint* getShoulderJoint1();                                   // @005a0838
    b2RevoluteJoint* getShoulderJoint2();                                   // @005a0840
    b2RevoluteJoint* getElbowJoint1();                                      // @005a0848
    b2RevoluteJoint* getElbowJoint2();                                      // @005a0850
    b2RevoluteJoint* getHipJoint1();                                        // @005a0858
    b2RevoluteJoint* getHipJoint2();                                        // @005a0860
    b2RevoluteJoint* getKneeJoint1();                                       // @005a0868
    b2RevoluteJoint* getKneeJoint2();                                       // @005a0870

    // RESTORED (PC addition): the browser game's lawnmower blade (Flash CharacterB2D.grindShape /
    // removeBody), used by the restored Lawnmower Man (src/restored/LawnMower). Not in the
    // original; nothing in the mobile game calls these.
    // ownsBody: the body is one of this character's parts. grindFixture: the part is caught by
    // the blade - it stops bleeding and no longer smashes. grindBody: the blade finishes the part
    // off - every joint holding it breaks, as in the browser game.
    bool ownsBody(b2Body* body);
    void grindFixture(b2Fixture* fixture);
    void grindBody(b2Body* body);

    // ONLINE (PC addition): a joint anchor of the browser game's body file
    // (online/FlashBodyShapes.h) in world space, false when this character has none by that
    // name (for example "handleAnchor", where the browser moped riders hold on).
    bool onlineFlashAnchor(const std::string& key, b2Vec2* worldPoint);

    // ONLINE (PC addition): browser user-built vehicles (Flash CharacterB2D.userVehicle,
    // grabAction, userVehicleEject). Implemented in src/online/vehicles/UserVehicleRider.cpp and
    // only reached in converted browser levels (online::flashLevel()).
    bool onlineDriveUserVehicle(unsigned char state);          // setState while riding one
    bool onlineGrabUserVehicle(int hand, b2Fixture* handle);   // a grabbing hand touched a handle
    void onlineUserVehicleEject();
    void onlineUserVehicleInjury(CharacterInjury injury);      // arm lost / death
    void onlineUserVehiclePose();                              // poses 10..12 (checkPose)
    void onlineUserVehicleJointDestroyed(b2Joint* joint);

    // QOL (PC addition): re-grab vehicle (qol::regrabVehicle, Vehicle::qolTryRemount). Not in
    // the original. Limb injuries are recorded (postInjury) so a re-mount lets go of the same
    // limbs again; qolLost* mirror Vehicle::handleInjury's mapping (shoulder: upper + lower arm,
    // elbow: lower arm, hip: upper + lower leg, knee and foot smash: lower leg).
    bool qolHasInjury(CharacterInjury injury) const;
    const std::vector<CharacterInjury>& qolInjuries() const { return _qolInjuries; }
    bool qolLostLowerArm(int arm) const;
    bool qolLostUpperLeg(int leg) const;
    bool qolLostLowerLeg(int leg, bool footSmashCounts = true) const;
    // Alive, not bleeding out, head, chest and pelvis still joined.
    bool qolFit();
    bool qolIsGripJoint(const b2Joint* joint) const { return joint && (joint == _gripJoint1 || joint == _gripJoint2); }
    // The eleven body parts (nullptr for a destroyed one).
    std::vector<b2Body*> qolParts();
    // Fixtures of the limbs lost to injuries (the parts no longer on him).
    std::vector<b2Fixture*> qolLostFixtures();
    // Vehicle::qolMount: let go of grips, cancel the ejected pose; then riding again (before the
    // vehicle replays the lost limbs, which may throw him off again); then hands closed, the
    // driving controls back and "characterRemounted".
    void qolBeginRemount();
    void qolSetRiding();
    void qolRemounted();

protected:
    // Voice clip names picked by playRandomVocals (.data @00abb5a8).
    static const char* _randomVocals[10];

    cocos2d::Vec2 _origin;                       // +0xa0  init position
    // RE-TODO: written only by the constructor (-1), never read; named after the unused iOS ivar.
    int _bleedCounter = -1;                      // +0xa8
    bool _mainCharacter = false;                 // +0xac  player's character: camera focus, "character*" events
    bool _dying = false;                         // +0xad  setDying(); bleeds out after 5 s (checkBleedOut)
    bool _dead = false;                          // +0xae
    bool _ejected = false;                       // +0xaf  separated from the vehicle (eject())
    bool _helmetOn = false;                      // +0xb0  bodies plist has a "helmetShape" (createSprites)
    std::string _name;                           // +0xb8  e.g. "business_guy": sprite-frame / plist prefix
    // RE-TODO: never assigned or read (iOS: _tag = bodiesDict["tag"]; Android plists have no "tag").
    std::string _tag;                            // +0xd0
    std::string _vocalPrefix;                    // +0xe8  "Char1", "Char2"...: voice clip = prefix + vocal name
    std::string _vehicleName;                    // +0x100 e.g. "personal_transporter" ("" without vehicle)
    float _bleedTimeCounter = 0.0f;              // +0x118 seconds spent dying (checkBleedOut)
    int _groupID = -1;                           // +0x11c Box2D collision group (getGroupIndex)
    b2Filter _defaultFilter;                     // +0x120 createFilters: {0x104, 0x10e, groupID}
    b2Filter _zeroFilter;                        // +0x126 createFilters: {0x104, 0xffff, 0} (severed parts)
    b2Filter _lowerBodyFilter;                   // +0x12c createFilters: {0x104, 0x10e, groupID - 5} (after torsoBreak)

    cocos2d::Sprite* _upperArm1Sprite = nullptr;      // +0x138
    cocos2d::Sprite* _upperArm2Sprite = nullptr;      // +0x140
    cocos2d::Sprite* _upperArm3Sprite = nullptr;      // +0x148 lower half of a dislocated upper arm 1
    cocos2d::Sprite* _upperArm4Sprite = nullptr;      // +0x150
    cocos2d::Sprite* _upperLeg1Sprite = nullptr;      // +0x158
    cocos2d::Sprite* _upperLeg2Sprite = nullptr;      // +0x160
    cocos2d::Sprite* _upperLeg3Sprite = nullptr;      // +0x168
    cocos2d::Sprite* _upperLeg4Sprite = nullptr;      // +0x170
    cocos2d::Sprite* _lowerArm1Sprite = nullptr;      // +0x178 closed hand
    cocos2d::Sprite* _lowerArmOpen1Sprite = nullptr;  // +0x180 open hand
    cocos2d::Sprite* _lowerArm2Sprite = nullptr;      // +0x188
    cocos2d::Sprite* _lowerArmOpen2Sprite = nullptr;  // +0x190
    cocos2d::Sprite* _lowerLeg1Sprite = nullptr;      // +0x198
    cocos2d::Sprite* _lowerLeg2Sprite = nullptr;      // +0x1a0
    cocos2d::Sprite* _pelvisSprite = nullptr;         // +0x1a8
    cocos2d::Sprite* _chestSprite = nullptr;          // +0x1b0
    cocos2d::Sprite* _headSprite = nullptr;           // +0x1b8
    cocos2d::Sprite* _shoulderWoundSprite = nullptr;  // +0x1c0
    cocos2d::Sprite* _neckWoundSprite = nullptr;      // +0x1c8
    cocos2d::Sprite* _foot1Sprite = nullptr;          // +0x1d0 foot1Smash
    cocos2d::Sprite* _foot2Sprite = nullptr;          // +0x1d8 foot2Smash

    b2Body* _headBody = nullptr;                 // +0x1e0
    b2Body* _chestBody = nullptr;                // +0x1e8
    b2Body* _upperArm1Body = nullptr;            // +0x1f0
    b2Body* _upperArm2Body = nullptr;            // +0x1f8
    b2Body* _lowerArm1Body = nullptr;            // +0x200
    b2Body* _lowerArm2Body = nullptr;            // +0x208
    b2Body* _pelvisBody = nullptr;               // +0x210
    b2Body* _upperLeg1Body = nullptr;            // +0x218
    b2Body* _upperLeg2Body = nullptr;            // +0x220
    b2Body* _lowerLeg1Body = nullptr;            // +0x228
    b2Body* _lowerLeg2Body = nullptr;            // +0x230
    b2Body* _cameraFocus = nullptr;              // +0x238 getFocus/setFocus (StageCamera target)
    b2Body* _helmetBody = nullptr;               // +0x240 knocked-off helmet (helmetSmash)

    b2Fixture* _headFixture = nullptr;           // +0x248
    b2Fixture* _chestFixture = nullptr;          // +0x250
    b2Fixture* _upperArm1Fixture = nullptr;      // +0x258
    b2Fixture* _upperArm2Fixture = nullptr;      // +0x260
    b2Fixture* _lowerArm1Fixture = nullptr;      // +0x268
    b2Fixture* _lowerArm2Fixture = nullptr;      // +0x270
    b2Fixture* _pelvisFixture = nullptr;         // +0x278
    b2Fixture* _upperLeg1Fixture = nullptr;      // +0x280
    b2Fixture* _upperLeg2Fixture = nullptr;      // +0x288
    b2Fixture* _lowerLeg1Fixture = nullptr;      // +0x290
    b2Fixture* _lowerLeg2Fixture = nullptr;      // +0x298
    // RE-TODO: +0x2a0/+0x2a8 are never used; names/types from the iOS BusinessGuy ivars
    // helmetFixture / helmetSmashLimit (the helmet moved into CharacterB2D in the Android port).
    b2Fixture* _helmetFixture = nullptr;         // +0x2a0
    float _helmetSmashLimit;                     // +0x2a8 not initialised

    b2Body* _heartBody = nullptr;                // +0x2b0 chestSmash; getCentralBody
    b2Body* _brainBody = nullptr;                // +0x2b8 headSmash
    b2Body* _upperArm3Body = nullptr;            // +0x2c0 dislocated shoulder (shoulderBreak1)
    b2Body* _upperArm4Body = nullptr;            // +0x2c8
    b2Body* _upperLeg3Body = nullptr;            // +0x2d0 dislocated hip (hipBreak1)
    b2Body* _upperLeg4Body = nullptr;            // +0x2d8
    IntestineChain* _intestineChain = nullptr;   // +0x2e0 torsoBreak (retained)
    SpinalCord* _spinalCord = nullptr;           // +0x2e8 neckBreak (retained)
    std::vector<Ligament*> _composites;          // +0x2f0 elbow/knee ligaments (retained)

    b2RevoluteJoint* _neckJoint = nullptr;       // +0x308 chest - head
    b2RevoluteJoint* _waistJoint = nullptr;      // +0x310 chest - pelvis
    b2RevoluteJoint* _shoulderJoint1 = nullptr;  // +0x318 chest - upperArm1
    b2RevoluteJoint* _shoulderJoint2 = nullptr;  // +0x320 chest - upperArm2
    b2RevoluteJoint* _elbowJoint1 = nullptr;     // +0x328 upperArm1 - lowerArm1
    b2RevoluteJoint* _elbowJoint2 = nullptr;     // +0x330 upperArm2 - lowerArm2
    b2RevoluteJoint* _hipJoint1 = nullptr;       // +0x338 pelvis - upperLeg1
    b2RevoluteJoint* _hipJoint2 = nullptr;       // +0x340 pelvis - upperLeg2
    b2RevoluteJoint* _kneeJoint1 = nullptr;      // +0x348 upperLeg1 - lowerLeg1
    b2RevoluteJoint* _kneeJoint2 = nullptr;      // +0x350 upperLeg2 - lowerLeg2
    std::vector<b2RevoluteJoint*> _jointsToCheck;  // +0x358 joints tested by checkJoints
    std::map<b2Joint*, float> _jointLimits;      // +0x370 break force per joint (setLimits)

    // RE-TODO: the constructor stores 9989 (0x2705), which is no enumerator (Vehicle's pose gets
    // the same value); it makes the first setCurrentPose(CharacterPoseNone) call cancelPose().
    CharacterPose _currentPose = static_cast<CharacterPose>(9989);  // +0x388
    bool _grabbing = false;                      // +0x38c startGrab/endGrab
    float _vocalPitch = 0.0f;                    // +0x390 init: 1.0 (createBodySound float argument)
    Sound* _voiceSound = nullptr;                // +0x398 playing voice clip
    VocalPriority _nextVocalPriority = VocalPriorityNone;     // +0x3a0
    VocalPriority _currentVocalPriority = VocalPriorityNone;  // +0x3a4
    std::string _nextVocalString;                // +0x3a8 queued clip name, e.g. "Elbow1"
    b2RevoluteJoint* _gripJoint1 = nullptr;      // +0x3c0 hand 1 holding something (grabAction1)
    b2RevoluteJoint* _gripJoint2 = nullptr;      // +0x3c8
    b2Vec2 _shoulderBloodFlowPos;                // +0x3d0 shoulder joint 1 local anchor A (createJoints)
    b2Vec2 _pelvisBloodFlowPos;                  // +0x3d8 hip joint 1 local anchor A (createJoints)

    // Blood emitters (nullptr when finished, see emitterComplete). Not initialised by the
    // constructor; init() clears them.
    Emitter* _neckBloodFlow;                     // +0x3e0
    Emitter* _headBloodFlow;                     // +0x3e8
    Emitter* _shoulder1BloodFlow;                // +0x3f0
    Emitter* _shoulder2BloodFlow;                // +0x3f8
    Emitter* _stomachBloodFlow;                  // +0x400
    Emitter* _hip1BloodFlow;                     // +0x408
    Emitter* _hip2BloodFlow;                     // +0x410
    Emitter* _arm1BloodFlow;                     // +0x418
    Emitter* _arm2BloodFlow;                     // +0x420
    Emitter* _thigh1BloodFlow;                   // +0x428
    Emitter* _thigh2BloodFlow;                   // +0x430

    Vehicle* _vehicle = nullptr;                 // +0x438 retained (setVehicle / subclass init)
    bool _showGore;                              // +0x440 !UserDefault "gore_disabled" (init); not ctor-initialised
    bool _targetable;                            // +0x441 init; head/chest/pelvis get fixture material 2
    // Impulse threshold per fixture (createDictionaries) and the strongest contact above it seen
    // by postSolve; consumed by handleContactResults. These shadow LevelItem's own maps.
    std::map<b2Fixture*, float> _contactImpulseDict;                  // +0x448
    std::map<b2Fixture*, LevelItemContact> _contactResultBufferDict;  // +0x460

    // Injury thresholds computed from the body masses in setLimits (rescaled by timeStepChanged).
    float _headSmashLimit = 0.0f;                // +0x478
    float _chestSmashLimit = 0.0f;               // +0x47c
    float _pelvisSmashLimit = 0.0f;              // +0x480
    float _footSmashLimit = 0.0f;                // +0x484
    float _neckBreakLimit = 0.0f;                // +0x488
    float _spineLimit = 0.0f;                    // +0x48c below it neckBreak leaves a spinal cord
    float _torsoBreakLimit = 0.0f;               // +0x490
    float _intestineLimit = 0.0f;                // +0x494 below it torsoBreak leaves intestines
    float _shoulderBreakLimit = 0.0f;            // +0x498
    float _shoulderSnapLimit = 0.0f;             // +0x49c up to it the shoulder only dislocates
    float _hipBreakLimit = 0.0f;                 // +0x4a0
    float _hipSnapLimit = 0.0f;                  // +0x4a4
    float _elbowBreakLimit = 0.0f;               // +0x4a8
    float _elbowLigamentLimit = 0.0f;            // +0x4ac
    float _kneeBreakLimit = 0.0f;                // +0x4b0
    float _kneeLigamentLimit = 0.0f;             // +0x4b4

    // Damage state of each limb = number of its current sprite frame ("<name>_upperArm1_<n>.png").
    // init: 1.
    int _upperArm1State = 0;                     // +0x4b8
    int _upperArm2State = 0;                     // +0x4bc
    int _upperLeg1State = 0;                     // +0x4c0
    int _upperLeg2State = 0;                     // +0x4c4
    int _lowerLeg1State = 0;                     // +0x4c8
    int _lowerLeg2State = 0;                     // +0x4cc
    cocos2d::Vec2 _heartAnchor;                  // +0x4d0 init: (0.6, 0.5) (chestSmash)
    cocos2d::Vec2 _brainAnchor;                  // +0x4d8 init: (0.5, 0.6) (headSmash)
    cocos2d::ValueMap _bodiesDict;               // +0x4e0 "characters/bodies/<name>_<vehicle>.plist"
    CharacterB2D* _mourner = nullptr;            // +0x508 mourns this character's death (setMourner)

    // QOL (PC addition): re-grab vehicle. Time since the ejection (sum of physics steps) and the
    // limb injuries in the order they happened.
    float _qolEjectedTime = 0.0f;
    std::vector<CharacterInjury> _qolInjuries;
    // A hand touched `other` (contact) or overlaps a body of the vehicle (qolCheckRemount): back
    // on the vehicle when everything allows it.
    bool qolGrabRemount(int hand, b2Body* other);
    void qolCheckRemount();
    bool qolHandFree(int hand) const;
};
