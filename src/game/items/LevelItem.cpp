#include "LevelItem.h"
#include "online/FlashRuntime.h"  // ONLINE (PC addition)

#include <cmath>
#include <sstream>

#include "cocos2d.h"

#include "ContactListener.h"
#include "LevelB2D.h"
#include "Session.h"
#include "Settings.h"
#include "SoundController.h"

USING_NS_CC;

// Global physics step shared by every level item (.data @00abb62c..@00abb63c, in this order).
// LevelItem::setTimeStep (called by LevelB2D::setTimeStep) keeps them in sync.
float LevelItem::s_timeStep = 1.0f / 60.0f;              // @00abb62c
float LevelItem::s_timeStepOverFlashTimeStep = 0.5f;     // @00abb630
float LevelItem::s_previousTimeStep = 1.0f / 60.0f;      // @00abb634
float LevelItem::s_timeStepInverse = 60.0f;              // @00abb638
float LevelItem::s_flashPtmRatio = 62.5f;                // @00abb63c

// @005dcd54
LevelItem::LevelItem()
    : _index(-1)
    , _triggered(false)
    , _specialType(SpecialTypeNone)
{
}

// @005dcdc8 (complete), @005dce28 (deleting)
LevelItem::~LevelItem()
{
}

// @005dce4c
int LevelItem::getIndex()
{
    return _index;
}

// @005dce54
void LevelItem::setSpecialType(SpecialType type)
{
    _specialType = type;
}

// @005dce5c
SpecialType LevelItem::getSpecialType()
{
    return _specialType;
}

// @005dce64
void LevelItem::setIndex(int index)
{
    _index = index;
}

// @005dce6c
bool LevelItem::init(LevelDataElement* element, b2Body* levelBody, b2Vec2 offset)
{
    return true;
}

// @005dce74
void LevelItem::loadSpriteFrames(LevelItemTextureId textureId)
{
    // Function-local static table (.bss @00ac62b8, guard @00ac62e8, destroyed at exit by the
    // atexit thunk @005dcf70).
    static std::string plistFiles[2] = {
        "level_items/level_items.plist",
        "level_items/mineExplosion.plist",
    };

    SpriteFrameCache* cache = SpriteFrameCache::getInstance();
    if (!cache->isSpriteFramesWithFileLoaded(plistFiles[textureId]))
    {
        cache->addSpriteFramesWithFile(plistFiles[textureId]);
    }
}

// @005dcfb4
void LevelItem::loadSpriteFrames(std::string plistFile)
{
    SpriteFrameCache* cache = SpriteFrameCache::getInstance();
    if (!cache->isSpriteFramesWithFileLoaded(plistFile))
    {
        cache->addSpriteFramesWithFile(plistFile);
    }
}

// @005dcff8
Node* LevelItem::getGameplayContainer()
{
    return Settings::getInstance()->getCurrentSession()->getGameplayContainer();
}

// @005dd010
Node* LevelItem::getLevelItemsNode()
{
    return Settings::getInstance()->getCurrentSession()->getLevelItemsNode();
}

// @005dd028
Session* LevelItem::getSession()
{
    return Settings::getInstance()->getCurrentSession();
}

// @005dd03c
b2Body* LevelItem::getLevelBody()
{
    return Settings::getInstance()->getCurrentSession()->getLevelBody();
}

// @005dd054
LevelB2D* LevelItem::getLevel()
{
    return Settings::getInstance()->getCurrentSession()->getLevel();
}

// @005dd06c
b2World* LevelItem::getWorld()
{
    return Settings::getInstance()->getCurrentSession()->getWorld();
}

// @005dd084
float LevelItem::getPtm()
{
    return Settings::getInstance()->getCurrentSession()->getPtmRatio();
}

// @005dd09c
void LevelItem::stopSoundsForBody(b2Body* body)
{
    Settings::getInstance()->getSoundController()->stopSoundsForBody(body);
}

// @005dd0c4
Sound* LevelItem::createBodySound(std::string soundName, b2Body* body, float volume, bool loop)
{
    return Settings::getInstance()->getSoundController()->createBodySound(soundName, body, volume,
                                                                          loop);
}

// @005dd1a0
Sound* LevelItem::createPositionSound(std::string soundName, Vec2 position, float volume,
                                      bool loop)
{
    return Settings::getInstance()->getSoundController()->createPositionSound(soundName, position,
                                                                              volume, loop);
}

// @005dd284
void LevelItem::setTimeStep(float timeStep)
{
    s_previousTimeStep = s_timeStep;
    s_timeStep = timeStep;
    s_timeStepInverse = 1.0f / timeStep;
    // The Flash original stepped at 30 fps.
    s_timeStepOverFlashTimeStep = timeStep / 0.0333333351f;
}

// @005dd2d4
float LevelItem::getTimeStep()
{
    return s_timeStep;
}

// @005dd2e4
float LevelItem::getTimeStepOverFlashTimeStep()
{
    return s_timeStepOverFlashTimeStep;
}

