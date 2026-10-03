#include "IntestineChain.h"

#include <cmath>

USING_NS_CC;

// @005c9f80
IntestineChain* IntestineChain::create(std::string tag, float ptmRatio, float timeStep,
                                       b2Body* chestBody, b2Body* pelvisBody, int totalIntestines,
                                       float intestineLength)
{
    IntestineChain* ret = new (std::nothrow) IntestineChain();
    if (ret && ret->initWithTag(tag, ptmRatio, timeStep, chestBody, pelvisBody, totalIntestines,
                                intestineLength))
    {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

// @005ca13c
bool IntestineChain::initWithTag(std::string tag, float ptmRatio, float timeStep,
                                 b2Body* chestBody, b2Body* pelvisBody, int totalIntestines,
                                 float intestineLength)
{
    _totalIntestines = totalIntestines;
    _intestineLength = intestineLength;
    _chestBody = chestBody;
    _pelvisBody = pelvisBody;
    _ptmRatio = ptmRatio;
    setTimeStep(timeStep);
    create(tag);
    return true;
}

// @005ca208
void IntestineChain::setTimeStep(float timeStep)
{
    _timeStepInverse = 1.0f / timeStep;
    float limit = (_timeStepInverse * 30.0f) / 60.0f;
    _breakLimitSquared = limit * limit;
}

// @005ca230
void IntestineChain::create(std::string tag)
{
    Node* chestSprite = static_cast<Node*>(_chestBody->GetUserData());
    b2World* world = _chestBody->GetWorld();
    Node* parent = chestSprite->getParent();
    int zOrder = chestSprite->getLocalZOrder();

    _intestineSprites.reserve(_totalIntestines + 5);

    // The original loop has no entry test: at least one sprite is always created.
    Sprite* sprite;
    unsigned int i = 0;
    do
    {
        sprite = Sprite::createWithSpriteFrameName(tag + "_intestine.png");
        parent->addChild(sprite, zOrder - 2);
        _intestineSprites.push_back(sprite);
        i++;
    } while (i < (unsigned int)_totalIntestines);
    _spriteWidth = sprite->getTextureRect().size.width;

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;

    b2CircleShape circle;
    circle.m_radius = 0.04f;

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &circle;
    fixtureDef.friction = 0.3f;
    fixtureDef.restitution = 0.1f;
    fixtureDef.density = 1.0f;
    fixtureDef.filter.categoryBits = 0x0004;
    fixtureDef.filter.maskBits = 0xffff;
    fixtureDef.filter.groupIndex = 0;

    std::vector<b2Body*> segments;

    b2PolygonShape* pelvisShape =
        static_cast<b2PolygonShape*>(_pelvisBody->GetFixtureList()->GetShape());
    b2Vec2 pelvisPoint = _pelvisBody->GetWorldPoint(b2Vec2(0.0f, pelvisShape->m_vertices[2].y));
    b2PolygonShape* chestShape =
        static_cast<b2PolygonShape*>(_chestBody->GetFixtureList()->GetShape());
    b2Vec2 chestPoint = _chestBody->GetWorldPoint(b2Vec2(0.0f, chestShape->m_vertices[0].y));

    float count = (float)_totalIntestines;
    for (int n = 1; n < _totalIntestines; n++)
    {
        bodyDef.position.x = pelvisPoint.x + ((chestPoint.x - pelvisPoint.x) / count) * (float)n;
        bodyDef.position.y = pelvisPoint.y + ((chestPoint.y - pelvisPoint.y) / count) * (float)n;
        bodyDef.fixedRotation = true;
        b2Body* segment = world->CreateBody(&bodyDef);
        segment->CreateFixture(&fixtureDef);
        segment->ResetMassData();
        segment->SetLinearVelocity(_chestBody->GetLinearVelocity());
        segments.push_back(segment);
    }

    float length = _intestineLength;
    b2DistanceJointDef jointDef;

    jointDef.Initialize(_pelvisBody, segments[0], pelvisPoint, segments[0]->GetPosition());
    jointDef.length = length;
    jointDef.collideConnected = false;
    _pelvisIntestineJoint = static_cast<b2DistanceJoint*>(world->CreateJoint(&jointDef));

    for (int n = 0; n < _totalIntestines - 2; n++)
    {
        jointDef.Initialize(segments[n], segments[n + 1], segments[n]->GetPosition(),
                            segments[n + 1]->GetPosition());
        jointDef.length = length;
        jointDef.collideConnected = false;
        _joints.push_back(static_cast<b2DistanceJoint*>(world->CreateJoint(&jointDef)));
    }

    b2Body* lastSegment = segments[_totalIntestines - 2];
    jointDef.Initialize(lastSegment, _chestBody, lastSegment->GetPosition(), chestPoint);
    jointDef.length = length;
    jointDef.collideConnected = false;
    _intestineChestJoint = static_cast<b2DistanceJoint*>(world->CreateJoint(&jointDef));
}

// @005cac34
void IntestineChain::paint()
{
    // End point of the previously drawn segment; the chest end continues from it.
    b2Vec2 lastPoint(0.0f, 0.0f);

    if (_pelvisIntestineJoint)
    {
        Sprite* sprite = _intestineSprites.front();
        b2Vec2 pointA = _pelvisIntestineJoint->GetAnchorA();
        lastPoint = _pelvisIntestineJoint->GetBodyB()->GetPosition();
        b2Vec2 delta = lastPoint - pointA;
        b2Vec2 middle = pointA + 0.5f * delta;
        sprite->setPosition(Vec2(middle.x * _ptmRatio, middle.y * _ptmRatio));
        float angle = atan2f(pointA.y - lastPoint.y, pointA.x - lastPoint.x);
        float length = sqrt(pow(delta.x, 2) + pow(delta.y, 2));
        sprite->setScaleX((_ptmRatio * length) / _spriteWidth);
        sprite->setRotation(angle * -57.2957802f);
    }
    for (size_t i = 0; i < _joints.size(); i++)
    {
        b2DistanceJoint* joint = _joints[i];
        b2Vec2 pointA = joint->GetBodyA()->GetPosition();
        lastPoint = joint->GetBodyB()->GetPosition();
        b2Vec2 delta = lastPoint - pointA;
        Sprite* sprite = _intestineSprites[i + 1];
        b2Vec2 middle = pointA + 0.5f * delta;
        sprite->setPosition(Vec2(middle.x * _ptmRatio, middle.y * _ptmRatio));
        sprite->setRotation(atan2f(pointA.y - lastPoint.y, pointA.x - lastPoint.x) * -57.2957802f);
        float length = sqrt(pow(delta.x, 2) + pow(delta.y, 2));
        sprite->setScaleX((_ptmRatio * length) / _spriteWidth);
    }
    if (_intestineChestJoint && (_pelvisIntestineJoint || _joints.size() > 0))
    {
        Sprite* sprite = _intestineSprites.back();
        b2Vec2 pointB = _intestineChestJoint->GetAnchorB();
        b2Vec2 delta = pointB - lastPoint;
        b2Vec2 middle = lastPoint + 0.5f * delta;
        sprite->setPosition(Vec2(_ptmRatio * middle.x, _ptmRatio * middle.y));
        sprite->setRotation(atan2f(lastPoint.y - pointB.y, lastPoint.x - pointB.x) * -57.2957802f);
        float length = sqrt(pow(delta.x, 2) + pow(delta.y, 2));
        sprite->setScaleX((_ptmRatio * length) / _spriteWidth);
    }
}

// @005caf68
void IntestineChain::intestineBreak1()
{
    if (_pelvisIntestineJoint)
    {
        b2World* world = _pelvisBody->GetWorld();
        _intestineSprites.front()->setVisible(false);
        world->DestroyJoint(_pelvisIntestineJoint);
        _pelvisIntestineJoint = nullptr;
    }
}

// @005cafbc
void IntestineChain::intestineBreak2()
{
    if (_intestineChestJoint)
    {
        b2World* world = _chestBody->GetWorld();
        _intestineSprites.back()->setVisible(false);
        world->DestroyJoint(_intestineChestJoint);
        _intestineChestJoint = nullptr;
    }
}

// @005cb010
void IntestineChain::checkJoints()
{
    if (_pelvisIntestineJoint &&
        _pelvisIntestineJoint->GetReactionForce(_timeStepInverse).LengthSquared() > _breakLimitSquared)
    {
        intestineBreak1();
    }
    if (_intestineChestJoint &&
        _intestineChestJoint->GetReactionForce(_timeStepInverse).LengthSquared() > _breakLimitSquared)
    {
        intestineBreak2();
    }
}

// @005cb0f4
IntestineChain::~IntestineChain()
{
    for (unsigned int i = 0; i < _intestineSprites.size(); i++)
    {
        _intestineSprites[i] = nullptr;
    }
    _intestineSprites.clear();
    _joints.clear();
}
