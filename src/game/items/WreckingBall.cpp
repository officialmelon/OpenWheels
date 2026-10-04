#include "online/FlashRuntime.h"  // ONLINE (PC addition)
#include "WreckingBall.h"

#include <string>

#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "LevelItemsDrawNode.h"
#include "Session.h"
#include "platform/compat/Box2DFloat.h"

USING_NS_CC;

// Inline in the original (only visible inlined into LevelB2D::addSpecial, case 7).
WreckingBall::WreckingBall()
    : _mc(nullptr),
      _ball(nullptr),
      _body(nullptr),
      _shape(nullptr),
      _joint(nullptr),
      _ballUpperAngle(0.0f),
      _ballLowerAngle(0.0f),
      _speed(0.0f),
      _chainPointA(0.0f, 0.0f),
      _chainPointBOffset(0.0f)
{
}

// @00648044 (D0; D1 is LevelItem's)
WreckingBall::~WreckingBall()
{
}

// @00647330
bool WreckingBall::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    loadSpriteFrames(LevelItemTextureIdLevelItems);
    float ptm = getPtm();

    float x = 0.0f;
    float y = 0.0f;
    float rotation = 0.0f;
    float length;  // not initialised in the original
    element->floatAttribute("p0", &x);
    element->floatAttribute("p1", &y);
    element->floatAttribute("p2", &length);
    getLevel()->convertPositionAndRotationData(&x, &y, &rotation);
    getLevel()->convertLengthData(&length);

    _speed = -28.0f / length;
    createBody(b2Vec2(x, y), length);
    createJoint(b2Vec2(x, y));

    Node* levelItemsNode = getLevelItemsNode();

    static float chainOverlap = ptm * 0.016f;
    static float anchorWidth = ptm * 0.576f;
    static float anchorHeight = ptm * 0.512f;

    Vec2 position(ptm * x, ptm * y);

    // Ceiling anchor: two mirrored halves.
    Sprite* anchor = Sprite::createWithSpriteFrameName("wreckingball_anchor.png");
    Vec2 anchorPosition = position + Vec2(anchorWidth * -0.25f, anchorHeight * 0.5f);
    anchorPosition.x = chainOverlap + anchorPosition.x;
    anchor->setPosition(anchorPosition);
    levelItemsNode->addChild(anchor);

    anchor = Sprite::createWithSpriteFrameName("wreckingball_anchor.png");
    anchorPosition = position + Vec2(anchorWidth * 0.25f, anchorHeight * 0.5f);
    anchorPosition.x = anchorPosition.x - chainOverlap;
    anchor->setPosition(anchorPosition);
    anchor->setScaleX(-1.0f);
    levelItemsNode->addChild(anchor);

    static float ballWidth = ptm * 2.4f;
    static float ballHeight = ptm * 0.4f;

    // Ball: a container (rotated in paint()) at the pivot, the two halves `length` below it.
    _ball = Node::create();
    _ball->setPosition(position);
    levelItemsNode->addChild(_ball);

    Sprite* ball = Sprite::createWithSpriteFrameName("wreckingball.png");
    Vec2 ballPosition = Vec2(ballWidth * -0.25f, ballHeight * 0.5f) - Vec2(0.0f, ptm * length);
    ballPosition.x = chainOverlap + ballPosition.x;
    ball->setPosition(ballPosition);
    _ball->addChild(ball);

    ball = Sprite::createWithSpriteFrameName("wreckingball.png");
    float ballX = ballWidth * 0.25f;
    ballPosition.x = ballX - chainOverlap;
    ball->setPosition(ballPosition);
    ball->setScaleX(-1.0f);
    _ball->addChild(ball);

    getLevel()->addToActions(this);
    getLevel()->addToPaintItem(this);
    return true;
}

// @00647a28
void WreckingBall::createBody(b2Vec2 position, float length)
{
    // The ball starts horizontally `length` to the right of the pivot.
    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = b2Vec2(position.x + length, position.y);
    bodyDef.angle = 1.57079637f;
    _body = getWorld()->CreateBody(&bodyDef);

    b2CircleShape circle;
    circle.m_radius = 1.2f;
    b2FixtureDef fixtureDef;
    fixtureDef.shape = &circle;
    fixtureDef.friction = 0.1f;
    fixtureDef.restitution = 0.1f;
    fixtureDef.density = 500.0f;
    fixtureDef.filter.categoryBits = 0x0008;
    fixtureDef.filter.maskBits = 0xFFFF;
    fixtureDef.filter.groupIndex = 0;
    _shape = _body->CreateFixture(&fixtureDef);

    // Chain shackle on top of the ball.
    b2PolygonShape box;
    box.SetAsBox(0.288f, 0.12f, b2Vec2(0.0f, 1.28f), 0.0f);
    fixtureDef.shape = &box;
    _body->CreateFixture(&fixtureDef);
    _body->ResetMassData();
    _chainPointBOffset = 1.52f;

    // Ceiling anchor block on the level body.
    box.SetAsBox(0.288f, 0.256f, b2Vec2(position.x, position.y + 0.256f), 0.0f);
    fixtureDef.shape = &box;
    getLevelBody()->CreateFixture(&fixtureDef);

    getSession()->getBackgroundDrawNode()->addWreckingBall(this);
}