// @005dd2f4
float LevelItem::getPreviousTimeStep()
{
    return s_previousTimeStep;
}

// @005dd304
float LevelItem::getTimeStepInverse()
{
    return s_timeStepInverse;
}

// @005dd314
void LevelItem::addToBeginContact(b2Fixture* fixture)
{
    Settings::getInstance()->getCurrentSession()->getContactListener()->addBeginContactListener(
        fixture, this);
}

// @005dd348
void LevelItem::addToEndContact(b2Fixture* fixture)
{
    Settings::getInstance()->getCurrentSession()->getContactListener()->addEndContactListener(
        fixture, this);
}

// @005dd37c
void LevelItem::addToPreSolve(b2Fixture* fixture)
{
    Settings::getInstance()->getCurrentSession()->getContactListener()->addPreSolveListener(fixture,
                                                                                            this);
}

// @005dd3b0
void LevelItem::addToPostSolve(b2Fixture* fixture)
{
    Settings::getInstance()->getCurrentSession()->getContactListener()->addPostSolveListener(
        fixture, this);
}

// @005dd3e4
void LevelItem::removeFromActions()
{
    Settings::getInstance()->getCurrentSession()->getLevel()->removeFromActions(this);
}

// @005dd410
void LevelItem::removeFromSingleAction()
{
    Settings::getInstance()->getCurrentSession()->getLevel()->removeFromSingleActions(this);
}

// @005dd43c
void LevelItem::removeBeginContact(b2Fixture* fixture)
{
    Settings::getInstance()->getCurrentSession()->getContactListener()->removeBeginContactListener(
        fixture, this);
}

// @005dd470
void LevelItem::removeEndContact(b2Fixture* fixture)
{
    Settings::getInstance()->getCurrentSession()->getContactListener()->removeEndContactListener(
        fixture, this);
}

// @005dd4a4
void LevelItem::removePreSolve(b2Fixture* fixture)
{
    Settings::getInstance()->getCurrentSession()->getContactListener()->removePreSolveListener(
        fixture, this);
}

// @005dd4d8
void LevelItem::removePostSolve(b2Fixture* fixture)
{
    Settings::getInstance()->getCurrentSession()->getContactListener()->removePostSolveListener(
        fixture, this);
}

// @005dd50c
void LevelItem::fixtureWillBeDestroyed(b2Fixture* fixture)
{
    // Only the pending hit-sound buffer is cleaned; the other fixture maps keep stale keys.
    _contactAddBufferDict.erase(fixture);
}

// @005dd5d0
void LevelItem::contactSoundHandler(b2Fixture* fixture, b2Fixture* otherFixture,
                                    b2Contact* contact, b2Fixture* unused)
{
    // One hit sound per fixture and step.
    if (_contactAddBufferDict.find(fixture) != _contactAddBufferDict.end())
    {
        return;
    }
    if (otherFixture->IsSensor())
    {
        return;
    }
    // Lighter (non-static) bodies do not make this fixture's body sound.
    b2Body* otherBody = otherFixture->GetBody();
    if (otherBody->GetMass() != 0.0f && otherBody->GetMass() < fixture->GetBody()->GetMass())
    {
        return;
    }

    // Strongest relative velocity over the manifold points. Note: the manifold's *local* point is
    // transformed by each body's own transform (as in the original).
    b2Manifold* manifold = contact->GetManifold();
    b2Vec2 maxVelocity(0.0f, 0.0f);
    for (int i = 0; i < manifold->pointCount; i++)
    {
        b2Vec2 localPoint = manifold->points[i].localPoint;
        b2Vec2 velocity = otherBody->GetLinearVelocityFromLocalPoint(localPoint) -
                          fixture->GetBody()->GetLinearVelocityFromLocalPoint(localPoint);
        if (velocity.LengthSquared() > maxVelocity.LengthSquared())
        {
            maxVelocity = velocity;
        }
    }

    float speed = fabsf(b2Dot(manifold->localNormal, maxVelocity));
    if (speed > 4.0f)
    {
        _contactAddBufferDict[fixture] = speed;
    }
}

// @005dd814
void LevelItem::handleContactAdds()
{
    for (auto it = _contactAddBufferDict.begin(); it != _contactAddBufferDict.end(); ++it)
    {
        b2Fixture* fixture = it->first;
        // operator[] inserts an empty entry for fixtures without a hit sound (sic).
        std::string sound = _contactAddSounds[fixture];
        if (sound.length() != 0)
        {
            createBodySound(sound, fixture->GetBody(), 1.0f, false);
        }
    }
    _contactAddBufferDict.clear();
}

// @005dda24
void LevelItem::stopInteractivity()
{
}

