#pragma once
// ONLINE (PC addition): browser special 18, Glass (Flash userspecials/Glass, GlassShard,
// GlassShard2 + editor GlassRef). The Android game has no glass.
//
// XML (GlassRef._attributes): p0 x, p1 y, p2 shapeWidth 5..50, p3 shapeHeight 50..500, p4 angle,
// p5 sleeping, p6 shatterStrength 1..10, p7 stabbing.
//
// A translucent dynamic pane (density 2). A contact impulse above mass x shatterStrength (or the
// "shatter" trigger action, aimed from the trigger's position) shatters it at the impact point
// into up to 8 triangular shards (glass burst, Glass<Light|Mid|Heavy> sound). Shards shatter
// again on hard hits; with "stabbing" (levels >= 1.36; older levels always stab) a character
// part hitting a shard is impaled on it (prismatic joint, blood burst and blood painted on the
// shard, fatal for heavy shards). Trigger actions: 0 shatter, 1 wake, 2 apply impulse.

#include <map>
#include <vector>

#include "online/items/FlashSpecials.h"
#include "online/items/MiscItemUtil.h"

namespace cocos2d {
class ClippingNode;
class DrawNode;
}

namespace online {

class GlassShard;

class Glass : public FlashItem
{
public:
    ~Glass() override;
    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;
    void singleAction() override;
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;
    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override;
    b2Body* getJointBody(b2Vec2 point) override { return _body; }
    void triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties) override;
    std::vector<b2Body*> getBodyList() override;

private:
    void shatter();
    void removeListeners();

    b2Body* _body = nullptr;
    b2Fixture* _shape = nullptr;
    cocos2d::Node* _mc = nullptr;  // retained
    float _halfW = 0.0f;           // metres (Flash leftX = -halfW, topY = -halfH)
    float _halfH = 0.0f;
    bool _stabbing = true;
    int _smashImpulse = 0;
    int _glassParticles = 100;
    std::string _soundSuffix;
    bool _hasImpact = false;
    b2Vec2 _impact;  // Flash body-local (y down), metres
    bool _queued = false;
    std::vector<GlassShard*> _shards;  // retained
};

// One triangle of a shattered pane (Flash GlassShard / GlassShard2).
class GlassShard : public LevelItem
{
public:
    GlassShard(b2Body* body, cocos2d::Node* root, const std::vector<cocos2d::Vec2>& triPoints,
               bool version2, bool stabbing);
    ~GlassShard() override;
    void actions() override;
    void singleAction() override;
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;
    void endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;
    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override;
    void jointWillBeDestroyed(b2Joint* joint) override;
    void fixtureWillBeDestroyed(b2Fixture* fixture) override;

private:
    void createPrisJoint(b2Body* other, b2Vec2 normal);
    void removeJoint(b2Body* other);
    void checkZeroJoints();
    void paintBlood(const b2Vec2& worldPoint);
    void forget(b2Body* other);

    b2Body* _body;
    b2Fixture* _shape;
    b2Fixture* _sensor = nullptr;
    cocos2d::Node* _root;  // retained; body userData
    cocos2d::ClippingNode* _bloodClip = nullptr;
    std::vector<cocos2d::Vec2> _tri;  // triangle in root-local points
    bool _v2;
    bool _stabbing;
    float _shatterImpulse = 0.0f;
    float _stabImpulse = 0.0f;
    std::string _soundSuffix;
    int _glassParticles = 50;
    int _bloodParticles = 50;
    bool _fatal = false;
    bool _stab = false;
    bool _inSAA = false;
    bool _resultListening = true;
    bool _dead = false;
    bool _inActions = false;
    b2Body* _previousBody = nullptr;
    std::vector<std::pair<b2Body*, b2Vec2>> _bodiesToAdd;
    std::vector<b2Body*> _bodiesToRemove;
    std::map<b2Body*, b2PrismaticJoint*> _bj;
    std::map<b2Body*, int> _count;
    std::map<b2Body*, b2Vec2> _lastNormal;  // from the solid shape's contacts (sensor contacts have none)
    int _fleshSoundFrames = 0;              // Flash: no new impale sound while one plays
    misc::FlashClock _clock;
};

}  // namespace online
