#pragma once

// LevelItem: base of every level object (items, characters, vehicles, triggers, target actions).
// Reconstructed from libMyGame.so 1.1.3 (arm64): cocos2d::Ref + own fields 0x28..0x94
// (sizeof 0x98, dsize 0x94 - derived classes may place a 4-byte field at 0x94).

#include <map>
#include <string>
#include <vector>

#include "Box2D/Box2D.h"
#include "base/CCRef.h"
#include "base/CCValue.h"
#include "math/Vec2.h"

namespace cocos2d {
class Node;
}

class LevelB2D;
class LevelDataElement;
class Session;
class Sound;

// LevelItem::_specialType (+0x90). Only these two values occur in the binary.
enum SpecialType
{
    SpecialTypeNone = -1,         // LevelItem::LevelItem()
    SpecialTypeCharacter = 5000,  // CharacterB2D::init; Trigger "triggered by any character"
};

// Index into the plist table of LevelItem::loadSpriteFrames(LevelItemTextureId).
enum LevelItemTextureId
{
    LevelItemTextureIdLevelItems = 0,     // "level_items/level_items.plist"
    LevelItemTextureIdMineExplosion = 1,  // "level_items/mineExplosion.plist"
};

// Mapped value of LevelItem::_contactResultBufferDict (sizeof 0x20): the strongest contact a
// fixture received during the last step (filled by the postSolve overrides of characters and
// vehicles). The iOS original's `Contact` is {b2Fixture*, b2Fixture*, b2Manifold*,
// b2ContactImpulse*, b2Vec2, float}; the port dropped the two pointers. Field names are ours.
struct LevelItemContact
{
    b2Fixture* fixture;       // +0x00
    b2Fixture* otherFixture;  // +0x08
    b2Vec2 position;          // +0x10 never written in 1.1.3 (value-initialised by map[])
    float impulse;            // +0x18
};

class LevelItem : public cocos2d::Ref
{
public:
    LevelItem();
    virtual ~LevelItem();