// @005dda28
b2Body* LevelItem::createBody(const ValueMap* bodyData, Vec2 position)
{
    b2World* world = getWorld();

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.angularDamping = 1.0f;
    bodyDef.allowSleep = false;
    if (online::flashLevel()) {
        // ONLINE (PC addition): Flash bodies are undamped and may sleep.
        bodyDef.angularDamping = 0.0f;
        bodyDef.allowSleep = true;
    }
    bodyDef.angle = bodyData->at("rot").asFloat();
    Vec2 offset = PointFromString(bodyData->at("pos").asString());
    bodyDef.position.Set(position.x + offset.x, position.y + offset.y);
    return world->CreateBody(&bodyDef);
}

// @005ddc1c
b2Fixture* LevelItem::createFixture(b2Body* body, b2Filter filter, const ValueMap* fixtureData)
{
    b2FixtureDef fixtureDef;
    fixtureDef.friction = 0.3f;
    fixtureDef.restitution = 0.1f;
    fixtureDef.density = 1.0f;
    fixtureDef.filter = filter;
    return createFixture(body, fixtureDef, fixtureData, false, false);
}

// @005ddc94
b2Fixture* LevelItem::createFixture(b2Body* body, b2FixtureDef fixtureDef,
                                    const ValueMap* fixtureData, bool usePosition,
                                    bool useRotation)
{
    if (fixtureData->find("radius") != fixtureData->end())
    {
        b2CircleShape circle;
        circle.m_radius = fixtureData->at("radius").asFloat();
        if (usePosition)
        {
            Vec2 position = PointFromString(fixtureData->at("pos").asString());
            circle.m_p.Set(position.x, position.y);
        }
        fixtureDef.shape = &circle;
        return body->CreateFixture(&fixtureDef);
    }
    else if (fixtureData->find("size") != fixtureData->end())
    {
        // "size" holds the half extents (passed to SetAsBox unchanged).
        Size size = SizeFromString(fixtureData->at("size").asString());
        b2PolygonShape box;
        float angle = 0.0f;
        if (useRotation)
        {
            angle = fixtureData->at("rot").asFloat();
        }
        b2Vec2 center = b2Vec2_zero;
        if (usePosition)
        {
            Vec2 position = PointFromString(fixtureData->at("pos").asString());
            center.Set(position.x, position.y);
        }
        box.SetAsBox(size.width, size.height, center, angle);
        fixtureDef.shape = &box;
        return body->CreateFixture(&fixtureDef);
    }
    else if (fixtureData->find("verts") != fixtureData->end())
    {
        // "x,y:x,y:..." polygon. No check against b2_maxPolygonVertices (as in the original).
        std::string vertsString = fixtureData->at("verts").asString();
        std::vector<std::string> points;
        split(vertsString, ':', points);
        unsigned int vertexCount = (unsigned int)points.size();
        b2Vec2 vertices[b2_maxPolygonVertices];
        for (unsigned int i = 0; i < vertexCount; i++)
        {
            Vec2 point = PointFromString(points[i]);
            vertices[i].Set(point.x, point.y);
        }
        b2PolygonShape polygon;
        polygon.Set(vertices, (int)vertexCount);
        fixtureDef.shape = &polygon;
        return body->CreateFixture(&fixtureDef);
    }
    return nullptr;
}

// @005de328
void LevelItem::split(const std::string& string, char delimiter,
                      std::vector<std::string>& elements)
{
    std::stringstream stream(string);
    std::string item;
    while (std::getline(stream, item, delimiter))
    {
        elements.push_back(item);
    }
}

// @005de568
b2RevoluteJoint* LevelItem::createJoint(b2Body* bodyA, b2Body* bodyB, float upperAngle,
                                        float lowerAngle, Vec2 anchor, Vec2 offset)
{
    b2World* world = getWorld();

    b2RevoluteJointDef jointDef;
    jointDef.enableLimit = true;
    jointDef.maxMotorTorque = 4.0f;
    // Limits are given in degrees relative to the bodies' current angles (read before
    // Initialize(), which stores the same difference as referenceAngle). The degree conversion
    // and the subtraction are separate statements: the original rounds the product (fmul, fsub;
    // no fused multiply-subtract).
    float lowerRadians = lowerAngle * 0.0174532924f;
    float upperRadians = upperAngle * 0.0174532924f;
    float angleDifference = bodyB->GetAngle() - bodyA->GetAngle();
    jointDef.Initialize(bodyA, bodyB, b2Vec2(anchor.x + offset.x, anchor.y + offset.y));
    jointDef.lowerAngle = lowerRadians - angleDifference;
    jointDef.upperAngle = upperRadians - angleDifference;
    return (b2RevoluteJoint*)world->CreateJoint(&jointDef);
}

// @005de694
void LevelItem::triggerSingleActivation(LevelItem* trigger, int action,
                                        std::vector<float> properties)
{
    if (_triggered)
    {
        return;
    }
    _triggered = true;
}

// @005de6ac
bool LevelItem::triggerRepeatActivation(LevelItem* trigger, int action,
                                        std::vector<float> properties, float time)
{
    return false;
}
