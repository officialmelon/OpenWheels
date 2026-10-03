#include "SpinalCord.h"

#include <cmath>

#include "platform/compat/Box2DFloat.h"

USING_NS_CC;

// @00630f10
SpinalCord* SpinalCord::create(std::string tag, float ptmRatio, float timeStep, b2Body* chestBody,
                               b2Vec2 chestAnchor, b2Body* headBody, int totalVertebrae,
                               float spineLength)
{
    SpinalCord* ret = new (std::nothrow) SpinalCord();
    if (ret && ret->initWithTag(tag, ptmRatio, timeStep, chestBody, chestAnchor, headBody,
                                totalVertebrae, spineLength))
    {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

// @006310e4
bool SpinalCord::initWithTag(std::string tag, float ptmRatio, float timeStep, b2Body* chestBody,
                             b2Vec2 chestAnchor, b2Body* headBody, int totalVertebrae,
                             float spineLength)
{
    _spineLength = spineLength;
    _totalVertebrae = totalVertebrae;
    _chestBody = chestBody;
    _headBody = headBody;
    _chestAnchor = chestAnchor;
    _ptmRatio = ptmRatio;
    setTimeStep(timeStep);
    create(tag);
    return true;
}

// @006311b4
void SpinalCord::setTimeStep(float timeStep)
{
    _timeStepInverse = 1.0f / timeStep;
    float limit = (_timeStepInverse * 30.0f) / 60.0f;
    _breakLimitSquared = limit * limit;
}

// @006311dc
void SpinalCord::create(std::string tag)
{
    Node* chestSprite = static_cast<Node*>(_chestBody->GetUserData());
    b2World* world = _chestBody->GetWorld();
    Node* parent = chestSprite->getParent();
    int zOrder = chestSprite->getLocalZOrder();

    for (unsigned int i = 0; i <= (unsigned int)_totalVertebrae; i++)
    {
        Sprite* sprite = Sprite::createWithSpriteFrameName(tag + "_spine.png");
        parent->addChild(sprite, zOrder - 2);
        _spineSprites.push_back(sprite);
    }

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

    std::vector<b2Body*> vertebrae;

    // GetWorldPoint with the original's fused rounding (fmul + fmla, see Box2DFloat.h).
    b2Vec2 chestPoint = owb2::worldPoint(_chestBody, _chestAnchor);
    float headRadius = _headBody->GetFixtureList()->GetShape()->m_radius;
    b2Vec2 headPoint = _headBody->GetWorldPoint(b2Vec2(0.0f, -headRadius));

    float segments = (float)(_totalVertebrae + 1);
    for (int i = 1; i <= _totalVertebrae; i++)
    {
        bodyDef.fixedRotation = true;
        // one fused multiply-add (fmla) per component in the original
        bodyDef.position.x = std::fma((chestPoint.x - headPoint.x) / segments, (float)i, headPoint.x);
        bodyDef.position.y = std::fma((chestPoint.y - headPoint.y) / segments, (float)i, headPoint.y);
        b2Body* vertebra = world->CreateBody(&bodyDef);
        vertebra->CreateFixture(&fixtureDef);
        vertebra->ResetMassData();
        vertebra->SetLinearVelocity(_chestBody->GetLinearVelocity());
        vertebrae.push_back(vertebra);
    }

    float length = _spineLength;
    b2DistanceJointDef jointDef;

    jointDef.Initialize(_headBody, vertebrae[0], headPoint, vertebrae[0]->GetPosition());
    jointDef.length = length;
    jointDef.collideConnected = false;
    _headToVertebraJoint = static_cast<b2DistanceJoint*>(world->CreateJoint(&jointDef));

    for (int i = 0; i < _totalVertebrae - 1; i++)
    {
        jointDef.Initialize(vertebrae[i], vertebrae[i + 1], vertebrae[i]->GetPosition(),
                            vertebrae[i + 1]->GetPosition());
        jointDef.length = length;
        jointDef.collideConnected = false;
        _joints.push_back(static_cast<b2DistanceJoint*>(world->CreateJoint(&jointDef)));
    }

    b2Body* lastVertebra = vertebrae[_totalVertebrae - 1];
    jointDef.Initialize(lastVertebra, _chestBody, lastVertebra->GetPosition(), chestPoint);
    jointDef.length = length;
    jointDef.collideConnected = false;
    _vertebraToChestJoint = static_cast<b2DistanceJoint*>(world->CreateJoint(&jointDef));
}

// @00631adc
void SpinalCord::paint()
{
    if (_headToVertebraJoint)
    {
        Sprite* sprite = _spineSprites.front();
        b2Vec2 anchorA = _headToVertebraJoint->GetAnchorA();
        b2Vec2 anchorB = _headToVertebraJoint->GetAnchorB();
        sprite->setPosition(Vec2(anchorA.x * _ptmRatio, anchorA.y * _ptmRatio));
        sprite->setRotation(atan2f(anchorA.y - anchorB.y, anchorA.x - anchorB.x) * -57.2957802f);
    }
    for (size_t i = 0; i < _joints.size(); i++)
    {
        b2DistanceJoint* joint = _joints[i];
        b2Vec2 positionA = joint->GetBodyA()->GetPosition();
        b2Vec2 positionB = joint->GetBodyB()->GetPosition();
        Sprite* sprite = _spineSprites[i + 1];
        sprite->setPosition(Vec2(positionA.x * _ptmRatio, positionA.y * _ptmRatio));
        sprite->setRotation(atan2f(positionA.y - positionB.y, positionA.x - positionB.x) *
                            -57.2957802f);
    }
    if (_vertebraToChestJoint)
    {
        Sprite* sprite = _spineSprites.back();
        b2Vec2 anchorA = _vertebraToChestJoint->GetAnchorA();
        b2Vec2 positionB = _vertebraToChestJoint->GetBodyB()->GetPosition();
        sprite->setPosition(Vec2(anchorA.x * _ptmRatio, anchorA.y * _ptmRatio));
        sprite->setRotation(atan2f(anchorA.y - positionB.y, anchorA.x - positionB.x) *
                            -57.2957802f);
    }
}

// @00631ce8
void SpinalCord::spineBreak1()
{
    if (_vertebraToChestJoint)
    {
        b2World* world = _headBody->GetWorld();
        _spineSprites.front()->setVisible(false);
        world->DestroyJoint(_vertebraToChestJoint);
        _vertebraToChestJoint = nullptr;
    }
}

// @00631d3c
void SpinalCord::spineBreak2()
{
    if (_headToVertebraJoint)
    {
        b2World* world = _headBody->GetWorld();
        _spineSprites.back()->setVisible(false);
        world->DestroyJoint(_headToVertebraJoint);
        _headToVertebraJoint = nullptr;
    }
}

// @00631d90
void SpinalCord::checkJoints()
{
    if (_vertebraToChestJoint &&
        _vertebraToChestJoint->GetReactionForce(_timeStepInverse).LengthSquared() > _breakLimitSquared)
    {
        spineBreak1();
    }
    if (_headToVertebraJoint &&
        _headToVertebraJoint->GetReactionForce(_timeStepInverse).LengthSquared() > _breakLimitSquared)
    {
        spineBreak2();
    }
}

// @00631e74
std::vector<b2DistanceJoint*> SpinalCord::getJoints()
{
    return _joints;
}
