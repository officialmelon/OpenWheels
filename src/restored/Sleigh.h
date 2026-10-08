#pragma once

// RESTORED (PC addition): Santa Claus's sleigh and the two elves pulling it (browser game character
// 8, Flash v1.87 com.totaljerkface.game.character.SantaClaus and SleighElf), which the mobile port
// left out. One mobile Vehicle carries the lot:
//   * the sleigh: one body (rear, ski base, back, seat, front, three shaft "stem" pieces, bumper),
//     sliding on four unseen rollers; Santa sits on it, his hands on a reins body that pumps up and
//     down on a prismatic joint while he drives;
//   * eight presents loose in the back;
//   * two elves (plain CharacterB2Ds, voice "Elf1") each running on an unseen wheel that their feet
//     are pinned to (Flash SleighElf: a stem on a vertical prismatic joint ahead of the sleigh, the
//     wheel turning at Santa's speed scaled by its radius), harnessed to the reins (a strap over
//     their heads) and to the shafts (a strap to each chest, the second one hung with bells);
//   * space: flight - everything floats (gravity cancelled) for as long as the boost meter lasts,
//     with sleigh bells and a spray of snow (Flash spacePressedActions);
//   * smashes: the sleigh (200) into seven pieces with shards, the ski (200) into three, the shafts
//     (200) into one; straps snap above an impulse of 0.4.
// The elves' controls follow Santa's keys (Flash SantaClaus.checkElfKeys): up/down run; shift lets
// go of elves whose legs are broken; Z after Santa is off releases them all.
//
// Shapes come from vehicles/bodies/sleigh.plist (the elves' stem and wheel under "elf_", elf2 being
// elf1 110 Flash px further on), sprites from vehicles/sleigh_sprites.plist; generated from
// character8.swf by tools/assets/extract_character.py. Conversions as MotorCart (motor speeds as in
// Flash, half the per-frame step at 60 Hz; lean impulses x s_timeStepOverFlashTimeStep; smash limits
// as in Flash). Flash metres are these metres (Flash m_physScale 62.5 px per metre).

#include "Vehicle.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <map>
#include <string>
#include <vector>

class CharacterB2D;
class Sound;

class Sleigh : public Vehicle
{
public:
    static Sleigh* create(cocos2d::Vec2 position, std::string name, int groupID);

    Sleigh();
    ~Sleigh() override;

    bool init(cocos2d::Vec2 position, std::string name, int groupID) override;
    void createSprites() override;
    void createBodies() override;
    void createJoints() override;
    void createDictionaries() override;
    void lockWheels() override;

    void addSanta(CharacterB2D* santa);
    // Elf 0 (behind) or 1 (ahead): its stem, running wheel and joints; the straps once both are on.
    void addElf(CharacterB2D* elf, int index);

    void handleInjury(CharacterInjury injury, CharacterB2D* character) override;
    void checkStateOfCharacter(CharacterB2D* character) override;
    bool ejectCharacter(CharacterB2D* character) override;

    // QOL (PC addition): re-grab vehicle (src/game/vehicles/Vehicle.h).
    void attachSanta(CharacterB2D* santa);  // addSanta's joints (also a re-mount)
    b2Body* qolFrameBody() override;
    bool qolCanRemount(CharacterB2D* character) override;
    void qolRemount(CharacterB2D* character) override;
    // The eject button throws Santa off (impulse 5) and leaves the elves running (Flash Z).
    void ejectAllCharacters() override;

    void forwardButtonPressed() override;
    void backButtonPressed() override;
    void forwardBackButtonsNull() override;
    void leanForwardButtonPressed() override;
    void leanBackButtonPressed() override;
    void special1ButtonPressed() override;
    void special1ButtonNull() override;

    // Every frame, with the control byte (SantaClaus::setState): the elves' running, flight while
    // Santa is off (no), shift = let go of lame elves, eject (0x80) once Santa is off = release them.
    void extraControls(unsigned char state);

    void actions() override;
    void paint() override;
    void checkJoints() override;
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;
    void endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;
    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override;
    void handleContactResults() override;
    void jointWillBeDestroyed(b2Joint* joint) override;
    void debugFunction(int value) override;  // smashes the sleigh

private:
    struct Elf
    {
        CharacterB2D* character = nullptr;
        float offsetX = 0.0f;              // guide offset (elf2: 110 Flash px)
        float verticalOffset = 0.0f;       // prismatic range (35 / 70 Flash px)
        b2Body* stem = nullptr;
        b2Body* wheel = nullptr;
        b2Fixture* wheelFixture = nullptr;
        b2RevoluteJoint* wheelJoint = nullptr;
        b2Joint* foot1 = nullptr;
        b2Joint* foot2 = nullptr;
        bool legsOk = true;
        bool ejected = false;
        bool strappedIn = true;
        bool headAttached = true;
        bool chestAttached = true;
        bool wasDead = false;
        int wheelContacts = 0;
        bool stepSign = true;
        float maxSpeed = 0.0f;
        float accelStep = 0.0f;
        b2Vec2 headPoint;                  // last strap point on the head / chest (world)
        b2Vec2 chestPoint;
        b2Vec2 headVelocity;
        b2Vec2 chestVelocity;
        int runPose = 0;                   // Flash currentPose 5 / 6 while riding
    };
    // A strap: a chain of rigid links (Flash b2DistanceJoint) through `bodies` (free nodes, or the
    // reins / sleigh / an elf's head or chest), each hooked at its own local point.
    struct Strap
    {
        std::vector<b2Body*> bodies;
        std::vector<b2Vec2> locals;
        std::vector<b2DistanceJoint*> joints;  // joints[i]: bodies[i] -> bodies[i + 1]
        std::vector<float> lengths;
    };
    struct Painted
    {
        b2Body* body;
        cocos2d::Sprite* sprite;
        bool atCentre;
    };

