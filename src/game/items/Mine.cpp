#include "Mine.h"

#include <cmath>
#include <functional>
#include <string>

#include "ContactListener.h"
#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "Patch.h"
#include "QueryCallback.h"
#include "Session.h"
#include "online/FlashPhysics.h"  // ONLINE (PC addition)

USING_NS_CC;

// @005ee0b4 (D0; D1 is LevelItem's)
Mine::~Mine()
{
}

// @005ed34c
bool Mine::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    loadSpriteFrames(LevelItemTextureIdLevelItems);
    loadSpriteFrames(LevelItemTextureIdMineExplosion);
    _exploded = false;
    _counter = 8;

    float x = 0.0f;
    float y = 0.0f;
    float angle = 0.0f;
    element->floatAttribute("p0", &x);
    element->floatAttribute("p1", &y);
    element->floatAttribute("p2", &angle);
    getLevel()->convertPositionAndRotationData(&x, &y, &angle);

    _mc = Sprite::createWithSpriteFrameName("mine.png");
    createBody(b2Vec2(x, y), angle * -0.0174532924f);
    _mc->setPosition(Vec2(x * getPtm(), y * getPtm()));
    _mc->setRotation(angle);

    _light = Sprite::createWithSpriteFrameName("mine_light.png");
    _light->setAnchorPoint(Vec2(0.5f, 0.0f));
    const Size& mcSize = _mc->getTextureRect().size;
    _light->setPosition(Vec2(mcSize.width * 0.5f, mcSize.height * 0.7f));
    _mc->addChild(_light);
    getLevelItemsNode()->addChild(_mc);

    getLevel()->addToFrameActions(this);
    return true;
}

// @005ed660
void Mine::createBody(b2Vec2 position, float angle)
{
    b2PolygonShape shape;
    shape.SetAsBox(0.2f, 0.016f);

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &shape;
    fixtureDef.friction = 1.0f;
    fixtureDef.restitution = 0.1f;
    fixtureDef.density = 1.0f;
    fixtureDef.filter.categoryBits = 0x0008;
    fixtureDef.filter.maskBits = 0xFFFF;
    fixtureDef.filter.groupIndex = 0;

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = position;
    bodyDef.angle = angle;
    _body = getWorld()->CreateBody(&bodyDef);
    _body->SetUserData(_mc);
    _body->CreateFixture(&fixtureDef);
    _body->ResetMassData();

    // Small trigger button on top of the mine.
    shape.SetAsBox(0.008f, 0.004f, b2Vec2(0.0f, 0.04f), 0.0f);
    if (online::browserPhysicsWanted()) {
        shape.SetAsBox(0.016f, 0.008f, b2Vec2(0.0f, 0.04f), 0.0f);  // ONLINE (PC addition): the browser game's button
    }
    fixtureDef.shape = &shape;
    _sensor = _body->CreateFixture(&fixtureDef);
    getSession()->getContactListener()->addPostSolveListener(_sensor, this);
    getLevel()->addToPaintBody(_body);
}

// @005ed810
void Mine::frameAction()
{
    if (!_exploded && --_counter == 0) {
        _counter = 8;
        _counter = online::stepsFor60HzFrames(8);  // ONLINE (PC addition): 4 browser physics steps
        _light->setVisible(!_light->isVisible());
    }
}

// @005ed87c
void Mine::singleAction()
{
    if (_exploded) {
        return;
    }
    _exploded = true;

    getLevel()->removeFromPaintBody(_body);
    b2Vec2 center = _body->GetWorldCenter();
    float angle = _body->GetAngle();
    getWorld()->DestroyBody(_body);
    _body = nullptr;
    _mc->setVisible(false);
    blastBodies(center, 2.0f);

    Vector<SpriteFrame*> frames(40);
    SpriteFrameCache* cache = SpriteFrameCache::getInstance();
    for (int i = 1; i <= 40; i++) {
        frames.pushBack(cache->getSpriteFrameByName("mineExplosion_" + patch::to_string(i) + ".png"));
    }
    Animate* animate = Animate::create(Animation::createWithSpriteFrames(frames, 1.0f / 60.0f, 1));

    Node* parent = _mc->getParent();
    Vec2 position(center.x * getPtm(), center.y * getPtm());
    _explosion = Sprite::createWithSpriteFrameName("mineExplosion_1.png");
    _explosion->setScale(2.0f);
    _explosion->setAnchorPoint(Vec2(0.5f, 0.04f));
    _explosion->setPosition(position);
    _explosion->setRotation(angle * -57.29578f);
    parent->addChild(_explosion);
    _explosion->runAction(
        Sequence::create(animate, CallFunc::create(std::bind(&Mine::animationComplete, this)), nullptr));

    createPositionSound("MineExplosion", Vec2(center.x, center.y), 1.0f, false);
}

// @005edd84
void Mine::blastBodies(b2Vec2 center, float radius)
{
    QueryCallback callback;
    b2AABB aabb;
    aabb.lowerBound = b2Vec2(center.x - radius, center.y - radius);
    aabb.upperBound = b2Vec2(center.x + radius, center.y + radius);
    online::flashQueryAABB(getWorld(), &callback, aabb);  // ONLINE (PC addition)

    for (unsigned int i = 0; i < callback._fixtures.size(); i++) {
        b2Body* body = callback._fixtures[i]->GetBody();
        if (body->GetType() == b2_staticBody) {
            continue;
        }
        b2Vec2 bodyCenter = body->GetWorldCenter();
        b2Vec2 delta = bodyCenter - center;
        float blastAngle = atan2f(delta.y, delta.x);
        b2Vec2 direction(cosf(blastAngle), sinf(blastAngle));
        float falloff = 1.0f - fminf(radius, delta.Length()) / radius;
        b2Vec2 impulse = falloff * direction;
        impulse = 10.0f * impulse;
        body->ApplyLinearImpulse(impulse, bodyCenter, true);
    }
}

// @005edf80
void Mine::animationComplete()
{
    _explosion->removeFromParent();
    _explosion = nullptr;
}

// @005edfb0
void Mine::postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                     const b2ContactImpulse* impulse)
{
    getLevel()->removeFromActions(this);
    getLevel()->removeFromFrameActions(this);
    getSession()->getContactListener()->removePostSolveListener(_sensor, this);
    getLevel()->addToSingleActions(this);
}

// @005ee00c
void Mine::triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties)
{
    if (_exploded) {
        return;
    }
    getLevel()->removeFromSingleActions(this);
    getLevel()->removeFromFrameActions(this);
    removeBeginContact(_sensor);
    singleAction();
}

// @005ee070
std::vector<b2Body*> Mine::getBodyList()
{
    return std::vector<b2Body*>{_body};
}
