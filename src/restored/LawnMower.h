#pragma once

// RESTORED (PC addition): Lawnmower Man's ride-on mower from the browser game (Flash v1.87
// com.totaljerkface.game.character.LawnMowerMan), which the mobile port left out. Rebuilt as a
// mobile-style Vehicle: the Flash class's mower half (bodies, joints, controls, smashing and the
// blade that grinds whatever it catches) lives here, the rider is LawnMowerMan.
//
// Shapes come from vehicles/bodies/lawnmower.plist, sprites from vehicles/lawnmower_sprites.plist;
// both are generated from the browser game's character6.swf by tools/assets/extract_character.py
// (shapeGuide in metres: 125 symbol px per metre, y flipped). Constants follow the mobile port's own
// conversions of its browser vehicles (see MotorCart vs Flash MotorCart): motor speeds as in
// Flash, accelStep and per-frame impulses x s_timeStepOverFlashTimeStep, Flash px / 62.5, smash
// limits as in Flash.

#include "Vehicle.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <map>
#include <string>
#include <vector>

class CharacterB2D;
class Sound;

class LawnMower : public Vehicle
{
public:
    static LawnMower* create(cocos2d::Vec2 position, std::string name, int groupID);

    LawnMower();
    ~LawnMower() override;

    bool init(cocos2d::Vec2 position, std::string name, int groupID) override;
    void lockWheels() override;
    void createSprites() override;
    void createBodies() override;
    void createJoints() override;
    void createDictionaries() override;
    void addCharacter(CharacterB2D* character) override;
    void handleInjury(CharacterInjury injury, CharacterB2D* character) override;
    void checkStateOfCharacter(CharacterB2D* character) override;
    bool ejectCharacter(CharacterB2D* character) override;

    void forwardButtonPressed() override;
    void backButtonPressed() override;
    void forwardBackButtonsNull() override;
    void leanForwardButtonPressed() override;
    void leanBackButtonPressed() override;
    void special1ButtonPressed() override;
    void special1ButtonNull() override;
    void leanBackPose() override;
    void leanForwardPose() override;

    void actions() override;
    void paint() override;
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;
    void endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;
    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override;
    void handleContactResults() override;
    void debugFunction(int value) override;  // smashes the mower

private:
    struct BladeContact
    {
        b2Fixture* fixture;
        float impulse;
        b2Vec2 point;
    };
    // A body the blade has caught: pulled up into the deck by a sensor "riser" on a prismatic
    // joint until it has left the clearance sensor, then finished off (Flash targetBodies /
    // risingBodies).
    struct Target
    {
        b2Body* body;
        b2Body* riser;
        CharacterB2D* owner;
        float massRatio;
        float leftX;   // spray range on the blade (mower local x)
        float rightX;
    };
    struct Piece
    {
        b2Body* body;
        cocos2d::Sprite* sprite;
    };

    cocos2d::ValueMap shape(const std::string& name);
    cocos2d::Vec2 point(const std::string& name);  // shapeGuide point (local, mobile metres)
    b2Fixture* addPolygon(b2Body* body, b2FixtureDef def, const std::string& prefix, int first,
                          int count);
    b2Fixture* addBox(b2Body* body, b2FixtureDef def, const std::string& name);
    CharacterB2D* ownerOf(b2Body* body);
    void handleBladeContacts();
    void finishTarget(const Target& target);
    void mowerSmash();
    void frontSmash();
    void rearSmash();
    b2Body* makePiece(b2Body* from, const std::vector<std::vector<b2Vec2>>& polygons,
                      const std::string& sprite);
    void shardBurst(b2Vec2 worldPoint, int count, cocos2d::Color4B color);
    void startLoop(Sound*& sound, const std::string& name, float volume, float fade);
    void fadeLoop(Sound*& sound, float volume, float time, bool stop);
    bool gameplay();

    // Flash constants (LawnMowerMan.as).
    float _wheelSpeedRatio;       // front wheel motor = 1.76 x back
    float _impulseLeft;           // 2.4  lean back
    float _impulseRight;          // 2.8  lean forward
    float _impulseOffset;         // 1
    float _maxSpinAV;             // 3.5
    float _mowerSmashLimit;       // 200 (Flash > 1.4)
    float _frontRearSmashLimit;   // 150
    float _ejectImpulse;          // 5
    float _verticalTranslation;   // 20 / m_physScale Flash m -> x 0.48

    b2Body* _mowerBody;
    b2Body* _frontShockBody;
    b2Body* _backShockBody;
    b2Body* _frontWheelBody;
    b2Body* _backWheelBody;
    b2Body* _frontBody;           // after mowerSmash
    b2Body* _rearBody;
    b2Fixture* _handleFixture;
    b2Fixture* _shaftFixture;
    b2Fixture* _frontFixture;
    b2Fixture* _baseFixture;
    b2Fixture* _bladeFixture;
    b2Fixture* _rearFixture;
    b2Fixture* _topFixture;
    b2Fixture* _seatFixture;
    b2Fixture* _clearanceFixture;
    b2Fixture* _brokenFrontFixture;
    b2Fixture* _brokenRearFixture;
    std::vector<b2Fixture*> _rearContactFixtures;
    b2PrismaticJoint* _frontShockJoint;
    b2PrismaticJoint* _backShockJoint;
    b2RevoluteJoint* _frontWheelJoint;
    b2RevoluteJoint* _backWheelJoint;
    CharacterB2D* _rider;

    cocos2d::Sprite* _mowerSprite;
    cocos2d::Sprite* _bladeCoverSprite;
    cocos2d::Sprite* _frontWheelSprite;
    cocos2d::Sprite* _backWheelSprite;
    cocos2d::DrawNode* _shockNode;
    std::vector<Piece> _pieces;   // painted at their mass centre, like Flash paint()

    Sound* _mowerLoop;
    Sound* _grindLoop;
    int _soundDelay;
    int _soundDelayCount;
    bool _impactSoundPlaying;
    bool _mowerSmashed;
    int _frameCounter;

    b2Vec2 _bladeCenter;          // mower local
    float _bladeHalfWidth;
    float _bladeBottom;           // mower local y of the blade's lower edge
    float _mowerMass;
    std::vector<BladeContact> _bladeContacts;
    std::vector<Target> _targets;
    std::vector<Target> _addedTargets;
    std::map<b2Body*, int> _contactCount;
    std::vector<b2Body*> _groundBodies;  // finished: inactive, sprite kept hidden
    float _frontBreakImpulse;
    float _rearBreakImpulse;
};
