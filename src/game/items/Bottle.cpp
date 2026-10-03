#include "Bottle.h"

#include <cmath>
#include <cstdlib>

#include "BurstEmitter.h"
#include "EmitterNode.h"
#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "Patch.h"
#include "Session.h"

USING_NS_CC;

// @00588e50 (D0; the complete destructor is LevelItem's)
Bottle::~Bottle()
{
}

// @005884f0
bool Bottle::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    loadSpriteFrames(LevelItemTextureIdLevelItems);

    float y = 0.0f;
    float x = 0.0f;
    float rotation = 0.0f;
    element->floatAttribute("p0", &x);
    element->floatAttribute("p1", &y);
    element->floatAttribute("p2", &rotation);
    getLevel()->convertPositionAndRotationData(&x, &y, &rotation);
    bool sleeping = false;
    _colorId = 1;
    bool interactive = true;
    element->intAttribute("p3", &_colorId);
    element->boolAttribute("p4", &sleeping);
    element->boolAttribute("p5", &interactive);

    Sprite* sprite = Sprite::createWithSpriteFrameName("bottle_" + patch::to_string(_colorId) + ".png");
    sprite->setPosition(Vec2(x * getPtm(), y * getPtm()));
    sprite->setRotation(rotation);
    getLevelItemsNode()->addChild(sprite);

    if (interactive)
    {
        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        bodyDef.position.Set(x, y);
        bodyDef.angle = rotation * -0.0174532924f;
        bodyDef.awake = !sleeping;
        bodyDef.userData = sprite;

        b2PolygonShape shape;
        shape.SetAsBox(0.08f, 0.232f);

        b2FixtureDef fixtureDef;
        fixtureDef.shape = &shape;
        fixtureDef.friction = 0.3f;
        fixtureDef.restitution = 0.1f;
        fixtureDef.density = 2.0f;
        fixtureDef.filter.categoryBits = 8;

        _body = getWorld()->CreateBody(&bodyDef);
        addToPostSolve(_body->CreateFixture(&fixtureDef));
        getLevel()->addToPaintBody(_body);
    }
    return true;
}

// @00588a10
void Bottle::postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                       const b2ContactImpulse* impulse)
{
    float maxImpulse = impulse->normalImpulses[0];
    if (contact->GetManifold()->pointCount == 2)
    {
        maxImpulse = maxImpulse > impulse->normalImpulses[1] ? maxImpulse
                                                             : impulse->normalImpulses[1];
    }
    if (maxImpulse > 1.78f)
    {
        getLevel()->addToSingleActions(this);
    }
}

// @00588a74
void Bottle::singleAction()
{
    removePostSolve(_body->GetFixtureList());
    getLevel()->removeFromPaintBody(_body);
    ((Node*)_body->GetUserData())->removeFromParentAndCleanup(false);

    int index = (int)ceilf(CCRANDOM_0_1() * 2);
    createPositionSound("GlassLight" + patch::to_string(index),
                        Vec2(_body->GetPosition().x, _body->GetPosition().y), 1.0f, false);

    EmitterNode* particles = getSession()->getParticlesForeground();
    if (particles)
    {
        Color4B color(86, 131, 65, 204);
        if (_colorId == 2)
        {
            color = Color4B(102, 204, 255, 102);
        }
        else if (_colorId == 3)
        {
            color = Color4B(100, 13, 18, 114);
        }
        else if (_colorId == 4)
        {
            color = Color4B(240, 222, 83, 114);
        }
        Emitter* burst = BurstEmitter::createBottleBurst(_body, Color4F(color));
        if (burst)
        {
            particles->addChild(burst);
        }
    }
    getWorld()->DestroyBody(_body);
    _body = nullptr;
}

// @00588ca0
std::vector<b2Body*> Bottle::getBodyList()
{
    std::vector<b2Body*> bodies;
    if (_body)
    {
        bodies.push_back(_body);
    }
    return bodies;
}

// @00588ce4
b2Body* Bottle::getJointBody(b2Vec2 point)
{
    return _body;
}

// @00588cec
void Bottle::triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties)
{
    if (!_body)
    {
        return;
    }
    if (action == 1)
    {
        LevelB2D* level = getLevel();
        float impulseX = properties[0];
        float impulseY = properties[1];
        level->convertDirectionIfNecessaryBasedOnRegistration(&impulseY);
        _body->ApplyLinearImpulse(
            b2Vec2(impulseX * _body->GetMass(), impulseY * _body->GetMass()),
            _body->GetWorldCenter(), true);
        float spin = properties[2];
        level->convertDirectionIfNecessaryBasedOnRegistration(&spin);
        _body->SetAngularVelocity(_body->GetAngularVelocity() + spin);
    }
    else if (action == 0)
    {
        _body->SetAwake(true);
    }
}