// @00647c0c (also inlined into init)
void WreckingBall::createJoint(b2Vec2 anchor)
{
    b2RevoluteJointDef jointDef;
    _chainPointA.x = anchor.x * getPtm();
    _chainPointA.y = anchor.y * getPtm();
    jointDef.Initialize(getLevelBody(), _body, anchor);
    jointDef.lowerAngle = -3.92699099f;
    jointDef.upperAngle = 0.785398185f;
    jointDef.enableLimit = true;
    jointDef.enableMotor = false;
    _ballUpperAngle = 0.785398185f;
    _ballLowerAngle = -3.92699099f;
    jointDef.maxMotorTorque = 10000000.0f;
    _joint = static_cast<b2RevoluteJoint*>(getWorld()->CreateJoint(&jointDef));
}

// @00647d18
void WreckingBall::paint()
{
    _ball->setRotation(_body->GetAngle() * -57.29578f);
}

// @00647d3c
Vec2 WreckingBall::pointA()
{
    return _chainPointA;
}

// @00647d44
Vec2 WreckingBall::pointB()
{
    b2Vec2 point = _body->GetWorldPoint(b2Vec2(0.0f, _chainPointBOffset));
    return Vec2(point.x * getPtm(), point.y * getPtm());
}

// @00647db0 (non-virtual thunk @00647db8)
Vec2 WreckingBall::getPointA()
{
    return _chainPointA;
}

// @00647dc0 (non-virtual thunk @00647e2c)
Vec2 WreckingBall::getPointB()
{
    b2Vec2 point = _body->GetWorldPoint(b2Vec2(0.0f, _chainPointBOffset));
    return Vec2(point.x * getPtm(), point.y * getPtm());
}

// @00647e9c
void WreckingBall::actions()
{
    // Swing: the motor pushes the ball one way until it passes a turning angle, then coasts; on
    // the way back it re-engages with the opposite speed.
    float angle = owb2::jointAngle(_joint);
    if (angle > 0.3f) {
        if (_joint->IsMotorEnabled()) {
            _joint->EnableMotor(false);
            if (_speed > 0.0f) {
                _speed = -_speed;
            }
            _joint->SetMotorSpeed(_speed);
        }
    } else {
        bool motorEnabled = _joint->IsMotorEnabled();
        if (angle < -2.84f) {
            if (motorEnabled) {
                _joint->EnableMotor(false);
                if (_speed < 0.0f) {
                    _speed = -_speed;
                }
                _joint->SetMotorSpeed(_speed);
            }
        } else if (!motorEnabled) {
            _joint->EnableMotor(true);
            _joint->SetMotorSpeed(_speed);
            createBodySound("BallSwing", _body, 1.0f, false);
        }
    }
}

// ---------------------------------------------------------------------------------------------
// ONLINE (PC addition): trigger hooks of browser levels (Flash WreckingBall).

void WreckingBall::prepareForTrigger()
{
    if (!online::flashLevel())
    {
        LevelItem::prepareForTrigger();
        return;
    }
    // Flash: out of the actions, joint limits 0..0 (held where it starts), asleep, sensors.
    removeFromActions();
    _joint->SetLimits(0.0f, 0.0f);
    _joint->EnableMotor(false);
    for (b2Fixture* f = _body->GetFixtureList(); f; f = f->GetNext())
    {
        f->SetSensor(true);  // (wakes the body in Box2D 2.3: put it to sleep after)
    }
    _body->SetAwake(false);
}

void WreckingBall::triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties)
{
    if (!online::flashLevel())
    {
        LevelItem::triggerSingleActivation(trigger, action, properties);
        return;
    }
    if (_triggered)
    {
        return;
    }
    _triggered = true;
    _body->SetAwake(true);
    _joint->SetLimits(_ballLowerAngle, _ballUpperAngle);
    for (b2Fixture* f = _body->GetFixtureList(); f; f = f->GetNext())
    {
        f->SetSensor(false);
        f->Refilter();
    }
    getLevel()->addToActions(this);
}
