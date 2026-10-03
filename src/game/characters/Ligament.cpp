#include "Ligament.h"

#include <cmath>

USING_NS_CC;

// @005e5788
Ligament* Ligament::create(float ptmRatio, float timeStep, b2Body* upperBody, b2Body* lowerBody,
                           float length)
{
    Ligament* ret = new (std::nothrow) Ligament();
    if (ret && ret->init(ptmRatio, timeStep, upperBody, lowerBody, length))
    {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

// @005e5860
bool Ligament::init(float ptmRatio, float timeStep, b2Body* upperBody, b2Body* lowerBody,
                    float length)
{
    _upperBody = upperBody;
    _lowerBody = lowerBody;
    setTimeStep(timeStep);
    // RE-TODO(@005e5890): create() ignores its argument; the original leaves 1/timeStep in s0 at
    // the call, which is what passing _timeStepInverse reproduces.
    create(_timeStepInverse);
    return true;
}

// @005e58a0
void Ligament::setTimeStep(float timeStep)
{
    _timeStepInverse = 1.0f / timeStep;
    float limit = (_timeStepInverse * 30.0f) / 60.0f;
    _breakLimitSquared = limit * limit;
}

// @005e58c8
void Ligament::create(float length)
{
    b2BodyDef bodyDef;

    b2CircleShape circle;
    circle.m_radius = 0.056f;

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &circle;
    fixtureDef.friction = 0.3f;
    fixtureDef.restitution = 0.1f;
    fixtureDef.density = 1.0f;
    fixtureDef.filter.categoryBits = 0x0008;
    fixtureDef.filter.maskBits = 0x0008;
    fixtureDef.filter.groupIndex = 0;

    b2PolygonShape* upperShape =
        static_cast<b2PolygonShape*>(_upperBody->GetFixtureList()->GetShape());
    b2Vec2 upperPoint = _upperBody->GetWorldPoint(b2Vec2(0.0f, upperShape->m_vertices[0].y));
    b2PolygonShape* lowerShape =
        static_cast<b2PolygonShape*>(_lowerBody->GetFixtureList()->GetShape());
    b2Vec2 lowerPoint = _lowerBody->GetWorldPoint(b2Vec2(0.0f, lowerShape->m_vertices[2].y));

    b2World* world = _upperBody->GetWorld();
    std::vector<b2Body*> bodies;
    // The original's y step is (lowerPoint.y - lowerPoint.y) / 3 = 0 (asm @005e59e4 moves
    // lowerPoint.y into the subtrahend before the fsub): both ligament bodies sit at upperPoint.y.
    // Positions are one fused multiply-add (fmla) per component.
    float stepX = (lowerPoint.x - upperPoint.x) / 3.0f;
    float stepY = (lowerPoint.y - lowerPoint.y) / 3.0f;
    for (unsigned int i = 1; i < 3; i++)
    {
        bodyDef.type = b2_dynamicBody;
        bodyDef.position.x = std::fma(stepX, (float)i, upperPoint.x);
        bodyDef.position.y = std::fma(stepY, (float)i, upperPoint.y);
        bodyDef.fixedRotation = true;
        b2Body* body = world->CreateBody(&bodyDef);
        body->CreateFixture(&fixtureDef);
        body->ResetMassData();
        body->SetLinearVelocity(_upperBody->GetLinearVelocity());
        bodies.push_back(body);
    }

    b2DistanceJointDef jointDef;

    jointDef.Initialize(_upperBody, bodies[0], upperPoint, bodies[0]->GetPosition());
    jointDef.collideConnected = false;
    jointDef.length = 0.064f;
    _mainJoint = static_cast<b2DistanceJoint*>(world->CreateJoint(&jointDef));
    _joints.push_back(_mainJoint);

    b2Body* middle = bodies[1];
    jointDef.Initialize(bodies[0], middle, bodies[0]->GetPosition(), middle->GetPosition());
    jointDef.length = 0.064f;
    _joints.push_back(static_cast<b2DistanceJoint*>(world->CreateJoint(&jointDef)));

    jointDef.Initialize(middle, _lowerBody, middle->GetPosition(), lowerPoint);
    jointDef.length = 0.064f;
    _joints.push_back(static_cast<b2DistanceJoint*>(world->CreateJoint(&jointDef)));
}

// @005e6190
void Ligament::checkJoints()
{
    if (_mainJoint &&
        _mainJoint->GetReactionForce(_timeStepInverse).LengthSquared() > _breakLimitSquared)
    {
        breakJoint();
    }
}

// @005e622c
void Ligament::breakJoint()
{
    _joints.erase(std::find(_joints.begin(), _joints.end(), _mainJoint));
    _lowerBody->GetWorld()->DestroyJoint(_mainJoint);
    _mainJoint = nullptr;
}

// @005e62a0
std::vector<b2DistanceJoint*> Ligament::getJoints()
{
    return _joints;
}