    cocos2d::ValueMap shape(const std::string& name);
    b2Vec2 point(const std::string& name, float dx = 0.0f);  // shapeGuide point, world
    float radiusOf(const std::string& name);
    b2Fixture* addPolygon(b2Body* body, b2FixtureDef def, const std::string& prefix, int count);
    std::vector<b2Vec2> guidePolygon(const std::string& prefix, int maxCount);
    b2Body* strapNode(b2Vec2 at, b2Vec2 velocity);
    void buildStraps();
    void chain(Strap& strap, b2Body* to, b2Vec2 toPoint, int segments, bool bells);
    void link(Strap& strap, size_t i);
    void unlink(Strap& strap, size_t i);
    // Hooks the strap's body k to another body (at `world`), keeping the links' lengths.
    void replaceStrapBody(Strap& strap, size_t k, b2Body* body, b2Vec2 world);
    void paintBody(b2Body* body, const std::string& frame, bool atCentre, int z);
    void antiGravity(bool elvesOnly);
    void santaEject();
    void elfEject(Elf& elf);
    void disableRunning(Elf& elf);
    void checkLegs(Elf& elf);
    void elfRun(Elf& elf, int direction);
    void elfRunPose(Elf& elf);
    void elfHeadRemove(Elf& elf, bool smashed);
    void elfChestRemove(Elf& elf, bool smashed);
    void breakStrap(Elf& elf);
    void checkReins();
    void sleighSmash();
    void skiSmash();
    void stemSmash(bool sound);
    void stopFlight();
    void updateMeter();
    void startLoop(Sound*& sound, const std::string& name, b2Body* body, float fade);
    void stopLoop(Sound*& sound, float fade);
    Elf* elfOf(CharacterB2D* character);
    bool gameplay();

    // Flash constants
    float _wheelMaxSpeed;      // 30
    float _accelStep;          // 1.25 -> 0.625 per 60 Hz step
    float _impulseLeft;        // 1.3
    float _impulseRight;       // 1.7
    float _impulseOffset;      // 1
    float _maxSpinAV;          // 3.5
    float _sleighSmashLimit;   // 200
    float _boostMax;           // 50
    float _boostStep;          // 0.5 per Flash frame -> 0.25
    float _strapBreakImpulse;  // 0.4

    CharacterB2D* _santa;
    bool _santaEjected;
    float _ejectImpulse;
    Elf _elves[2];
    float _boostVal;
    bool _boosting;
    float _pumpCounter;
    int _frontContacts;
    int _backContacts;
    int _frame;

    b2Body* _sleighBody;
    b2Body* _reinsBody;
    b2Body* _rollers[4];       // back, mid1, mid2, front
    b2RevoluteJoint* _rollerJoints[4];
    b2PrismaticJoint* _reinsJoint;
    b2Fixture* _rearFixture;
    b2Fixture* _skiFixture;    // Flash skiShape, later baseShape
    b2Fixture* _backFixture;
    b2Fixture* _seatFixture;
    b2Fixture* _frontFixture;
    b2Fixture* _bumperFixture;
    b2Fixture* _stemFixtures[3];
    bool _sleighSmashed;
    bool _skiSmashed;
    bool _stemSmashed;
    float _santaWheelRadius;
    b2Vec2 _sprayStart;        // sleigh-local line the flight snow falls from
    b2Vec2 _sprayEnd;
    std::vector<b2Body*> _boxes;
    std::vector<b2Body*> _floating;   // Flash antiGravArray (besides the riders' own parts)

    Strap _headStrap;    // reins -> 7 nodes -> elf1 head -> 3 nodes -> elf2 head
    Strap _chestStrap1;  // shafts -> 2 nodes -> elf1 chest
    Strap _chestStrap2;  // shafts -> 4 nodes (bells) -> elf2 chest

    std::map<b2Fixture*, float> _results;
    std::vector<Painted> _painted;
    cocos2d::Sprite* _sleighSprite;
    cocos2d::Sprite* _stemSprite;
    cocos2d::Sprite* _skiSprite;
    cocos2d::DrawNode* _strapNode;

    Sound* _skiLoop;
    Sound* _bellLoop;
};
