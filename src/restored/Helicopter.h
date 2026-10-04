#pragma once

// RESTORED (PC addition): Helicopter Man's helicopter (browser game character 11, Flash v1.87
// com.totaljerkface.game.character.HelicopterMan), which the mobile port left out. The Flash
// class's copter half lives here, the rider is HelicopterMan.
//
//   * one copter body (blade, stem, base, back, two legs with wheels; density 3, linear damping
//     0.4) holding the man by the seat and both hands; a magnet hangs from it on a rope joint;
//   * the copter balances itself towards a target angle (Flash balanceCopter: the angular velocity
//     is set to -sin(angle - target) * 3) and cancels gravity for the whole rider + copter + magnet
//     mass at their centre (Flash hoverCopter); up / down climb and sink (speed-limited at 10 m/s);
//     the lean buttons turn the target angle (Flash left / right);
//   * space toggles the magnet: it then holds anything that hits its underside inside its range
//     (a revolute joint limited to +-3 degrees) until it is turned off;
//   * shift / ctrl (control bits 0x20 / 0x40) reel the rope in / out (0.05 per Flash frame,
//     between its initial length and 3 m); the rope is drawn as a 20-segment verlet line;
//   * the blade pushes away whatever it touches (impulse 3 shared by inverse mass) with ricochet
//     sparks and sounds; a hit above 30 smashes it into four BladeShards that impale flesh on a
//     hard hit (Flash BladeShard: the shard turns into a sensor that pins every touched body part
//     with a prismatic joint); without the blade the copter falls and the lean buttons spin it;
//   * the back / base above 40 smashes the copter into five pieces (+ the blade and legs), a leg's
//     wheel above 40 breaks that leg off.
//
// Shapes come from vehicles/bodies/helicopter.plist, sprites from vehicles/helicopter_sprites.plist
// (generated from character11.swf by tools/assets/extract_character.py). Flash metres are these
// metres (Flash m_physScale 62.5 px per metre); per-frame impulses and steps are halved at 60 Hz
// (s_timeStepOverFlashTimeStep), smash limits as in Flash.

#include "Vehicle.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <map>
#include <set>
#include <string>
#include <vector>

class CharacterB2D;
class Sound;

class Helicopter : public Vehicle
{
public:
    static Helicopter* create(cocos2d::Vec2 position, std::string name, int groupID);

    Helicopter();
    ~Helicopter() override;

    bool init(cocos2d::Vec2 position, std::string name, int groupID) override;
    void createSprites() override;
    void createBodies() override;
    void createDictionaries() override;
    void addCharacter(CharacterB2D* character) override;
    void handleInjury(CharacterInjury injury, CharacterB2D* character) override;
    bool ejectCharacter(CharacterB2D* character) override;

    void forwardButtonPressed() override;   // up: climb
    void backButtonPressed() override;      // down: sink
    void forwardBackButtonsNull() override;
    void leanForwardButtonPressed() override;  // Flash right
    void leanBackButtonPressed() override;     // Flash left
    void leanButtonsNull() override;
    void special1ButtonPressed() override;     // magnet on / off
    void special1ButtonNull() override;
    void leanForwardPose() override;
    void leanBackPose() override;

    // Shift (0x20) reels the rope in, ctrl (0x40) out, while riding (HelicopterMan::setState).
    void extraControls(unsigned char state);
    bool riderOn() const { return _rider && !_riderEjected; }

    void actions() override;
    void paint() override;
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;
    void endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;
    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override;
    void jointWillBeDestroyed(b2Joint* joint) override;
    void fixtureWillBeDestroyed(b2Fixture* fixture) override;
    void debugFunction(int value) override;  // 0: smash the copter, 1: the blade

private:
    struct Painted
    {
        b2Body* body;
        cocos2d::Sprite* sprite;
        bool atCentre;
    };
    struct Hit
    {
        b2Fixture* other;
        b2Vec2 point;
        float impulse;
    };
    // Flash heli.BladeShard
    struct Shard
    {
        b2Body* body = nullptr;
        b2Fixture* fixture = nullptr;
        b2Fixture* sensor = nullptr;
        bool rightSide = false;
        float stabImpulse = 0.0f;
        bool stab = false;
        bool active = false;  // in Flash bladeActions
        std::vector<std::pair<b2Body*, b2Vec2>> toAdd;
        std::vector<b2Body*> toRemove;
        std::map<b2Body*, b2PrismaticJoint*> joints;
        std::map<b2Body*, int> counts;
        b2Body* previousBody = nullptr;
        Sound* fleshSound = nullptr;
        cocos2d::DrawNode* node = nullptr;
    };
    // Flash heli.VPoint
    struct RopePoint
    {
        b2Vec2 curr;
        b2Vec2 prev;
        bool fixed;
    };