    // ---- virtual interface (declaration order == vtable order, slots vptr+0x10 .. +0x108) ----
    virtual bool init() { return true; }                                             // +0x010
    virtual bool init(LevelDataElement* element, b2Body* levelBody, b2Vec2 offset);  // +0x018
    virtual void die() {}                                                            // +0x020
    virtual void paint() {}                                                          // +0x028
    virtual void actions() {}                                                        // +0x030
    virtual void singleAction() {}                                                   // +0x038
    virtual void frameAction() {}                                                    // +0x040
    virtual void timeStepChanged() {}                                                // +0x048
    virtual b2Body* getJointBody(b2Vec2 point) { return nullptr; }                   // +0x050
    virtual void stopInteractivity();                                                // +0x058
    virtual void removeSprites() {}                                                  // +0x060
    virtual int getFluidType() { return 0; }                                         // +0x068
    virtual int shapeImpale(b2Fixture* fixture, bool impale, b2Vec2 point, float radius)
    {
        return 0;
    }                                                                                // +0x070
    virtual void explodeShape(b2Fixture* fixture, float force) {}                    // +0x078
    virtual void contactSoundHandler(b2Fixture* fixture, b2Fixture* otherFixture,
                                     b2Contact* contact, b2Fixture* unused);         // +0x080
    virtual void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) {}
                                                                                     // +0x088
    virtual void endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) {}
                                                                                     // +0x090
    virtual void preSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                          const b2Manifold* oldManifold) {}                          // +0x098
    virtual void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                           const b2ContactImpulse* impulse) {}                       // +0x0a0
    virtual void jointWillBeDestroyed(b2Joint* joint) {}                             // +0x0a8
    virtual void fixtureWillBeDestroyed(b2Fixture* fixture);                         // +0x0b0
    virtual void bodyWillBeDestroyed(b2Body* body) {}                                // +0x0b8
    virtual void handleContactAdds();                                                // +0x0c0
    virtual void paintWithOffsetPoints(cocos2d::Vec2 offset, float rotation) {}      // +0x0c8
                                                                    // rotation in degrees
    virtual void setOpacity(float opacity) {}                                        // +0x0d0
    virtual void triggerSingleActivation(LevelItem* trigger, int action,
                                         std::vector<float> properties);             // +0x0d8
    virtual bool triggerRepeatActivation(LevelItem* trigger, int action,
                                         std::vector<float> properties, float time); // +0x0e0
    virtual void prepareForTrigger() {}                                              // +0x0e8
    virtual std::vector<b2Body*> getBodyList() { return std::vector<b2Body*>(); }    // +0x0f0
    virtual void debugFunction(int value) {}                                         // +0x0f8
    virtual void setSpecialType(SpecialType type);                                   // +0x100
    virtual SpecialType getSpecialType();                                            // +0x108

    // ---- non-virtual API ----
    int getIndex();
    void setIndex(int index);

    void loadSpriteFrames(LevelItemTextureId textureId);
    void loadSpriteFrames(std::string plistFile);

    // Shortcuts into Settings::getInstance()->getCurrentSession().
    cocos2d::Node* getGameplayContainer();
    cocos2d::Node* getLevelItemsNode();
    Session* getSession();
    b2Body* getLevelBody();
    LevelB2D* getLevel();
    b2World* getWorld();
    float getPtm();

    // Shortcuts into Settings::getInstance()->getSoundController().
    void stopSoundsForBody(b2Body* body);
    Sound* createBodySound(std::string soundName, b2Body* body, float volume, bool loop);
    Sound* createPositionSound(std::string soundName, cocos2d::Vec2 position, float volume,
                               bool loop);

    // Global physics step shared by all items (written by LevelB2D::setTimeStep).
    static void setTimeStep(float timeStep);
    float getTimeStep();
    float getTimeStepOverFlashTimeStep();
    float getPreviousTimeStep();
    float getTimeStepInverse();

    // Registration with the session's ContactListener / the level's action lists.
    void addToBeginContact(b2Fixture* fixture);
    void addToEndContact(b2Fixture* fixture);
    void addToPreSolve(b2Fixture* fixture);
    void addToPostSolve(b2Fixture* fixture);
    void removeFromActions();
    void removeFromSingleAction();
    void removeBeginContact(b2Fixture* fixture);
    void removeEndContact(b2Fixture* fixture);
    void removePreSolve(b2Fixture* fixture);
    void removePostSolve(b2Fixture* fixture);

    // Body/fixture/joint factories driven by the plist body descriptions
    // (keys "rot", "pos", "radius", "size", "verts").
    b2Body* createBody(const cocos2d::ValueMap* bodyData, cocos2d::Vec2 position);
    b2Fixture* createFixture(b2Body* body, b2Filter filter, const cocos2d::ValueMap* fixtureData);
    b2Fixture* createFixture(b2Body* body, b2FixtureDef fixtureDef,
                             const cocos2d::ValueMap* fixtureData, bool usePosition,
                             bool useRotation);
    void split(const std::string& string, char delimiter, std::vector<std::string>& elements);
    b2RevoluteJoint* createJoint(b2Body* bodyA, b2Body* bodyB, float upperAngle, float lowerAngle,
                                 cocos2d::Vec2 anchor, cocos2d::Vec2 offset);

    static float s_timeStep;                   // 1/60
    static float s_previousTimeStep;           // 1/60
    static float s_timeStepInverse;            // 60
    static float s_timeStepOverFlashTimeStep;  // 0.5 (step / (1/30): the Flash game ran at 30 fps)
    static float s_flashPtmRatio;              // 62.5

protected:
    // Names of 0x28/0x40/0x8c/0x88 from the iOS original's LevelItem ivars, 0x58/0x70 from its
    // CharacterB2D (the port moved them up; CharacterB2D still has its own copies).
    std::map<b2Fixture*, float> _contactAddBufferDict;               // +0x28 fixture -> impact speed
    std::map<b2Fixture*, std::string> _contactAddSounds;             // +0x40 fixture -> hit sound
    std::map<b2Fixture*, LevelItemContact> _contactResultBufferDict; // +0x58
    std::map<b2Fixture*, float> _contactImpulseDict;                 // +0x70 fixture -> max impulse
    int _index;                                                      // +0x88 (-1)
    bool _triggered;                                                 // +0x8c triggerSingleActivation
    SpecialType _specialType;                                        // +0x90 (SpecialTypeNone)
};