    cocos2d::ValueMap shape(const std::string& name);
    b2Vec2 guide(const std::string& name);  // shapeGuide point, guide coordinates (body local)
    b2Fixture* addBox(b2Body* body, b2FixtureDef def, const std::string& name);
    b2Fixture* addPolygon(b2Body* body, b2FixtureDef def, const std::string& prefix, int count);
    cocos2d::Sprite* sprite(const std::string& frame, cocos2d::Node* parent, int z);
    void watch(b2Fixture* fixture);
    void unwatchAll();
    bool gameplay();
    void startLoop(Sound*& sound, const std::string& name, b2Body* body, float volume, float fade);
    void stopLoop(Sound*& sound, float fade);

    std::vector<b2Body*> comBodies();
    b2Vec2 centerOfMass(float* totalMass);
    void balanceCopter();
    void hoverCopter();
    void spin(bool right);
    void resizeRope();
    void stepRope();
    void handleBladeContacts();
    void handleMagnetContacts();
    void shardActions(Shard& shard);
    void createPrisJoint(Shard& shard, b2Body* body, b2Vec2 normal);
    void removeShardJoint(Shard& shard, b2Body* body);
    void checkZeroJoints(Shard& shard);
    void drawShard(Shard& shard);

    void copterSmash();
    void bladeSmash(b2Vec2 worldPoint);
    void legSmash(int leg, bool sound);
    b2Body* brokenPiece(int index);
    void eject();

    // Flash constants (per Flash frame values converted to 60 Hz steps where noted)
    float _impulseLeft;        // 1 (blade smashed: lean impulses)
    float _impulseRight;       // 1
    float _impulseOffset;      // 1
    float _maxSpinAV;          // 3.5
    float _copterSmashLimit;   // 40
    float _bladeSmashLimit;    // 30
    float _ejectImpulse;       // 2
    float _accelStep;          // 0.025 per frame^2 -> / 4 per step^2
    float _decelStep;          // 0.02 -> / 4
    float _maxStep;            // 0.1 per frame -> / 2 per step
    float _ropeMaxLength;      // 3
    float _ropeSpeed;          // 0.05 per frame -> / 2
    int _numRopeSegments;      // 20

    CharacterB2D* _rider;
    bool _riderEjected;
    int _hoverState;           // 1 up, -1 down, 0 hover
    bool _isLoud;
    float _targetAng;          // Flash sense (clockwise positive)
    float _spinAcceleration;   // Flash sense
    bool _copterSmashed;
    bool _bladeSmashed;
    bool _legOn[2];
    bool _magnetized;
    bool _spaceOff;
    int _soundDelay;
    int _soundDelayCount;
    bool _bladeImpactSound;
    int _frameCounter;
    // Rider parts no longer counted in the hover mass (Flash removeFromCOMArray) / added stumps.
    std::set<int> _comOff;
    std::set<int> _comOn;

    b2Body* _copterBody;
    b2Body* _magnetBody;
    b2Fixture* _bladeFixture;
    b2Fixture* _stemFixture;
    b2Fixture* _baseFixture;
    b2Fixture* _backFixture;
    b2Fixture* _legFixture[2];
    b2Fixture* _wheelFixture[2];
    b2Fixture* _magnetFixture;
    b2RopeJoint* _ropeJoint;
    float _ropeMinLength;
    b2Vec2 _bladeLocalCenter;
    b2Vec2 _copterAnchor;   // copter local
    b2Vec2 _magnetAnchor;   // magnet local
    b2Vec2 _magnetRange[4]; // magnet local (Flash magnetRangeSensor)

    std::vector<RopePoint> _ropePoints;
    std::vector<float> _ropeLengths;

    std::vector<Hit> _bladeHits;
    std::map<b2Fixture*, Hit> _magnetHits;
    float _bladeResult;      // strongest blade hit above the limit this step (0 = none)
    b2Vec2 _bladeResultPoint;
    bool _copterResult;
    bool _legResult[2];
    std::set<b2Fixture*> _watched;
    std::vector<Shard> _shards;

    cocos2d::Sprite* _copterSprite;
    cocos2d::Sprite* _propellerSprite;
    cocos2d::Sprite* _brokenPropellerSprite;
    cocos2d::Sprite* _frontSprite;
    cocos2d::Sprite* _legSprite[2];
    cocos2d::Sprite* _magnetSprite;
    cocos2d::DrawNode* _ropeNode;
    std::vector<Painted> _painted;

    Sound* _heliLoop;
    Sound* _magnetLoop;
};
